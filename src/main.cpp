#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "Adafruit_ADXL375.h"
#include <Adafruit_MPU6050.h>

// ==== Test Pins =====
#define TESTPIN 32

// ==== SD Pins ====
#define SD_CS 4

//ADXL375 Setup
TwoWire I2CBus = TwoWire(0);  // Use default I²C bus
Adafruit_ADXL375 accel(12345, &I2CBus);
float gyro_x_offset = -0.027746;
float gyro_y_offset = 0.061322;
float gyro_z_offset = 0.004399;
#define ADXL375_SDA 33
#define ADXL375_SCL 25

//MPU6050 and BMP390 Setup
TwoWire I2CBUS2 = TwoWire(1);
Adafruit_MPU6050 gyroscope;
#define Wire1_SDA 27
#define Wire1_SCL 26

// ==== Logging Parameters ====
#define LOG_FREQ_HZ 3200
#define QUEUE_LENGTH 2000
#define CHUNK_SIZE 400

// ==== Data Struct ====
struct LogData {
  uint32_t timestamp;
  char data_type; //1 for ADXL375 data, 2 for MPU9250 data, 3 for barometer data
  float x;
  float y;
  float z;
};

// ==== FreeRTOS Handles ====
QueueHandle_t logQueue;
TaskHandle_t sdTaskHandle;
TaskHandle_t Log_ADXL375_Handle;
TaskHandle_t Log_Gyroscope_Handle;
hw_timer_t* hwTimer = NULL;
hw_timer_t* hwTimer_ADX375 = NULL;
hw_timer_t* hwTimer_BMP = NULL;
hw_timer_t* hwTimer_Gyroscope = NULL;

// ==== SD ====
File dataFile;

// ====IMU ISR: runs at 3.2 kHz on Core 0 ====
void IRAM_ATTR HIGH_G_IMU_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(Log_ADXL375_Handle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void IRAM_ATTR GYROSCOPE_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(Log_Gyroscope_Handle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

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

void Task_Log_Gyroscope(void* parameter){
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    static LogData data;
    sensors_event_t garbage1, gyro, garbage2;
    gyroscope.getEvent(&garbage1, &gyro, &garbage2);
    data.timestamp = micros();
    data.x = gyro.gyro.x - gyro_x_offset;
    data.y = gyro.gyro.y - gyro_y_offset;
    data.z = gyro.gyro.z - gyro_z_offset;
    xQueueSend(logQueue, &data, 0);
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
  pinMode(TESTPIN, OUTPUT);

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
  
  //MPU6050 Initialization
  I2CBUS2.begin(Wire1_SDA, Wire1_SCL, 1000000);
  if(!gyroscope.begin(0x68, &I2CBUS2)){
    Serial.println("Failed to find mpu6050");
  }
  gyroscope.setGyroRange(MPU6050_RANGE_2000_DEG);
    // --- Tell the sensor to output data at 2 kHz ---
  I2CBUS2.beginTransmission(0x68);
  I2CBUS2.write(0x1A);      // CONFIG register
  I2CBUS2.write(0x00);      // DLPF = 0 → 8 kHz internal
  I2CBUS2.endTransmission();

  I2CBUS2.beginTransmission(0x68);
  I2CBUS2.write(0x19);      // SMPLRT_DIV
  I2CBUS2.write(0x03);      // 8 kHz / (1 + 3) = 2 kHz
  I2CBUS2.endTransmission();
  Serial.println("MPU6050 initialized");

  // Create queue for samples
  logQueue = xQueueCreate(QUEUE_LENGTH, sizeof(LogData));
  if (!logQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  // Create SD writer task on core 1
  xTaskCreatePinnedToCore(TaskSD, "SDWriter", 16384, NULL, 1, &sdTaskHandle, 1);
  xTaskCreatePinnedToCore(Task_Log_ADXL375, "", 5000, NULL, 2, &Log_ADXL375_Handle, 0); //change stack size if needed later
  xTaskCreatePinnedToCore(Task_Log_Gyroscope, "", 5000, NULL, 1, &Log_Gyroscope_Handle, 0);

  // ==== Configure hardware timer on core 0 ====
  hwTimer_ADX375 = timerBegin(0, 80, true); 
  timerAttachInterrupt(hwTimer_ADX375, &HIGH_G_IMU_ISR, true);
  timerAlarmWrite(hwTimer_ADX375, 312, true); 
  timerAlarmEnable(hwTimer_ADX375);

  hwTimer_Gyroscope = timerBegin(1, 80, true);
  timerAttachInterrupt(hwTimer_Gyroscope, &GYROSCOPE_ISR, true);
  timerAlarmWrite(hwTimer_Gyroscope, 500, true);
  timerAlarmEnable(hwTimer_Gyroscope);

}

void loop() {
  // Nothing here — all real-time work handled by ISR + FreeRTOS task
}