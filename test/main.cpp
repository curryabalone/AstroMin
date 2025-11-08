#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "Adafruit_ADXL375.h"

// ==== SD Pins ====
#define SD_CS 4

// ==== HIGH G IMU Pins ======
#define HIGH_G_IMU_PIN 27

// ==== Logging Parameters ====
#define LOG_FREQ_HZ 1000
#define QUEUE_LENGTH 2000
#define CHUNK_SIZE 400

// ==== Data Struct ====
struct LogData {
  uint32_t timestamp;
  float x;
  float y;
  float z;
};

// ==== FreeRTOS Handles ====
QueueHandle_t logQueue;
TaskHandle_t sdTaskHandle;
TaskHandle_t HIGH_G_IMU_TaskHandle;
hw_timer_t* hwTimer = NULL;

// ==== SD ====
File dataFile;

// ==== ISR: runs at 1 kHz on Core 0 ====
void IRAM_ATTR HIGH_G_IMU_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(HIGH_G_IMU_TaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}
// ==== HIGH G IMU Task: runs on Core 0 ====
void Task_Log_High_G_IMU(void* parameter){
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

// ==== SD Task: runs on Core 1 ====
void TaskSD(void* parameter) {
  LogData buffer[CHUNK_SIZE];
  int count = 0;

  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    vTaskDelete(NULL);
  }

  dataFile = SD.open("/data_log.csv", FILE_WRITE);
  if (!dataFile) {
    Serial.println("File open failed!");
    vTaskDelete(NULL);
  }

  for (;;) {
    if (xQueueReceive(logQueue, &buffer[count], pdMS_TO_TICKS(10)) == pdTRUE) {
      count++;
    }

    // Flush once buffer full or after timeout
    if (count >= CHUNK_SIZE) {
      for (int i = 0; i < count; i++) {
        dataFile.printf("%lu,%.3f\n", buffer[i].timestamp, buffer[i].value);
      }
      dataFile.flush();
      count = 0;
    }
  }
}



void setup() {
  Serial.begin(115200);
  pinMode(HIGH_G_IMU_PIN, INPUT);

  // Create queue for samples
  logQueue = xQueueCreate(QUEUE_LENGTH, sizeof(LogData));
  if (!logQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  // Create SD writer task on core 1
  xTaskCreatePinnedToCore(TaskSD, "SDWriter", 16384, NULL, 1, &sdTaskHandle, 1);
  xTaskCreatePinnedToCore(Task_Log_High_G_IMU, "", 5000, NULL, 1, &HIGH_G_IMU_TaskHandle, 0); //change stack size if needed later

  // ==== Configure hardware timer on core 0 ====
  hwTimer = timerBegin(0, 80, true); // 80 prescaler → 1 µs ticks at 80 MHz APB
  timerAlarmWrite(hwTimer, 500, true); // 1000 µs = 1 kHz for running the scheduler
  timerAlarmEnable(hwTimer);

  attachInterrupt(HIGH_G_IMU_PIN, &HIGH_G_IMU_ISR, true);

  Serial.println("Logging started at 1 kHz");
}

void loop() {
  // Nothing here — all real-time work handled by ISR + FreeRTOS task
}