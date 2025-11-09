#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "Adafruit_ADXL375.h"
#include <Adafruit_BMP3XX.h>

// ==== Test Pins =====
#define TESTPIN 32

// ==== SD Pins ====
#define SD_CS 4

//ADXL375 Setup
TwoWire I2CBus = TwoWire(0);  // Use default I²C bus
Adafruit_ADXL375 accel(12345, &I2CBus);
#define ADXL375_SDA 33
#define ADXL375_SCL 25

//MPU6050 and BMP390 Setup
TwoWire I2CBUS2 = TwoWire(1);
Adafruit_BMP3XX bmp;
#define Wire1_SDA 27
#define Wire1_SCL 26

// ==== Logging Parameters ====
#define LOG_FREQ_HZ 3200
#define QUEUE_LENGTH 4000
#define CHUNK_SIZE 400

// ==== Data Struct ====
struct LogData {
  uint32_t timestamp;
  uint8_t data_type; //1 for ADXL375 data, 2 for barometer data
  float x;
  float y;
  float z;
};

// ==== FreeRTOS Handles ====
QueueHandle_t logQueue;
TaskHandle_t sdTaskHandle;
TaskHandle_t Log_ADXL375_Handle;
TaskHandle_t Log_BMP_Handle;
hw_timer_t* hwTimer_ADX375 = NULL;
hw_timer_t* hwTimer_BMP = NULL;

// ==== SD ====
File dataFile;

// ====IMU ISR: runs at 3.2 kHz on Core 0 ====
void IRAM_ATTR HIGH_G_IMU_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(Log_ADXL375_Handle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// ====BMP ISR ====
// void IRAM_ATTR BMP_ISR() {
//   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//   vTaskNotifyGiveFromISR(Log_BMP_Handle, &xHigherPriorityTaskWoken);
//   if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
// }

// ==== HIGH G IMU Task: runs on Core 0 ====
void Task_Log_ADXL375(void* parameter){
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    static LogData data;
    data.timestamp = micros();
    sensors_event_t event;
    data.data_type = 1;
    accel.getEvent(&event);
    data.x = event.acceleration.x;
    data.y = event.acceleration.y;
    data.z = event.acceleration.z;
    xQueueSend(logQueue, &data, 0);
  }
}

// === BMP Task: runs on Core 1 ===== 
void Task_Log_BMP(void* parameter){
  for(;;){
    static LogData data;
    data.timestamp = micros();
    if (bmp.performReading()){
      data.data_type = 2;
      data.x = bmp.temperature;
      data.y = bmp.pressure;
      data.z = bmp.readAltitude(1013.25);
      xQueueSend(logQueue, &data, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
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
        dataFile.printf("%lu,%d,%.3f,%.3f,%.3f\n",
                buffer[i].timestamp, buffer[i].data_type,
                buffer[i].x, buffer[i].y, buffer[i].z);
      }
      dataFile.flush();
      count = 0;
      vTaskDelay(1); 
    }
  }
}



void setup() {
  Serial.begin(115200);
  delay(2000);

  //ADXL375 Initialization
  I2CBus.begin(ADXL375_SDA, ADXL375_SCL, 1000000); // SDA, SCL, clock
  if(!accel.begin()){
    Serial.println("Could not find ADXL375. Check address or wiring!");
    delay(200);
  }
  accel.setDataRate(ADXL3XX_DATARATE_3200_HZ);
  accel.setTrimOffsets(-(12+2)/4,  //change offsets for each new board
                       -(1+2)/4, 
                       -(15-20+2)/4); 
  Serial.println("ADXL375 initialized!");
  
  //BMP Initialization
  I2CBUS2.begin(Wire1_SDA, Wire1_SCL, 100000);
  if(!bmp.begin_I2C(0x77, &I2CBUS2)){
    Serial.println("Could not find a valid BMP390 sensor, check wiring or address!");
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);  // about 50 Hz output rate

  delay(100);
  
  // Create queue for samples
  logQueue = xQueueCreate(QUEUE_LENGTH, sizeof(LogData));
  if (!logQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  // Pin tasks to cores
  xTaskCreatePinnedToCore(TaskSD, "SDWriter", 16384, NULL, 1, &sdTaskHandle, 1);
  xTaskCreatePinnedToCore(Task_Log_ADXL375, "", 5000, NULL, 2, &Log_ADXL375_Handle, 0); //change stack size if needed later
  xTaskCreatePinnedToCore(Task_Log_BMP, "BMP", 5000, NULL, 1, &Log_BMP_Handle, 1);  // now runs on Core 1


  // ==== Configure hardware timer on core 0 ====
  hwTimer_ADX375 = timerBegin(0, 80, true); 
  timerAttachInterrupt(hwTimer_ADX375, &HIGH_G_IMU_ISR, true);
  timerAlarmWrite(hwTimer_ADX375, 312, true); 
  timerAlarmEnable(hwTimer_ADX375);
  // hwTimer_BMP = timerBegin(1, 80, true);
  // timerAttachInterrupt(hwTimer_BMP, &BMP_ISR, true);
  // timerAlarmWrite(hwTimer_BMP, 20000, true);
  // timerAlarmEnable(hwTimer_BMP);
}

void loop() {
  // Nothing here — all real-time work handled by ISR + FreeRTOS task
}