#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "SD.h"
#include "SPI.h"
#include "esp_timer.h"

#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK 18
#define SD_CS 4


const char* FILE_NAME = "/loggingdata.csv";
const int BUFFER_SIZE = 1000;
File datafile;
struct logbuffer{
    uint32_t time;
    float imu_x;
};

logbuffer buffer[BUFFER_SIZE];
int buffer_index = 0;

void setup(){
    Serial.begin(115200);
    // Initialize SD card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("SD card initialization failed!");
        return;
    }
    Serial.println("SD card initialized.");
    datafile = SD.open(FILE_NAME, FILE_WRITE);
    if(!datafile){
        Serial.println("Failed to open file");
        return;
    }
    datafile.println("Start");
    datafile.flush();
}

uint32_t prev_time = 0;
void loop() {
    datafile.println(millis() - prev_time);
    prev_time = millis();
    datafile.flush();
}