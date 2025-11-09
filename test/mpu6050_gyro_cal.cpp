#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

TwoWire I2CBus2 = TwoWire(1);
Adafruit_MPU6050 mpu;

// Number of samples for averaging
const int NUM_SAMPLES = 2000;
double gx_off = 0, gy_off = 0, gz_off = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Gyro Calibration ===");
  Serial.println("Keep the sensor completely still...");

  I2CBus2.begin(27, 26, 400000);

  // Initialize your IMU
  if (!mpu.begin(0x68, &I2CBus2)) {
    Serial.println("Failed to find IMU!");
    while (1);
  }

  // Use widest range for best bias capture
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(100);

  // Accumulate samples
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    gx_off += gyro.gyro.x;
    gy_off += gyro.gyro.y;
    gz_off += gyro.gyro.z;

    if (i % 200 == 0) Serial.printf("Collecting sample %d / %d\n", i, NUM_SAMPLES);
    delay(2); // ~500 Hz sample collection
  }

  gx_off /= NUM_SAMPLES;
  gy_off /= NUM_SAMPLES;
  gz_off /= NUM_SAMPLES;

  Serial.println("\n=== Calibration Complete ===");
  Serial.println("Gyro bias (rad/s):");
  Serial.printf("  X offset = %.6f\n", gx_off);
  Serial.printf("  Y offset = %.6f\n", gy_off);
  Serial.printf("  Z offset = %.6f\n", gz_off);

  Serial.println("\nNow subtract these offsets from future gyro readings!");
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Apply offsets
  gyro.gyro.x -= gx_off;
  gyro.gyro.y -= gy_off;
  gyro.gyro.z -= gz_off;
  delay(10000000);
}
