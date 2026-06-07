#include <Arduino.h>
#include <Wire.h>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#include "pins.h"
#include "static_face.h"  // Idle face (static_face_static)
#include "booped_face.h"  // Booped face (booped_face)
#include "talking.h"      // Talking face (talking)
#include <AnimatedGIF.h>

// --- APDS9960 Raw I2C Registers ---
#define APDS9960_I2C_ADDR   0x39
#define APDS9960_REG_ENABLE 0x80
#define APDS9960_REG_PDATA  0x9C

// --- Sound Sensor & Calibration LED Pins ---
#define SOUND_SENSOR_PIN    37
#define CALIBRATION_LED_PIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;
AnimatedGIF talkingGif;

// State Machine tracking
enum DisplayState { STATE_IDLE, STATE_TALKING, STATE_BOOPED, STATE_COOLDOWN };
DisplayState currentState = STATE_IDLE;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 100;

bool isBooped = false;                     
unsigned long cooldownStartTime = 0;       

// Sound tracking
bool isTalking = false;
unsigned long lastSoundTime = 0;
const unsigned long talkingHoldWindow = 150; // Keeps mouth open for 150ms after a sound peak to prevent flickering

// Helper function to read raw proximity data
uint8_t readProximity() {
    Wire.beginTransmission(APDS9960_I2C_ADDR);
    Wire.write(APDS9960_REG_PDATA);
    if (Wire.endTransmission() != 0) return 0;

    Wire.requestFrom(APDS9960_I2C_ADDR, 1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

void GIFDraw(GIFDRAW *pDraw) {
    uint8_t *s = pDraw->pPixels;
    uint16_t *usPalette = pDraw->pPalette;
    uint16_t usTemp[128];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > 128) iWidth = 128;
    y = pDraw->iY + pDraw->y;

    if (pDraw->ucDisposalMethod == 2) {
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) {
                s[x] = pDraw->ucBackground;
            }
        }
        pDraw->ucHasTransparency = 0;
    }

    if (pDraw->ucHasTransparency) {
        uint8_t *pEnd = s + iWidth;
        uint8_t c;
        uint8_t ucTransparent = pDraw->ucTransparent;
        int xPos = 0;

        while (xPos < iWidth) {
            int count = 0;
            c = ucTransparent - 1;
            while (c != ucTransparent && s < pEnd) {
                c = *s++;
                if (c == ucTransparent) {
                    s--;
                } else {
                    usTemp[count++] = usPalette[c];
                }
            }
            if (count) {
                for (int i = 0; i < count; i++) {
                    dma_display->drawPixel(pDraw->iX + xPos + i, y, usTemp[i]);
                }
                xPos += count;
            }
            count = 0;
            c = ucTransparent;
            while (c == ucTransparent && s < pEnd) {
                c = *s++;
                if (c == ucTransparent) {
                    count++;
                } else {
                    s--;
                }
            }
            xPos += count;
        }
    } else {
        for (x = 0; x < iWidth; x++) {
            dma_display->drawPixel(pDraw->iX + x, y, usPalette[*s++]);
        }
    }
}

void playTalkingAnimation() {
    if (talkingGif.openFLASH((uint8_t *)talking, sizeof(talking), GIFDraw)) {
        while (talkingGif.playFrame(true, NULL)) {
            // play the GIF file frame by frame
        }
        talkingGif.close();
    } else {
        dma_display->clearScreen();
        dma_display->drawRGBBitmap(0, 0, static_face_static, 128, 32);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); 

    // 1. Initialize Sound Sensor & Calibration LED
    pinMode(SOUND_SENSOR_PIN, INPUT);
    pinMode(CALIBRATION_LED_PIN, OUTPUT);
    digitalWrite(CALIBRATION_LED_PIN, LOW);

    // 2. Initialize Raw I2C Proximity Sensor
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.beginTransmission(APDS9960_I2C_ADDR);
    Wire.write(APDS9960_REG_ENABLE); 
    Wire.write(0x05);
    Wire.endTransmission();
    
    // 3. Initialize HUB75 Setup
    HUB75_I2S_CFG::i2s_pins pins = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };
    HUB75_I2S_CFG mxconfig(64, 32, 2, pins); 
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness8(80);
    
    // 4. Draw Initial Static Idle Face
    dma_display->clearScreen();
    dma_display->drawRGBBitmap(0, 0, static_face_static, 128, 32);
}

void loop() {
    unsigned long currentMillis = millis();

    // --- 1. SOUND SENSOR LOGIC (With Anti-Flicker) ---
    if (digitalRead(SOUND_SENSOR_PIN) == HIGH) {
        digitalWrite(CALIBRATION_LED_PIN, HIGH);
        lastSoundTime = currentMillis;
        isTalking = true;
    } else {
        digitalWrite(CALIBRATION_LED_PIN, LOW);
        // Turn off talking flag only if no sound has been detected for 150ms
        if (currentMillis - lastSoundTime > talkingHoldWindow) {
            isTalking = false;
        }
    }

    // --- 2. PROXIMITY SENSOR POLLING (Every 100ms) ---
    if (currentMillis - lastSensorRead >= sensorInterval) {
        lastSensorRead = currentMillis;
        uint8_t proximity = readProximity();
        isBooped = (proximity > 60); 
    }

    // --- 3. DISPLAY STATE ENGINE ---
    switch (currentState) {
        
        case STATE_IDLE:
            if (isBooped) {
                dma_display->clearScreen();
                dma_display->drawRGBBitmap(0, 0, booped_face, 128, 32);
                currentState = STATE_BOOPED;
            } else if (isTalking) {
                dma_display->clearScreen();
                playTalkingAnimation();
                currentState = STATE_TALKING;
            }
            break;

        case STATE_TALKING:
            if (isBooped) {
                // Boop overrides talking
                dma_display->clearScreen();
                dma_display->drawRGBBitmap(0, 0, booped_face, 128, 32);
                currentState = STATE_BOOPED;
            } else if (!isTalking) {
                // Revert to idle face
                dma_display->clearScreen();
                dma_display->drawRGBBitmap(0, 0, static_face_static, 128, 32);
                currentState = STATE_IDLE;
            }
            break;

        case STATE_BOOPED:
            if (!isBooped) {
                cooldownStartTime = currentMillis;
                currentState = STATE_COOLDOWN;
            }
            break;

        case STATE_COOLDOWN:
            if (isBooped) {
                currentState = STATE_BOOPED;
            } 
            else if (currentMillis - cooldownStartTime >= 1000) {
                // Determine which state to return to after the 1s cooldown
                dma_display->clearScreen();
                if (isTalking) {
                    playTalkingAnimation();
                    currentState = STATE_TALKING;
                } else {
                    dma_display->drawRGBBitmap(0, 0, static_face_static, 128, 32);
                    currentState = STATE_IDLE;
                }
            }
            break;
    }
}