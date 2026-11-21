#include "train_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <esp_system.h>

#include "config.h"
#include "log.h"
#include "train_protocol.h"

// ---- Timing ----
static const uint32_t SEARCH_TIMEOUT_MS      = 120000; // SEARCHING -> SLEEP
static const uint32_t RECONNECT_TIMEOUT_MS   = 60000;  // RECONNECTING -> SLEEP
static const uint32_t RESPONSE_TIMEOUT_MS    = 5000;   // WAIT_RESPONSE: first notification
static const uint32_t RETRY_PAUSE_MS         = 1000;   // before scanning again
static const uint32_t SCAN_CYCLE_MS          = 5000;
static const uint32_t CONNECT_TIMEOUT_MS     = 5000;   // NimBLE default is 30 s
static const uint8_t  CONNECT_RETRIES        = 1;
static const uint32_t STOP_BEFORE_DISCONNECT_MS = 300; // let the stop command go out first
static const uint32_t NEAREST_WINDOW_MS      = 1000;   // after the first train is seen, wait for closer ones

// ---- Event queue ----
static const size_t EVENT_QUEUE_LEN  = 32;
static const size_t LOW_PRIO_RESERVE = 8; // slots never given to scan results
static const size_t MAX_PAYLOAD      = 32;
static const size_t MAX_NAME         = 24;
static const size_t MAX_MANUFACTURER = 12;

// Matched only when the advertised service is missing.
static const char* const NAME_SUBSTRINGS[] = {"DUPLO", "Train", "LEGO"};
static const char* const FAKE_TARGET = "aa:bb:cc:dd:ee:ff";

static const NimBLEUUID SERVICE_UUID(lwp::SERVICE_UUID);
static const NimBLEUUID CHAR_UUID(lwp::CHAR_UUID);

enum class EventType : uint8_t { DEVICE, SCAN_END, LINK_UP, CONNECT_FAIL, DISCONNECT, NOTIFY, COMMAND };

struct Event {
  EventType  type;
  int        code;                  // reason code or command character
  ble_addr_t addr;                  // DEVICE
  int8_t     rssi;                  // DEVICE
  bool       advertisesLegoService; // DEVICE
  char       name[MAX_NAME];        // DEVICE
  uint8_t    length;                // payload length
  uint8_t    payload[MAX_PAYLOAD];  // NOTIFY: data; DEVICE: manufacturer data
  uint8_t    fullLength;            // length before truncation
};

// ---- Shared with loop() ----
static QueueHandle_t         g_events = nullptr;
static std::atomic<LinkState> g_state{LinkState::BOOT};
static std::atomic<uint32_t> g_stateSinceMs{0};
static std::atomic<uint32_t> g_droppedEvents{0};
static std::atomic<int>      g_desiredSpeed{0};
static std::atomic<bool>     g_forceSpeedSend{false};

// ---- Owned by the link task ----
static NimBLEClient* g_client = nullptr;
static NimBLERemoteCharacteristic* g_char = nullptr;
static NimBLEAddress g_target;
static NimBLEAddress g_knownTrain;
static bool     g_haveKnownTrain = false;
static uint32_t g_searchStartMs = 0;
static uint32_t g_scanAllowedAtMs = 0;
static const int SPEED_NOT_SENT = 1000;
static int      g_lastSentSpeed = SPEED_NOT_SENT;
static uint32_t g_lastSpeedSendMs = 0;
static uint8_t  g_lightIndex = 0;
static uint32_t g_scanCycles = 0;
static uint32_t g_connectOk = 0;
static uint32_t g_connectFail = 0;
static uint32_t g_notifications = 0;

// Best train seen in the current choosing window (strongest signal = nearest).
static bool          g_haveCandidate = false;
static NimBLEAddress g_candidate;
static int8_t        g_candidateRssi = 0;
static const char*   g_candidateReason = nullptr;
static uint32_t      g_candidateSinceMs = 0;

const char* linkStateName(LinkState s) {
  switch (s) {
    case LinkState::BOOT:          return "BOOT";
    case LinkState::SEARCHING:     return "SEARCHING";
    case LinkState::CONNECTING:    return "CONNECTING";
    case LinkState::WAIT_RESPONSE: return "WAIT_RESPONSE";
    case LinkState::CONNECTED:     return "CONNECTED";
    case LinkState::RECONNECTING:  return "RECONNECTING";
    case LinkState::SLEEP:         return "SLEEP";
  }
  return "?";
}

#define LOG(...) logLine(linkStateName(g_state.load()), __VA_ARGS__)

// ---- Queue (called from NimBLE callbacks and from loop) ----
static void postEvent(const Event& ev, bool lowPriority = false) {
  // A flood of scan results must not push out a disconnect or a notification.
  if (lowPriority && uxQueueSpacesAvailable(g_events) <= LOW_PRIO_RESERVE) {
    g_droppedEvents++;
    return;
  }
  if (xQueueSend(g_events, &ev, 0) != pdTRUE) {
    g_droppedEvents++;
  }
}

static Event makeEvent(EventType type, int code = 0) {
  Event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = type;
  ev.code = code;
  return ev;
}

// ---- NimBLE callbacks: only post events, never log or wait ----
class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    Event ev = makeEvent(EventType::DEVICE);
    ev.addr = *d->getAddress().getBase();
    ev.rssi = d->getRSSI();
    ev.advertisesLegoService = d->isAdvertisingService(SERVICE_UUID);
    if (d->haveName()) {
      strlcpy(ev.name, d->getName().c_str(), sizeof(ev.name));
    }
    if (d->getManufacturerDataCount() > 0) {
      std::string m = d->getManufacturerData();
      ev.fullLength = min<size_t>(m.size(), 255);
      ev.length = min<size_t>(m.size(), MAX_MANUFACTURER);
      memcpy(ev.payload, m.data(), ev.length);
    }
    postEvent(ev, true);
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    Event ev = makeEvent(EventType::SCAN_END, reason);
    ev.fullLength = min(results.getCount(), 255);
    postEvent(ev);
  }
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) override { postEvent(makeEvent(EventType::LINK_UP)); }
  void onConnectFail(NimBLEClient*, int reason) override { postEvent(makeEvent(EventType::CONNECT_FAIL, reason)); }
  void onDisconnect(NimBLEClient*, int reason) override { postEvent(makeEvent(EventType::DISCONNECT, reason)); }
};

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  Event ev = makeEvent(EventType::NOTIFY);
  ev.fullLength = min<size_t>(len, 255);
  ev.length = min<size_t>(len, MAX_PAYLOAD);
  memcpy(ev.payload, data, ev.length);
  postEvent(ev);
}

static ScanCallbacks   g_scanCallbacks;
static ClientCallbacks g_clientCallbacks;

// ---- Transitions ----
static void setState(LinkState next, const char* reason) {
  LinkState prev = g_state.load();
  uint32_t now = millis();
  logLine("FSM", "%s -> %s (%s), was in state %lu ms", linkStateName(prev), linkStateName(next), reason,
          (unsigned long)(now - g_stateSinceMs.load()));
  if (next == LinkState::CONNECTED) {
    g_lastSentSpeed = SPEED_NOT_SENT; // send the current (zero) speed right after connecting
  }
  g_stateSinceMs = now;
  g_state = next;
}

static bool isSearchState(LinkState s) { return s == LinkState::SEARCHING || s == LinkState::RECONNECTING; }

// Start a new search phase: the timeout to SLEEP counts from here.
static void beginSearchPhase(LinkState searchState, const char* reason) {
  g_haveCandidate = false;
  g_searchStartMs = millis();
  g_scanAllowedAtMs = millis() + RETRY_PAUSE_MS;
  setState(searchState, reason);
}

// Back to searching after a failure. The SLEEP timeout keeps running,
// otherwise constant errors would keep the board awake forever.
static void returnToSearch(const char* reason) {
  g_haveCandidate = false;
  g_scanAllowedAtMs = millis() + RETRY_PAUSE_MS;
  setState(g_haveKnownTrain ? LinkState::RECONNECTING : LinkState::SEARCHING, reason);
}

static void stopScan() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) {
    scan->stop();
  }
  scan->clearResults();
}

// ---- Search ----
static bool nameMatches(const char* name) {
  for (const char* sub : NAME_SUBSTRINGS) {
    if (strstr(name, sub) != nullptr) {
      return true;
    }
  }
  return false;
}

static void connectTo(const NimBLEAddress& addr, const char* reason) {
  g_haveCandidate = false;
  g_target = addr;
  stopScan();
  setState(LinkState::CONNECTING, reason);
}

static void handleDevice(const Event& ev) {
  if (!isSearchState(g_state.load())) {
    return; // leftovers in the queue after the scan stopped
  }

  NimBLEAddress addr(ev.addr);
  char mfg[MAX_MANUFACTURER * 3 + 1];
  toHex(ev.payload, ev.length, mfg, sizeof(mfg));
  LOG("dev %s type=%u rssi=%d lego_svc=%d name='%s' mfg=[%s]", addr.toString().c_str(), addr.getType(), ev.rssi,
      ev.advertisesLegoService, ev.name, mfg);

  if (millis() < g_scanAllowedAtMs) {
    return; // pause after a failure
  }

  // The train we just lost: connect at once, no need to compare.
  if (g_haveKnownTrain && addr == g_knownTrain) {
    connectTo(addr, "known train address");
    return;
  }

  // The hub takes a new random address on every power-on (seen on the train),
  // so trains are matched by service, or by name as a fallback.
  const char* reason = nullptr;
  if (ev.advertisesLegoService) {
    reason = "by service";
  } else if (nameMatches(ev.name)) {
    reason = "by name";
  } else {
    return;
  }

  // Several trains may be on: keep the strongest signal for NEAREST_WINDOW_MS.
  if (!g_haveCandidate) {
    g_haveCandidate = true;
    g_candidateSinceMs = millis();
    LOG("train %s rssi=%d found %s, looking for a nearer one for %lu ms", addr.toString().c_str(), ev.rssi, reason,
        (unsigned long)NEAREST_WINDOW_MS);
  } else if (ev.rssi <= g_candidateRssi) {
    return;
  } else if (addr != g_candidate) {
    LOG("train %s rssi=%d is nearer than %s rssi=%d", addr.toString().c_str(), ev.rssi,
        g_candidate.toString().c_str(), g_candidateRssi);
  }
  g_candidate = addr;
  g_candidateRssi = ev.rssi;
  g_candidateReason = reason;
}

// Connects to the nearest train once the choosing window is over.
static void tickNearest() {
  if (!g_haveCandidate || millis() - g_candidateSinceMs < NEAREST_WINDOW_MS) {
    return;
  }
  char reason[48];
  snprintf(reason, sizeof(reason), "nearest train %s, rssi=%d", g_candidateReason, g_candidateRssi);
  connectTo(g_candidate, reason);
}

static void tickSearch() {
  LinkState state = g_state.load();
  uint32_t now = millis();
  uint32_t limit = state == LinkState::RECONNECTING ? RECONNECT_TIMEOUT_MS : SEARCH_TIMEOUT_MS;

  if (now - g_searchStartMs >= limit) {
    stopScan();
    setState(LinkState::SLEEP, "search timeout");
    return;
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (now >= g_scanAllowedAtMs && !scan->isScanning()) {
    uint32_t cycle = ++g_scanCycles;
    if (scan->start(SCAN_CYCLE_MS, false)) {
      LOG("scan cycle #%lu started, %lu s left before sleep", (unsigned long)cycle,
          (unsigned long)((limit - (now - g_searchStartMs)) / 1000));
    } else {
      LOG("scan start FAILED");
      g_scanAllowedAtMs = now + RETRY_PAUSE_MS;
    }
  }
}

// ---- Sending ----
static bool send(const lwp::Command& cmd) {
  if (g_char == nullptr || !g_client->isConnected()) {
    LOG("tx %s: NOT CONNECTED, dropped", cmd.name);
    return false;
  }
  char hex[sizeof(cmd.bytes) * 3 + 1];
  toHex(cmd.bytes, cmd.length, hex, sizeof(hex));
  bool ok = g_char->writeValue(cmd.bytes, cmd.length, false); // without response: does not block
  LOG("tx %s: %s%s", cmd.name, hex, ok ? "" : " WRITE FAILED");
  return ok;
}

// Sends the desired speed at most every SPEED_SEND_INTERVAL_MS, and only when it changed.
static void tickSpeed() {
  uint32_t now = millis();
  if (now - g_lastSpeedSendMs < SPEED_SEND_INTERVAL_MS) {
    return;
  }
  int desired = g_desiredSpeed.load();
  if (desired == g_lastSentSpeed && !g_forceSpeedSend.load()) {
    return;
  }
  g_forceSpeedSend = false;
  g_lastSpeedSendMs = now;
  if (send(lwp::speed(desired))) {
    g_lastSentSpeed = desired; // on a write error, retry on the next tick
  }
}

// ---- Connecting ----
static void logLastError(const char* step) {
  int rc = g_client->getLastError();
  LOG("%s FAILED rc=%d (%s), link=%d", step, rc, NimBLEUtils::returnCodeToString(rc), g_client->isConnected());
}

static void logAllServices() {
  const auto& services = g_client->getServices(true);
  LOG("hub exposes %u service(s):", (unsigned)services.size());
  for (auto* service : services) {
    LOG("  service %s", service->getUUID().toString().c_str());
  }
}

static void failConnection(const char* reason) {
  g_connectFail++;
  g_char = nullptr;
  if (g_client->isConnected()) {
    g_client->disconnect();
  }
  returnToSearch(reason);
}

// Blocks only the link task; loop() keeps running meanwhile.
static void runConnecting() {
  LOG("step 1/4 link: connecting to %s type=%u", g_target.toString().c_str(), g_target.getType());
  uint32_t t = millis();
  if (!g_client->connect(g_target)) {
    LOG("step 1/4 took %lu ms", (unsigned long)(millis() - t));
    logLastError("step 1/4 link");
    failConnection("link failed");
    return;
  }
  LOG("step 1/4 link OK in %lu ms, mtu=%u", (unsigned long)(millis() - t), g_client->getMTU());

  t = millis();
  NimBLERemoteService* service = g_client->getService(SERVICE_UUID);
  if (service == nullptr) {
    logLastError("step 2/4 service");
    if (g_client->isConnected()) {
      logAllServices();
    }
    failConnection("service not found");
    return;
  }
  LOG("step 2/4 service OK in %lu ms", (unsigned long)(millis() - t));

  t = millis();
  g_char = service->getCharacteristic(CHAR_UUID);
  if (g_char == nullptr) {
    logLastError("step 3/4 characteristic");
    failConnection("characteristic not found");
    return;
  }
  LOG("step 3/4 characteristic OK in %lu ms, canNotify=%d canWriteNoRsp=%d", (unsigned long)(millis() - t),
      g_char->canNotify(), g_char->canWriteNoResponse());

  t = millis();
  if (!g_char->canNotify() || !g_char->subscribe(true, onNotify)) {
    logLastError("step 4/4 subscribe");
    failConnection("subscribe failed");
    return;
  }
  LOG("step 4/4 subscribe OK in %lu ms", (unsigned long)(millis() - t));

  g_knownTrain = g_target;
  g_haveKnownTrain = true;
  g_connectOk++;
  setState(LinkState::WAIT_RESPONSE, "subscribed");

  for (uint8_t port : {lwp::PORT_COLOR, lwp::PORT_SPEED_SENSOR, lwp::PORT_BATTERY}) {
    send(lwp::subscribePort(port));
  }
}

// ---- Commands ----
static void logStatus() {
  LOG("status: in state %lu ms, speed_req=%d sent=%d, known_train=%s, scans=%lu, connect ok=%lu fail=%lu, rx=%lu, "
      "dropped=%lu, heap=%lu",
      (unsigned long)(millis() - g_stateSinceMs.load()), g_desiredSpeed.load(),
      g_lastSentSpeed == SPEED_NOT_SENT ? 0 : g_lastSentSpeed,
      g_haveKnownTrain ? g_knownTrain.toString().c_str() : "none", (unsigned long)g_scanCycles,
      (unsigned long)g_connectOk, (unsigned long)g_connectFail, (unsigned long)g_notifications,
      (unsigned long)g_droppedEvents.load(), (unsigned long)ESP.getFreeHeap());
}

static void shutdownLink(const char* reason) {
  if (g_client->isConnected()) {
    send(lwp::speed(0));
    vTaskDelay(pdMS_TO_TICKS(STOP_BEFORE_DISCONNECT_MS));
    g_client->disconnect();
  }
  g_char = nullptr;
  stopScan();
  setState(LinkState::SLEEP, reason);
}

static void handleCommand(char cmd) {
  LinkState state = g_state.load();
  switch (cmd) {
    case LinkCmd::STATUS:
      logStatus();
      break;
    case LinkCmd::DISCONNECT:
      if (g_client->isConnected()) {
        LOG("cmd d: disconnecting");
        g_client->disconnect();
      } else {
        LOG("cmd d: not connected");
      }
      break;
    case LinkCmd::WAKE:
      if (state == LinkState::SLEEP) {
        g_haveKnownTrain = false;
        beginSearchPhase(LinkState::SEARCHING, "wake");
      }
      break;
    case LinkCmd::HORN:
      send(lwp::horn());
      break;
    case LinkCmd::NEXT_LIGHT:
      send(lwp::light(g_lightIndex));
      g_lightIndex = (g_lightIndex + 1) % lwp::LIGHT_COLOR_COUNT;
      break;
    case LinkCmd::SHUTDOWN:
      if (state != LinkState::SLEEP) {
        shutdownLink("inactivity");
      }
      break;
    case LinkCmd::SLEEP_NOW:
      if (state != LinkState::SLEEP) {
        shutdownLink("cmd z");
      }
      break;
    case LinkCmd::FAKE_CONNECT:
      if (isSearchState(state)) {
        g_target = NimBLEAddress(std::string(FAKE_TARGET), BLE_ADDR_PUBLIC);
        stopScan();
        setState(LinkState::CONNECTING, "cmd f: fake target");
      } else {
        LOG("cmd f: only from a search state");
      }
      break;
    default:
      LOG("unknown command '%c'", cmd);
      break;
  }
}

// ---- Events ----
static void handleEvent(const Event& ev) {
  LinkState state = g_state.load();
  switch (ev.type) {
    case EventType::DEVICE:
      handleDevice(ev);
      break;
    case EventType::SCAN_END:
      LOG("scan ended: reason=%d, devices=%u", ev.code, ev.fullLength);
      break;
    case EventType::LINK_UP:
      LOG("link up");
      break;
    case EventType::CONNECT_FAIL:
      LOG("connect fail callback: reason=%d (%s)", ev.code, NimBLEUtils::returnCodeToString(ev.code));
      break;
    case EventType::DISCONNECT:
      LOG("disconnected: reason=%d (%s)", ev.code, NimBLEUtils::returnCodeToString(ev.code));
      g_char = nullptr;
      if (state == LinkState::WAIT_RESPONSE || state == LinkState::CONNECTED) {
        beginSearchPhase(LinkState::RECONNECTING, "link lost");
      }
      break;
    case EventType::NOTIFY:
      g_notifications++;
      lwp::logNotification(linkStateName(state), ev.payload, ev.length, ev.fullLength);
      if (state == LinkState::WAIT_RESPONSE) {
        setState(LinkState::CONNECTED, "first notification received");
      }
      break;
    case EventType::COMMAND:
      handleCommand((char)ev.code);
      break;
  }
}

// ---- Task ----
static void bootBle() {
  LOG("reset reason=%d, NimBLE %s", (int)esp_reset_reason(), NimBLEDevice::getVersion());

  NimBLEDevice::init("ESP32-DuploCtl");
  LOG("own address %s", NimBLEDevice::getAddress().toString().c_str());

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&g_scanCallbacks);
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(45);

  // One client for the whole run instead of a new one per attempt.
  g_client = NimBLEDevice::createClient();
  g_client->setClientCallbacks(&g_clientCallbacks, false);
  g_client->setSelfDelete(false, false);
  g_client->setConnectTimeout(CONNECT_TIMEOUT_MS);
  g_client->setConnectRetries(CONNECT_RETRIES);
}

static void linkTask(void*) {
  bootBle();
  beginSearchPhase(LinkState::SEARCHING, "ble ready");
  g_scanAllowedAtMs = millis();

  Event ev;
  for (;;) {
    if (xQueueReceive(g_events, &ev, pdMS_TO_TICKS(20)) == pdTRUE) {
      handleEvent(ev);
    }

    switch (g_state.load()) {
      case LinkState::SEARCHING:
      case LinkState::RECONNECTING:
        tickNearest();
        if (isSearchState(g_state.load())) {
          tickSearch();
        }
        break;
      case LinkState::CONNECTING:
        runConnecting();
        break;
      case LinkState::WAIT_RESPONSE:
        if (millis() - g_stateSinceMs.load() >= RESPONSE_TIMEOUT_MS) {
          LOG("no notification in %lu ms", (unsigned long)RESPONSE_TIMEOUT_MS);
          g_client->disconnect();
          g_char = nullptr;
          beginSearchPhase(LinkState::RECONNECTING, "no response");
        }
        break;
      case LinkState::CONNECTED:
        tickSpeed();
        break;
      case LinkState::BOOT:
      case LinkState::SLEEP:
        break;
    }
  }
}

// ---- Public API ----
void linkStart() {
  g_events = xQueueCreate(EVENT_QUEUE_LEN, sizeof(Event));
  // Same priority as loop(); the NimBLE host task runs above both.
  xTaskCreatePinnedToCore(linkTask, "app_fsm", 8192, nullptr, 1, nullptr, 0);
}

LinkState linkState() { return g_state.load(); }
uint32_t linkStateSinceMs() { return g_stateSinceMs.load(); }
void linkCommand(char cmd) { postEvent(makeEvent(EventType::COMMAND, cmd)); }
void linkSetDesiredSpeed(int speed) { g_desiredSpeed = speed; }
int linkDesiredSpeed() { return g_desiredSpeed.load(); }
void linkForceSpeedSend() { g_forceSpeedSend = true; }
uint32_t linkDroppedEvents() { return g_droppedEvents.load(); }
