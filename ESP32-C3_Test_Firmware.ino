/*
 * ESP32-C3 Test Firmware for Button & Potentiometer Control
 * 
 * Hardware Configuration:
 * - Digital Inputs: GPIO 0, 1, 2, 3 (Buttons 1-4)
 * - Analog Input: GPIO 4 (Potentiometer)
 * - Digital Output: GPIO 5 (Power Relay)
 * 
 * Features:
 * - Detects button presses and logs to console
 * - Monitors potentiometer changes
 * - Auto-disables relay after 5 minutes of inactivity
 * - USB Serial output for debugging
 */

// Pin Configuration
const int BUTTON_PINS[4] = {0, 1, 2, 3};          // GPIO pins for 4 buttons
const int POTENTIOMETER_PIN = 4;                   // GPIO pin for analog potentiometer
const int RELAY_PIN = 5;                           // GPIO pin for power relay

// Timing Configuration
const unsigned long INACTIVITY_TIMEOUT = 5 * 60 * 1000;  // 5 minutes in milliseconds

// Button debounce configuration
const int DEBOUNCE_DELAY = 50;                     // milliseconds
const int POTENTIOMETER_THRESHOLD = 20;            // ADC value change threshold

// State Variables
unsigned long lastEventTime = 0;                   // Timestamp of last event
bool relayActive = true;                           // Current relay state
int lastPotentiometerValue = 0;                    // Previous potentiometer reading
bool buttonStates[4] = {false, false, false, false};  // Current button states
bool prevButtonStates[4] = {false, false, false, false};  // Previous button states

void setup() {
  // Initialize Serial for console output
  Serial.begin(115200);
  delay(2000);  // Wait for serial connection to stabilize
  
  Serial.println("\n\n================================");
  Serial.println("ESP32-C3 Test Firmware Started");
  Serial.println("================================\n");
  
  // Initialize button pins as inputs
  for (int i = 0; i < 4; i++) {
    pinMode(BUTTON_PINS[i], INPUT);
    Serial.printf("Button %d configured on GPIO %d\n", i + 1, BUTTON_PINS[i]);
  }
  
  // Initialize potentiometer pin as input
  pinMode(POTENTIOMETER_PIN, INPUT);
  Serial.printf("Potentiometer configured on GPIO %d\n", POTENTIOMETER_PIN);
  
  // Initialize relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Relay active (HIGH = ON)
  Serial.printf("Relay configured on GPIO %d (ACTIVE)\n", RELAY_PIN);
  
  // Initialize last event time
  lastEventTime = millis();
  lastPotentiometerValue = analogRead(POTENTIOMETER_PIN);
  
  Serial.println("\nSystem ready! Waiting for events...\n");
}

void loop() {
  // Check for button events
  checkButtons();
  
  // Check for potentiometer changes
  checkPotentiometer();
  
  // Check for inactivity timeout
  checkInactivityTimeout();
  
  // Small delay to reduce CPU usage
  delay(10);
}

/*
 * Check for button press events
 */
void checkButtons() {
  for (int i = 0; i < 4; i++) {
    buttonStates[i] = digitalRead(BUTTON_PINS[i]);
    
    // Detect button press (transition from LOW to HIGH)
    if (buttonStates[i] == HIGH && prevButtonStates[i] == LOW) {
      delay(DEBOUNCE_DELAY);  // Debounce
      
      // Confirm press after debounce
      if (digitalRead(BUTTON_PINS[i]) == HIGH) {
        onButtonPress(i + 1);
        recordEvent();
      }
    }
    
    prevButtonStates[i] = buttonStates[i];
  }
}

/*
 * Handle button press event
 */
void onButtonPress(int buttonNumber) {
  Serial.printf(">>> BUTTON %d PRESSED at %lu ms\n", buttonNumber, millis());
}

/*
 * Check for potentiometer value changes
 */
void checkPotentiometer() {
  int currentValue = analogRead(POTENTIOMETER_PIN);
  int difference = abs(currentValue - lastPotentiometerValue);
  
  if (difference > POTENTIOMETER_THRESHOLD) {
    onPotentiometerChange(currentValue);
    lastPotentiometerValue = currentValue;
    recordEvent();
  }
}

/*
 * Handle potentiometer change event
 */
void onPotentiometerChange(int value) {
  // Scale ADC value (0-4095) to percentage (0-100%)
  int percentage = map(value, 0, 4095, 0, 100);
  Serial.printf(">>> POTENTIOMETER CHANGED: %d (ADC) / %d%% at %lu ms\n", 
                value, percentage, millis());
}

/*
 * Record that an event occurred (update last event timestamp)
 */
void recordEvent() {
  lastEventTime = millis();
  
  // Ensure relay is active
  if (!relayActive) {
    relayActive = true;
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("*** RELAY REACTIVATED ***\n");
  }
}

/*
 * Check for inactivity timeout and disable relay if needed
 */
void checkInactivityTimeout() {
  unsigned long currentTime = millis();
  unsigned long timeSinceLastEvent = currentTime - lastEventTime;
  
  if (relayActive && timeSinceLastEvent >= INACTIVITY_TIMEOUT) {
    disableRelay();
  }
}

/*
 * Disable the relay (power cutoff)
 */
void disableRelay() {
  relayActive = false;
  digitalWrite(RELAY_PIN, LOW);  // Relay inactive (LOW = OFF)
  
  unsigned long minutesSinceEvent = (millis() - lastEventTime) / (60 * 1000);
  Serial.printf("\n!!! INACTIVITY TIMEOUT !!!\n");
  Serial.printf("No events detected for %lu minutes\n", minutesSinceEvent);
  Serial.printf("RELAY DEACTIVATED at %lu ms\n\n", millis());
}
