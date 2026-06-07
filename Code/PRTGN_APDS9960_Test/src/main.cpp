#include <Arduino.h>
#include <Wire.h>
#include "PINS.h"

#include <SPI.h> // Keep this if PlatformIO complains, otherwise you can remove it

#define APDS9960_I2C_ADDR  0x39
#define APDS9960_REG_ENABLE 0x80
#define APDS9960_REG_PDATA  0x9C

unsigned long lastReadTime = 0;
const unsigned long readInterval = 100; // Read 10 times a second

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for native USB CDC
  Serial.println("Starting Custom APDS9960 Driver...");

  // 1. Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // 2. Enable Power (PON = bit 0) and Proximity (PEN = bit 2)
  // 0x01 (PON) + 0x04 (PEN) = 0x05
  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(APDS9960_REG_ENABLE); 
  Wire.write(0x05); 
  if (Wire.endTransmission() == 0) {
    Serial.println("Sensor successfully configured via Raw I2C!");
  } else {
    Serial.println("Failed to connect to sensor over I2C.");
    while(1);
  }
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastReadTime >= readInterval) {
    lastReadTime = currentMillis;

    // 3. Point to the Proximity Data Register
    Wire.beginTransmission(APDS9960_I2C_ADDR);
    Wire.write(APDS9960_REG_PDATA);
    Wire.endTransmission();

    // 4. Request 1 byte of data
    Wire.requestFrom(APDS9960_I2C_ADDR, 1);
    
    uint8_t proximity = 0;
    if (Wire.available()) { 
      proximity = Wire.read(); // Values: 0 (far) to 255 (touching)
    }

    Serial.print("Proximity Value: ");
    Serial.println(proximity);

    if (proximity > 50) {
      Serial.println("-> Hand Detected!");
    }
  }
}