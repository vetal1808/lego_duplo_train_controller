#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// LEGO Hub BLE Service и Characteristic UUIDs

#define GATT_ID        "00001623-1212-efde-1623-785feabcd123"

static BLEUUID serviceUUID(GATT_ID);
static BLEUUID charUUID(GATT_ID);

// Глобальные переменные
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// Определение портов (как в Python скрипте)
#define PORT_MOTOR         0x32
#define PORT_COLOR         0x33
#define PORT_BATTERY       0x35
#define PORT_SPEED_SENSOR  0x36

// Callback для уведомлений от LEGO Hub
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  
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
}

// Callback для подключения/отключения
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("✅ Connected to LEGO Hub!");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("❌ Disconnected from LEGO Hub");
  }
};

// Функция подключения к LEGO Hub
bool connectToServer() {
  Serial.print("Connecting to: ");
  Serial.println(myDevice->getAddress().toString().c_str());
  
  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Client created");

  pClient->setClientCallbacks(new MyClientCallback());

  // Подключение к удаленному BLE серверу
  pClient->connect(myDevice);
  Serial.println(" - Connected to server");

  // Получение ссылки на сервис
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Service UUID not found: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Service found");

  // Получение ссылки на характеристику
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Characteristic UUID not found: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Characteristic found");

  // Регистрация на уведомления
  if(pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println(" - Registered for notifications");
  }

  connected = true;
  return true;
}

// Callback для поиска устройств
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("📡 Found BLE device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Проверяем, является ли это LEGO Hub
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      Serial.println("✅ Found LEGO DUPLO Train!");
    }
  }
};

// Функция для отправки команды на мотор
void sendMotorCommand(uint8_t port, int8_t speed) {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  // Формируем команду StartSpeed (как в Python)
  uint8_t command[] = {
    0x09,        // Length
    0x00,        // Hub ID
    0x81,        // Message Type (Port Output Command)
    port,        // Port ID
    0x11,        // Startup & Completion
    0x07,        // Sub Command (StartSpeed)
    (uint8_t)speed,  // Speed (-100 до 100)
    0x64,        // Max Power (100%)
    0x03         // Use Profile
  };

  pRemoteCharacteristic->writeValue(command, sizeof(command));
  
  Serial.print("🚂 Speed: ");
  Serial.print(speed);
  Serial.println("%");
}

// Функция гудка (как в Python)
void doHorn() {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  uint8_t command[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x07, 0x01, 0x00, 0x00};
  pRemoteCharacteristic->writeValue(command, sizeof(command));
  Serial.println("📯 Horn!");
}

// Установить цвет света (как в Python)
void setLightColor(uint8_t color_code) {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  uint8_t command[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, color_code, 0x00};
  pRemoteCharacteristic->writeValue(command, sizeof(command));
  
  Serial.print("💡 Light color: 0x");
  Serial.println(color_code, HEX);
}

// Остановка мотора (float mode, как в Python)
void stopMotor(uint8_t port) {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  // Stop command: float mode (0x7F)
  uint8_t command[] = {
    0x08,        // Length
    0x00,        // Hub ID
    0x81,        // Message Type
    port,        // Port ID
    0x11,        // Startup & Completion
    0x51,        // SubCommand
    0x00,        // Parameter
    0x7F         // Float mode
  };

  pRemoteCharacteristic->writeValue(command, sizeof(command));
  Serial.println("⛔ Stop");
}

// Подписка на датчик цвета (как в Python)
void subscribeToColorSensor() {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  uint8_t command[] = {0x0A, 0x00, 0x41, PORT_COLOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
  pRemoteCharacteristic->writeValue(command, sizeof(command));
  Serial.println("📡 Subscribed to color sensor");
}

// Подписка на датчик скорости (как в Python)
void subscribeToSpeedSensor() {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  uint8_t command[] = {0x0A, 0x00, 0x41, PORT_SPEED_SENSOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
  pRemoteCharacteristic->writeValue(command, sizeof(command));
  Serial.println("📡 Subscribed to speed sensor");
}

// Подписка на датчик батареи (как в Python)
void subscribeToBatterySensor() {
  if (!connected) {
    Serial.println("❌ Not connected to Hub!");
    return;
  }

  uint8_t command[] = {0x0A, 0x00, 0x41, PORT_BATTERY, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
  pRemoteCharacteristic->writeValue(command, sizeof(command));
  Serial.println("📡 Subscribed to battery sensor");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n===========================================");
  Serial.println("🚂 LEGO DUPLO Train Controller for ESP32");
  Serial.println("===========================================\n");

  BLEDevice::init("");
  
  // Начинаем сканирование
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
  
  Serial.println("🔍 Searching for LEGO DUPLO Train...\n");
}

void loop() {
  // Если найден Hub и нужно подключиться
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("\n✅ Successfully connected to LEGO Hub!\n");
      
      delay(1000);
      
      // Подписываемся на датчики (как в Python)
      subscribeToColorSensor();
      subscribeToSpeedSensor();
      subscribeToBatterySensor();
      
      delay(500);
      
    } else {
      Serial.println("❌ Failed to connect to LEGO Hub");
    }
    doConnect = false;
  }

  // Демонстрационная программа управления
  if (connected) {
    Serial.println("\n" + String('=', 40));
    Serial.println("🚂 MOTOR CONTROL DEMO");
    Serial.println(String('=', 40) + "\n");
    
    // Тест 1: Движение вперед на 50%
    Serial.println("→ Forward at 50% speed...");
    sendMotorCommand(PORT_MOTOR, 50);
    delay(2000);
    
    // Тест 2: Остановка
    Serial.println("\nStopping...");
    stopMotor(PORT_MOTOR);
    delay(1000);
    
    // Тест 3: Гудок
    Serial.println("\nHorn test...");
    doHorn();
    delay(500);
    
    // Тест 4: Свет - красный
    Serial.println("\nRed light...");
    setLightColor(0x05);
    delay(1000);
    
    // Тест 5: Свет - зелёный
    Serial.println("\nGreen light...");
    setLightColor(0x02);
    delay(1000);
    
    // Тест 6: Свет - синий
    Serial.println("\nBlue light...");
    setLightColor(0x03);
    delay(1000);
    
    // Тест 7: Свет - жёлтый
    Serial.println("\nYellow light...");
    setLightColor(0x01);
    delay(1000);
    
    // Тест 8: Свет - белый
    Serial.println("\nWhite light...");
    setLightColor(0x0A);
    delay(1000);
    
    // Тест 9: Движение назад на 30%
    Serial.println("\n← Backward at 30% speed...");
    sendMotorCommand(PORT_MOTOR, -30);
    delay(2000);
    
    // Тест 10: Полная скорость вперед
    Serial.println("\nFull speed forward...");
    sendMotorCommand(PORT_MOTOR, 100);
    delay(2000);
    
    // Финальная остановка
    Serial.println("\nFinal stop...");
    stopMotor(PORT_MOTOR);
    
    Serial.println("\n" + String('=', 40));
    Serial.println("✅ DEMO COMPLETE");
    Serial.println(String('=', 40) + "\n");
    
    // Отключаемся после демонстрации
    connected = false;
    if (pRemoteCharacteristic->getRemoteService()->getClient()) {
      pRemoteCharacteristic->getRemoteService()->getClient()->disconnect();
      Serial.println("👋 Disconnected\n");
    }
  }

  delay(1000);
}