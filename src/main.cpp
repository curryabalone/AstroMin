#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// ==== SD Pins ====
#define SD_CS 4

// ==== Logging Parameters ====
#define LOG_FREQ_HZ 1000
#define QUEUE_LENGTH 2000
#define CHUNK_SIZE 400

// ==== Data Struct ====
struct LogData {
  uint32_t timestamp;
  float value;
};

// ==== FreeRTOS Handles ====
QueueHandle_t logQueue;
TaskHandle_t sdTaskHandle;
hw_timer_t* hwTimer = NULL;

// ==== SD ====
File dataFile;

// ==== ISR: runs at 1 kHz on Core 0 ====
void IRAM_ATTR onTimer() {
  static LogData sample;
  sample.value = micros()-sample.timestamp;
  sample.timestamp = micros();

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(logQueue, &sample, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
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
  pinMode(A0, INPUT);

  // Create queue for samples
  logQueue = xQueueCreate(QUEUE_LENGTH, sizeof(LogData));
  if (!logQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  // Create SD writer task on core 1
  xTaskCreatePinnedToCore(TaskSD, "SDWriter", 16384, NULL, 1, &sdTaskHandle, 1);

  // ==== Configure hardware timer on core 0 ====
  hwTimer = timerBegin(0, 80, true); // 80 prescaler → 1 µs ticks at 80 MHz APB
  timerAttachInterrupt(hwTimer, &onTimer, true);
  timerAlarmWrite(hwTimer, 500, true); // 1000 µs = 1 kHz
  timerAlarmEnable(hwTimer);

  Serial.println("Logging started at 1 kHz");
}

void loop() {
  // Nothing here — all real-time work handled by ISR + FreeRTOS task
}