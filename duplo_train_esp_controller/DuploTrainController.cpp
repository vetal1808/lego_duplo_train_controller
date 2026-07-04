#include "DuploTrainController.h"

// Static member initialization
BLEUUID DuploTrainController::serviceUUID(GATT_ID);
BLEUUID DuploTrainController::charUUID(GATT_ID);
DuploTrainController* DuploTrainController::instance = nullptr;

// BLE Advertised Device Callback
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (DuploTrainController::getInstance()) {
            DuploTrainController::getInstance()->handleScanResults(advertisedDevice);
        }
    }
};

// BLE Client Callback
class MyClientCallback : public BLEClientCallbacks {
public:
    void onConnect(BLEClient* pclient) override {
        Serial.println("✅ Connected to LEGO Hub!");
    }

    void onDisconnect(BLEClient* pclient) override {
        Serial.println("❌ Disconnected from LEGO Hub");
        // Обработка разрыва соединения в контроллере
        if (DuploTrainController::getInstance()) {
            DuploTrainController::getInstance()->handleDisconnect();
        }
    }
};

// Static callback wrapper
void DuploTrainController::notifyCallbackHelper(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
    if (instance) {
        instance->onNotifyData(pData, length);
    }
}

// Constructor
DuploTrainController::DuploTrainController()
    : pRemoteCharacteristic(nullptr), myDevice(nullptr), pClient(nullptr),
      connected(false), scanInProgress(false), sensorCallback(nullptr), 
      scanStartTime(0), autoRescanOnDisconnect(true) {
    instance = this;
}

// Destructor
DuploTrainController::~DuploTrainController() {
    if (pClient) {
        pClient->disconnect();
    }
    instance = nullptr;
}

// Get singleton instance
DuploTrainController* DuploTrainController::getInstance() {
    return instance;
}

// Initialize BLE device
void DuploTrainController::init() {
    BLEDevice::init("");
    
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    
    Serial.println("✅ BLE initialized. Call startScan() to begin searching for LEGO DUPLO Train.\n");
}

// Start BLE scan (non-blocking)
void DuploTrainController::startScan() {
    if (scanInProgress) {
        Serial.println("⚠️  Scan already in progress");
        return;
    }
    Serial.println("🔍 Starting BLE scan for LEGO DUPLO Train...");
    scanInProgress = true;
    scanStartTime = millis();
    BLEDevice::getScan()->start(0, false);  // Асинхронное сканирование
}

// Stop BLE scan (non-blocking)
void DuploTrainController::stopScan() {
    if (!scanInProgress) {
        Serial.println("⚠️  Scan is not running");
        return;
    }
    Serial.println("⏹️  Stopping BLE scan");
    BLEDevice::getScan()->stop();
    scanInProgress = false;
}

// Handle hub disconnection - internal cleanup and optional auto-rescan
void DuploTrainController::handleDisconnect() {
    Serial.println("🧹 Cleaning up connection resources...");
    
    // Очищаем состояние
    connected = false;
    pRemoteCharacteristic = nullptr;
    
    // Отключаем клиент если не был отключен
    if (pClient) {
        pClient->disconnect();
    }
    
    // Очищаем данные устройства для переподключения
    if (myDevice) {
        delete myDevice;
        myDevice = nullptr;
    }
    
    Serial.println("✅ Cleanup complete.");
    
    // Если включено автоматическое переподключение - перезапускаем сканирование
    if (autoRescanOnDisconnect && !scanInProgress) {
        Serial.println("🔄 Auto-rescan enabled. Restarting BLE scan in 2 seconds...");
        delay(2000);  // TODO: Заменить на millis()-based таймер в update()
        startScan();
    } else if (!autoRescanOnDisconnect) {
        Serial.println("⚠️  Auto-rescan disabled. Call startScan() to search again.");
    }
}

// Set auto-rescan on disconnect
void DuploTrainController::setAutoRescan(bool enabled) {
    autoRescanOnDisconnect = enabled;
    Serial.print("🔧 Auto-rescan ");
    Serial.println(enabled ? "ENABLED" : "DISABLED");
}

// Handle scan results
void DuploTrainController::handleScanResults(BLEAdvertisedDevice advertisedDevice) {
    // Пропускаем если уже нашли девайс
    if (myDevice != nullptr) {
        return;
    }
    
    Serial.print("📡 Found BLE device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if it's LEGO Hub
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(serviceUUID)) {
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        // Остановка сканирования произойдёт в update()
    }
}

// Connect to LEGO Hub
bool DuploTrainController::connectToHub() {
    if (!myDevice) {
        Serial.println("❌ No LEGO Hub device found!");
        return false;
    }

    Serial.print("Connecting to: ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    pClient = BLEDevice::createClient();
    Serial.println(" - Client created");

    pClient->setClientCallbacks(new MyClientCallback());

    // TODO: BLOCKING OPERATION - pClient->connect() может зависнуть без таймаута!
    // Рекомендация: Добавить таймаут и реализовать асинхронное подключение
    // Connect to remote BLE server
    pClient->connect(myDevice);
    Serial.println(" - Connected to server");

    // TODO: BLOCKING OPERATION - getService() может заблокировать поиск сервиса!
    // Рекомендация: Добавить таймаут и обработку ошибок для обнаружения сервиса
    // Get service reference
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Service UUID not found: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Service found");

    // TODO: BLOCKING OPERATION - getCharacteristic() может заблокировать поиск характеристик!
    // Рекомендация: Добавить таймаут и обработку ошибок
    // Get characteristic reference
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Characteristic UUID not found: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Characteristic found");

    // Register for notifications
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallbackHelper);
        Serial.println(" - Registered for notifications");
    }

    connected = true;
    return true;
}

// Check connection status
bool DuploTrainController::isConnected() const {
    return connected;
}

// Check if device was found during scan
bool DuploTrainController::isDeviceFound() const {
    return myDevice != nullptr;
}

// Disconnect from hub
void DuploTrainController::disconnect() {
    if (pClient && connected) {
        pClient->disconnect();
        handleDisconnect();
    }
}

// Send raw command
void DuploTrainController::sendCommand(const uint8_t* command, size_t length) {
    if (!connected || !pRemoteCharacteristic) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }
    // TODO: POTENTIAL BLOCKING - writeValue() может блокировать при проблемах с BLE!
    // Рекомендация: Добавить таймаут и очередь команд для асинхронной отправки
    pRemoteCharacteristic->writeValue((uint8_t*)command, length);
}

// Motor: Set speed
void DuploTrainController::setMotorSpeed(uint8_t port, int8_t speed) {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {
        0x09,           // Length
        0x00,           // Hub ID
        0x81,           // Message Type (Port Output Command)
        port,           // Port ID
        0x11,           // Startup & Completion
        0x07,           // Sub Command (StartSpeed)
        (uint8_t)speed, // Speed (-100 to 100)
        0x64,           // Max Power (100%)
        0x03            // Use Profile
    };

    sendCommand(command, sizeof(command));
    
    Serial.print("🚂 Motor ");
    Serial.print(port, HEX);
    Serial.print(" Speed: ");
    Serial.print(speed);
    Serial.println("%");
}

// Motor: Stop
void DuploTrainController::stopMotor(uint8_t port) {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {
        0x08,       // Length
        0x00,       // Hub ID
        0x81,       // Message Type
        port,       // Port ID
        0x11,       // Startup & Completion
        0x51,       // SubCommand
        0x00,       // Parameter
        0x7F        // Float mode
    };

    sendCommand(command, sizeof(command));
    Serial.println("⛔ Stop");
}

// Emergency stop all motors
void DuploTrainController::emergencyStop() {
    Serial.println("🛑 EMERGENCY STOP!");
    stopMotor(PORT_MOTOR);
}

// Sound: Horn
void DuploTrainController::doHorn() {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x07, 0x01, 0x00, 0x00};
    sendCommand(command, sizeof(command));
    Serial.println("📯 Horn!");
}

// Light: Set color
void DuploTrainController::setLightColor(uint8_t color_code) {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, color_code, 0x00};
    sendCommand(command, sizeof(command));
    
    Serial.print("💡 Light color: 0x");
    Serial.println(color_code, HEX);
}

// Subscribe: Color sensor
void DuploTrainController::subscribeToColorSensor() {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {0x0A, 0x00, 0x41, PORT_COLOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    sendCommand(command, sizeof(command));
    Serial.println("📡 Subscribed to color sensor");
}

// Subscribe: Speed sensor
void DuploTrainController::subscribeToSpeedSensor() {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {0x0A, 0x00, 0x41, PORT_SPEED_SENSOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    sendCommand(command, sizeof(command));
    Serial.println("📡 Subscribed to speed sensor");
}

// Subscribe: Battery sensor
void DuploTrainController::subscribeToBatterySensor() {
    if (!connected) {
        Serial.println("❌ Not connected to Hub!");
        return;
    }

    uint8_t command[] = {0x0A, 0x00, 0x41, PORT_BATTERY, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    sendCommand(command, sizeof(command));
    Serial.println("📡 Subscribed to battery sensor");
}

// Subscribe: All sensors
void DuploTrainController::subscribeToAllSensors() {
    subscribeToColorSensor();
    // TODO: BLOCKING OPERATION - delay(100) замораживает CPU на 100ms!
    // Рекомендация: Использовать state machine с millis() вместо delay()
    delay(100);
    subscribeToSpeedSensor();
    // TODO: BLOCKING OPERATION - delay(100) замораживает CPU на 100ms!
    // Рекомендация: Использовать state machine с millis() вместо delay()
    delay(100);
    subscribeToBatterySensor();
    // TODO: BLOCKING OPERATION - delay(100) замораживает CPU на 100ms!
    // Рекомендация: Использовать state machine с millis() вместо delay()
    delay(100);
}

// Set sensor callback
void DuploTrainController::setSensorCallback(SensorDataCallback callback) {
    sensorCallback = callback;
}

// Handle notification data
void DuploTrainController::onNotifyData(uint8_t* pData, size_t length) {
    if (length < 5) return;
    
    uint8_t msg_type = pData[2];
    uint8_t port = pData[3];

    if (msg_type == 0x45) {  // Event message type
        if (port == PORT_COLOR && length >= 5) {
            uint8_t color_id = pData[4];
            Serial.print("🎨 Tile color: 0x");
            Serial.println(color_id, HEX);
        }
        else if (port == PORT_SPEED_SENSOR && length >= 5) {
            int8_t speed_value = (int8_t)pData[4];
            Serial.print("🏃 Speedometer: ");
            Serial.println(speed_value);
        }
        else if (port == PORT_BATTERY && length >= 6) {
            uint16_t voltage_mv = pData[4] | (pData[5] << 8);
            Serial.print("🔋 Battery: ");
            Serial.print(voltage_mv);
            Serial.println(" mV");
        }
    }
    
    // Call user callback if set
    if (sensorCallback) {
        sensorCallback(port, pData, length);
    }
}

// Update routine (call from main loop)
void DuploTrainController::update() {
    // Проверка таймаута сканирования
    if (scanInProgress && myDevice == nullptr) {
        unsigned long elapsed = millis() - scanStartTime;
        if (elapsed >= 5000) {  // 5 секунд
            Serial.println("⏱️  Scan timeout - stopping BLE scan");
            BLEDevice::getScan()->stop();
            scanInProgress = false;
            Serial.println("❌ LEGO DUPLO Train not found. Call startScan() to try again.");
        }
    }
    
    // Если девайс найден и сканирование активно - останавливаем сканирование
    if (scanInProgress && myDevice != nullptr) {
        Serial.println("✅ Found LEGO DUPLO Train! Stopping scan.");
        BLEDevice::getScan()->stop();
        scanInProgress = false;
    }
    
    // Проверка соединения - если потеряно, обработать отключение
    // (обычно обработается через onDisconnect callback, но добавляем проверку)
    if (connected && pClient && !pClient->isConnected()) {
        Serial.println("⚠️  Connection lost! (detected in update)");
        handleDisconnect();
    }
}

// Print status
void DuploTrainController::printStatus() const {
    Serial.println("\n=== DuploTrainController Status ===");
    Serial.print("Connected: ");
    Serial.println(connected ? "YES" : "NO");
    Serial.print("Device: ");
    if (myDevice) {
        Serial.println(myDevice->getAddress().toString().c_str());
    } else {
        Serial.println("None");
    }
    Serial.println("===================================\n");
}
