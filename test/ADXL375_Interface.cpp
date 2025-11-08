#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL375.h>


TwoWire I2CBus = TwoWire(0);  // Use default I²C bus
Adafruit_ADXL375 accel(12345, &I2CBus);
void setup() {
  Serial.begin(115200);
  delay(2000); // Give the serial time to attach
  Serial.println("Starting ADXL375 test...");

  // Explicitly start the I2C bus on ESP32 default pins
  I2CBus.begin(33, 25, 400000); // SDA, SCL, clock
  accel.setDataRate(ADXL3XX_DATARATE_3200_HZ);
  

  // Explicit address + Wire bus
  if (!accel.begin()) {
    Serial.println("Could not find ADXL375. Check address or wiring!");
    while (1) delay(100);
  }

  Serial.println("ADXL375 initialized!");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);
  delayMicroseconds(10);
}
