#include <Arduino.h>
#include <Wire.h>

// Custom I2C pins for ESP32
#define I2C_SDA_PIN 27
#define I2C_SCL_PIN 26

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("\nI2C Scanner starting...");

  // Initialize I2C with your chosen pins
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);  // 100 kHz for stability
  delay(100);
}

void scanI2C() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning I2C bus...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      char buf[32];
      sprintf(buf, "I2C device found at 0x%02X (%d)", address, address);
      Serial.println(buf);
      nDevices++;
    } 
    else if (error == 4) {
      char buf[32];
      sprintf(buf, "Unknown error at address 0x%02X", address);
      Serial.println(buf);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.printf("%d device(s) found\n\n", nDevices);
  }
}

void loop() {
  scanI2C();
  delay(5000);  // rescan every 5s
}
