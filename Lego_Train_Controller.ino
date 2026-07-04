/*
 * LEGO Duplo Train Controller – ESP32-C3
 *
 * Combines BLE train control (from Lego_Train_esprunio_v011.js)
 * with button / potentiometer / relay handling (from ESP32-C3_Test_Firmware.ino).
 * BLE commands are taken from the tested Python reference (duplo_train.py).
 *
 * Hardware:
 *   GPIO 0–3  Buttons 1–4 (INPUT_PULLDOWN)
 *   GPIO 4    Potentiometer (analog, 0–4095)
 *   GPIO 5    Power relay
 *
 * Button mapping:
 *   Button 1 – Horn
 *   Button 2 – Cycle light colour
 *   Button 3 – Water / steam sound
 *   Button 4 – Connect / disconnect train BLE
 *
 * Potentiometer: full CCW = −100 % (full reverse),
 *                centre    =    0 % (stop, ±10 % deadband),
 *                full CW   = +100 % (full forward).
 *
 * Relay turns off automatically after 5 minutes of inactivity and
 * reactivates on the next button press or potentiometer movement.
 *
 * BLE discovery: scans for a device whose name contains "DUPLO",
 * "Train", "LEGO", or "HUB".  Falls back to the hardcoded address
 * from the Espruino script if no device is found by name.
 */

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ─── Pin configuration ────────────────────────────────────────────────────────
static const int BUTTON_PINS[4]    = {0, 1, 2, 3};
static const int POTENTIOMETER_PIN = 4;
static const int RELAY_PIN         = 5;

// ─── BLE configuration ────────────────────────────────────────────────────────
static const char* FALLBACK_ADDRESS = "e0:7d:ea:0c:03:29"; // from Espruino script
static const char* SERVICE_UUID     = "00001623-1212-efde-1623-785feabcd123";
static const char* CHAR_UUID        = "00001624-1212-efde-1623-785feabcd123";

// ─── Timing ───────────────────────────────────────────────────────────────────
static const unsigned long INACTIVITY_TIMEOUT_MS = 5UL * 60 * 1000; // 5 min
static const int           DEBOUNCE_MS           = 50;
static const int           POT_THRESHOLD         = 80;   // ~2 % of 4095
static const unsigned long BLE_RETRY_MS          = 6000; // reconnect interval

// ─── LEGO Duplo port constants (verified in duplo_train.py) ──────────────────
static const uint8_t PORT_MOTOR        = 0x32;
static const uint8_t PORT_SOUND_LIGHT  = 0x34; // sounds and LED
static const uint8_t PORT_COLOR_SENSOR = 0x33;
static const uint8_t PORT_BATTERY      = 0x35;
static const uint8_t PORT_SPEED_SENSOR = 0x36;

// Light colour codes used for cycling (yellow / green / blue / red / white)
static const uint8_t LIGHT_COLORS[]   = {0x01, 0x02, 0x03, 0x05, 0x0A};
static const int     NUM_LIGHT_COLORS = (int)sizeof(LIGHT_COLORS);

// ─── Runtime state ────────────────────────────────────────────────────────────
static BLEClient*               pClient      = nullptr;
static BLERemoteCharacteristic* pChar        = nullptr;
static bool                     bleConnected = false;
static int                      lightColorIdx= 0;

static unsigned long lastEventTime    = 0;
static bool          relayActive      = true;
static int           lastPotValue     = 0;
static bool          prevBtnStates[4] = {};
static unsigned long lastBleRetry     = 0;

// ─── BLE notification callback ───────────────────────────────────────────────
static void onNotify(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  if (len < 5) return;
  uint8_t msgType = data[2];
  uint8_t port    = data[3];

  if (msgType == 0x45) { // Port value update
    if (port == PORT_COLOR_SENSOR) {
      Serial.printf("Tile colour: 0x%02X\n", data[4]);
    } else if (port == PORT_BATTERY && len >= 6) {
      uint16_t mv;
      memcpy(&mv, data + 4, 2);
      Serial.printf("Battery: %u mV\n", mv);
    } else if (port == PORT_SPEED_SENSOR) {
      Serial.printf("Speedometer: %d\n", (int8_t)data[4]);
    }
  }
}

// ─── BLE client callbacks ────────────────────────────────────────────────────
class TrainClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient*) override { Serial.println("BLE: connected"); }
  void onDisconnect(BLEClient*) override {
    bleConnected = false;
    pChar        = nullptr;
    Serial.println("BLE: disconnected");
  }
};

// ─── Low-level BLE write ─────────────────────────────────────────────────────
static void bleSend(const uint8_t* buf, size_t len) {
  if (!bleConnected || !pChar) return;
  pChar->writeValue(const_cast<uint8_t*>(buf), len, false);
}

// ─── Post-connection setup: enable notifications and subscribe to sensors ────
static void setupSensors() {
  if (pChar->canNotify()) {
    pChar->registerForNotify(onNotify);
  }
  uint8_t subColor[]   = {0x0A,0x00,0x41,PORT_COLOR_SENSOR,0x00,0x01,0x00,0x00,0x00,0x01};
  uint8_t subSpeed[]   = {0x0A,0x00,0x41,PORT_SPEED_SENSOR,0x00,0x01,0x00,0x00,0x00,0x01};
  uint8_t subBattery[] = {0x0A,0x00,0x41,PORT_BATTERY,     0x00,0x01,0x00,0x00,0x00,0x01};
  bleSend(subColor,   sizeof(subColor));   delay(50);
  bleSend(subSpeed,   sizeof(subSpeed));   delay(50);
  bleSend(subBattery, sizeof(subBattery)); delay(50);
}

// ─── Finish connecting once we have the client connected ─────────────────────
static bool finalizeConnection() {
  BLERemoteService* pSvc = pClient->getService(SERVICE_UUID);
  if (!pSvc) {
    Serial.println("BLE: service not found");
    pClient->disconnect();
    return false;
  }
  pChar = pSvc->getCharacteristic(CHAR_UUID);
  if (!pChar) {
    Serial.println("BLE: characteristic not found");
    pClient->disconnect();
    return false;
  }
  bleConnected = true;
  setupSensors();
  Serial.println("BLE: train ready");
  return true;
}

// ─── Connect by scanning for a DUPLO/LEGO/Train/HUB device name ──────────────
static bool connectTrain() {
  if (bleConnected) return true;

  if (!pClient) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new TrainClientCallbacks());
  }

  // --- Try scanning by name first ---
  Serial.println("BLE: scanning for LEGO train...");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  BLEScanResults results = pScan->start(5, false);

  BLEAdvertisedDevice* target = nullptr;
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice d = results.getDevice(i);
    String name = String(d.getName().c_str());
    if (name.indexOf("DUPLO") >= 0 || name.indexOf("Train") >= 0 ||
        name.indexOf("LEGO")  >= 0 || name.indexOf("HUB")   >= 0) {
      target = new BLEAdvertisedDevice(d);
      Serial.printf("BLE: found \"%s\" [%s]\n",
                    d.getName().c_str(), d.getAddress().toString().c_str());
      break;
    }
  }
  pScan->clearResults();

  if (target) {
    bool ok = pClient->connect(target->getAddress());
    delete target;
    if (ok) return finalizeConnection();
    Serial.println("BLE: connection to scanned device failed");
  } else {
    Serial.println("BLE: no device found by name, trying hardcoded address...");
  }

  // --- Fallback: connect by hardcoded address ---
  if (pClient->connect(BLEAddress(FALLBACK_ADDRESS))) {
    return finalizeConnection();
  }

  Serial.println("BLE: connection failed");
  return false;
}

static void disconnectTrain() {
  if (pClient && bleConnected) pClient->disconnect();
  bleConnected = false;
  pChar        = nullptr;
  Serial.println("BLE: disconnected by user");
}

// ─── Train commands ───────────────────────────────────────────────────────────

// Horn (port 0x34, sound subcommand 0x07)
static void trainHorn() {
  uint8_t cmd[] = {0x0B,0x00,0x81,PORT_SOUND_LIGHT,0x11,0x51,0x01,0x07,0x01,0x00,0x00};
  bleSend(cmd, sizeof(cmd));
  Serial.println("Train: horn");
}

// Water / steam sound (port 0x34, sound subcommand 0x09)
static void trainWaterSound() {
  uint8_t cmd[] = {0x0B,0x00,0x81,PORT_SOUND_LIGHT,0x11,0x51,0x01,0x09,0x01,0x00,0x00};
  bleSend(cmd, sizeof(cmd));
  Serial.println("Train: water sound");
}

// Set LED to a specific colour code
static void trainSetLightColor(uint8_t colorCode) {
  uint8_t cmd[] = {0x0B,0x00,0x81,PORT_SOUND_LIGHT,0x11,0x51,0x01,0x04,0x01,colorCode,0x00};
  bleSend(cmd, sizeof(cmd));
  Serial.printf("Train: light colour 0x%02X\n", colorCode);
}

// Cycle through the predefined light colours
static void trainNextLightColor() {
  trainSetLightColor(LIGHT_COLORS[lightColorIdx]);
  lightColorIdx = (lightColorIdx + 1) % NUM_LIGHT_COLORS;
}

// Float-stop the motor
static void trainStop() {
  uint8_t cmd[] = {0x08,0x00,0x81,PORT_MOTOR,0x11,0x51,0x00,0x7F};
  bleSend(cmd, sizeof(cmd));
  Serial.println("Train: stop");
}

// Set motor speed: −100…+100 %
// Positive values: byte = speed; negative values: byte = 256 + speed (per duplo_train.py)
static void trainSetSpeed(int speedPct) {
  speedPct = constrain(speedPct, -100, 100);
  if (speedPct == 0) { trainStop(); return; }
  uint8_t s = (speedPct > 0) ? (uint8_t)speedPct : (uint8_t)(256 + speedPct);
  uint8_t cmd[] = {0x09,0x00,0x81,PORT_MOTOR,0x11,0x07,s,0x64,0x03};
  bleSend(cmd, sizeof(cmd));
  Serial.printf("Train: speed %d %%\n", speedPct);
}

// ─── Relay / inactivity helpers ───────────────────────────────────────────────
static void recordEvent() {
  lastEventTime = millis();
  if (!relayActive) {
    relayActive = true;
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("*** RELAY REACTIVATED ***");
  }
}

static void checkInactivityTimeout() {
  if (relayActive && (millis() - lastEventTime) >= INACTIVITY_TIMEOUT_MS) {
    relayActive = false;
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("!!! INACTIVITY TIMEOUT – relay off !!!");
  }
}

// ─── Button handling ─────────────────────────────────────────────────────────
static void onButtonPress(int btn) {
  Serial.printf("Button %d pressed at %lu ms\n", btn, millis());
  recordEvent();
  switch (btn) {
    case 1: trainHorn();          break;
    case 2: trainNextLightColor();break;
    case 3: trainWaterSound();    break;
    case 4:
      if (bleConnected) disconnectTrain();
      else              connectTrain();
      break;
  }
}

static void checkButtons() {
  for (int i = 0; i < 4; i++) {
    bool state = digitalRead(BUTTON_PINS[i]);
    if (state && !prevBtnStates[i]) {
      delay(DEBOUNCE_MS);
      if (digitalRead(BUTTON_PINS[i])) onButtonPress(i + 1);
    }
    prevBtnStates[i] = state;
  }
}

// ─── Potentiometer → train speed ─────────────────────────────────────────────
// Maps ADC 0–4095 linearly to −100…+100 %; ±10 % deadband at centre = stop.
static int adcToSpeedPct(int adc) {
  int pct = (int)map((long)adc, 0L, 4095L, -100L, 100L);
  if (pct >= -10 && pct <= 10) pct = 0;
  return pct;
}

static void checkPotentiometer() {
  int adc  = analogRead(POTENTIOMETER_PIN);
  int diff = abs(adc - lastPotValue);
  if (diff > POT_THRESHOLD) {
    lastPotValue = adc;
    recordEvent();
    trainSetSpeed(adcToSpeedPct(adc));
  }
}

// ─── BLE auto-reconnect ───────────────────────────────────────────────────────
static void checkBleConnection() {
  if (!bleConnected && (millis() - lastBleRetry) >= BLE_RETRY_MS) {
    lastBleRetry = millis();
    connectTrain();
  }
}

// ════════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n================================");
  Serial.println(" LEGO Duplo Train Controller");
  Serial.println("================================\n");

  for (int i = 0; i < 4; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
    Serial.printf("Button %d on GPIO %d\n", i + 1, BUTTON_PINS[i]);
  }

  pinMode(POTENTIOMETER_PIN, INPUT);
  Serial.printf("Potentiometer on GPIO %d\n", POTENTIOMETER_PIN);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.printf("Relay on GPIO %d (active)\n\n", RELAY_PIN);

  lastEventTime = millis();
  lastPotValue  = analogRead(POTENTIOMETER_PIN);

  BLEDevice::init("ESP32-TrainCtrl");
  connectTrain();

  Serial.println("System ready!\n");
}

void loop() {
  checkButtons();
  checkPotentiometer();
  checkInactivityTimeout();
  checkBleConnection();
  delay(10);
}
