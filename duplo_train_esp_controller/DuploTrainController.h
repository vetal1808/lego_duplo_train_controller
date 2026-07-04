#ifndef DUPLO_TRAIN_CONTROLLER_H
#define DUPLO_TRAIN_CONTROLLER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <cstdint>

// LEGO Hub port definitions
#define PORT_MOTOR         0x32
#define PORT_COLOR         0x33
#define PORT_BATTERY       0x35
#define PORT_SPEED_SENSOR  0x36
#define PORT_LIGHT         0x34

// BLE UUIDs
#define GATT_ID "00001623-1212-efde-1623-785feabcd123"

// Callback types
typedef void (*SensorDataCallback)(uint8_t port, const uint8_t* data, size_t length);

class DuploTrainController {
private:
    static BLEUUID serviceUUID;
    static BLEUUID charUUID;
    
    BLERemoteCharacteristic* pRemoteCharacteristic;
    BLEAdvertisedDevice* myDevice;
    BLEClient* pClient;
    
    bool connected;
    bool scanInProgress;
    unsigned long scanStartTime;  // Для управления таймаутом сканирования без блокировки
    bool autoRescanOnDisconnect;  // Автоматически перезапускать сканирование при отвале
    
    // Sensor callbacks
    SensorDataCallback sensorCallback;
    
    // Private helper methods
    bool discoverServices();
    void sendCommand(const uint8_t* command, size_t length);
    
    // Static callback helpers
    static DuploTrainController* instance;
    static void notifyCallbackHelper(
        BLERemoteCharacteristic* pBLERemoteCharacteristic,
        uint8_t* pData,
        size_t length,
        bool isNotify);

public:
    DuploTrainController();
    ~DuploTrainController();
    
    // Connection management
    void init();
    bool connectToHub();
    bool isConnected() const;
    void disconnect();
    void handleScanResults(BLEAdvertisedDevice advertisedDevice);
    
    // BLE Scan control (external)
    void startScan();
    void stopScan();
    void setAutoRescan(bool enabled);  // Включить/выключить автоматическое сканирование при отвале
    bool isDeviceFound() const;  // Проверить, найден ли хаб
    void handleDisconnect();  // Обработка отключения хаба (public для callback)
    
    // Motor control
    void setMotorSpeed(uint8_t port, int8_t speed);
    void stopMotor(uint8_t port);
    void emergencyStop();
    
    // Light and sound
    void doHorn();
    void setLightColor(uint8_t color_code);
    
    // Sensor subscriptions
    void subscribeToColorSensor();
    void subscribeToSpeedSensor();
    void subscribeToBatterySensor();
    void subscribeToAllSensors();
    
    // Callbacks
    void setSensorCallback(SensorDataCallback callback);
    void onNotifyData(uint8_t* pData, size_t length);
    
    // Utilities
    void update();
    void printStatus() const;
    static DuploTrainController* getInstance();
};

#endif // DUPLO_TRAIN_CONTROLLER_H
