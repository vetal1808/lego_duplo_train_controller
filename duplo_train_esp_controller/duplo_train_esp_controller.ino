#include "DuploTrainController.h"

// Create global controller instance
DuploTrainController trainController;

// State machine для управления подключением
enum State {
    STATE_INIT,
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_DEMO_RUNNING
};

State currentState = STATE_INIT;
unsigned long demoStartTime = 0;
bool demoRunning = false;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n===========================================");
    Serial.println("🚂 LEGO DUPLO Train Controller for ESP32");
    Serial.println("===========================================\n");

    // Initialize controller (non-blocking)
    trainController.init();
    
    // Enable auto-rescan on disconnect
    trainController.setAutoRescan(true);
    
    // Start scanning for hub
    trainController.startScan();
    
    currentState = STATE_SCANNING;
}

void loop() {
    // ВСЕГДА вызывать update() для обработки сканирования и таймаутов
    trainController.update();
    
    // State machine для управления подключением без блокировок
    switch (currentState) {
        case STATE_SCANNING:
            // Ждём пока девайс будет найден (update() обработает это)
            if (trainController.isDeviceFound()) {
                currentState = STATE_CONNECTING;
                Serial.println("\n→ Attempting to connect to Hub...\n");
            }
            break;
            
        case STATE_CONNECTING:
            // Попытка подключения
            if (trainController.connectToHub()) {
                Serial.println("✅ Successfully connected to LEGO Hub!\n");
                currentState = STATE_CONNECTED;
                demoStartTime = millis();
                demoRunning = false;
            } else {
                // Ошибка подключения - вернуться к сканированию
                Serial.println("❌ Connection failed. Restarting scan...\n");
                trainController.startScan();
                currentState = STATE_SCANNING;
            }
            break;
            
        case STATE_CONNECTED:
            // Подключены - начинаем демо
            if (!demoRunning) {
                Serial.println("⏳ Starting demo sequence...\n");
                trainController.subscribeToAllSensors();
                demoRunning = true;
                demoStartTime = millis();
            } else {
                runDemoSequence();
            }
            break;
            
        case STATE_DEMO_RUNNING:
            // Демо выполняется
            break;
            
        case STATE_INIT:
            // Инициализация завершена, перейти к сканированию
            currentState = STATE_SCANNING;
            break;
    }
}

// Demo sequence (non-blocking, time-based)
void runDemoSequence() {
    if (!trainController.isConnected()) {
        Serial.println("⚠️  Not connected!");
        demoRunning = false;
        return;
    }
    
    unsigned long elapsed = millis() - demoStartTime;
    
    // Используем временные интервалы вместо delay() для асинхронной работы
    if (elapsed < 2000) {
        // Forward at 50%
        if (elapsed == 0) {
            Serial.println("→ Forward at 50% speed...");
            trainController.setMotorSpeed(PORT_MOTOR, 50);
        }
    }
    else if (elapsed < 3000) {
        // Stop
        if (elapsed == 2000) {
            Serial.println("⏹️  Stopping...");
            trainController.stopMotor(PORT_MOTOR);
        }
    }
    else if (elapsed < 4000) {
        // Horn
        if (elapsed == 3000) {
            Serial.println("📯 Horn test...");
            trainController.doHorn();
        }
    }
    else if (elapsed < 5000) {
        // Light color
        if (elapsed == 4000) {
            Serial.println("💡 Light color test (White)...");
            trainController.setLightColor(0xFF);
        }
    }
    else if (elapsed < 7000) {
        // Backward at 50%
        if (elapsed == 5000) {
            Serial.println("← Backward at 50% speed...");
            trainController.setMotorSpeed(PORT_MOTOR, -50);
        }
    }
    else if (elapsed < 8000) {
        // Stop
        if (elapsed == 7000) {
            Serial.println("⏹️  Stopping...");
            trainController.stopMotor(PORT_MOTOR);
        }
    }
    else {
        // Demo complete
        Serial.println("\n✅ Demo sequence complete!\n");
        demoRunning = false;
        currentState = STATE_CONNECTED;
    }
}