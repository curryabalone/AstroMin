#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "Adafruit_ADXL375.h"
#include <Wire.h>

Adafruit_ADXL375 IMU(12345); //garbage identifier field

void setup(){
    Serial.begin(115200);
    Wire.begin();
    if(!IMU.begin(0x53)){ //defualts to Wire on ESP32
        while(true){
            Serial.println("Failed to initialize ADX375");
            delay(500);
        }
    }
    delay(500);
    Serial.println("Initialized IMU");
}

void loop(){
    Serial.println("hello, world");
    delay(100);
}