#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  Serial.begin(115200);            // start USB/CDC serial at 115200 baud
  delay(100);                      // short pause to let the USB enumerate
  Serial.println("Hello from C3 Super Mini");
}

void loop() {
  Serial.print("ADC value: ");
  int v = analogRead(1);           // ADC1_CH0 is GPIO0 on Super Mini
  Serial.println(v);
  delay(100);
}


// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}