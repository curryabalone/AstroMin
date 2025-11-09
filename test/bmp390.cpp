#include <Wire.h>
#include <Adafruit_BMP3XX.h>

// ==== I2C Pins for ESP32 ====
#define BMP_SDA 27
#define BMP_SCL 26

// ==== Create BMP390 object and I2C bus ====
TwoWire I2CBus1 = TwoWire(1);   // use I2C port #1
Adafruit_BMP3XX bmp;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("BMP390 Test");

  // ==== Initialize I2C bus ====
  I2CBus1.begin(BMP_SDA, BMP_SCL, 400000);  // SDA, SCL, 400 kHz

  // ==== Initialize BMP390 via I2C bus ====
  if (!bmp.begin_I2C(0x77, &I2CBus1)) {  // Try 0x76 if SDO pin is GND
    Serial.println("Could not find a valid BMP390 sensor, check wiring or address!");
    while (1) delay(10);
  }

  // ==== Configure oversampling and filter ====
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);  // about 50 Hz output rate

  Serial.println("BMP390 initialized!");
}

void loop() {
  if (bmp.performReading()) {
    Serial.printf("T = %.2f °C,  P = %.2f hPa,  Alt = %.2f m\n",
                  bmp.temperature,
                  bmp.pressure / 100.0,
                  bmp.readAltitude(1013.25));
  } else {
    Serial.println("BMP390 read failed!");
  }
  delay(100);  // print about 10 Hz
}
