#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
  uint32_t time_ms = millis();
  printf("Time since start: %lu ms\n", time_ms);
  delay(1000); // Delay for 1 second
}
 