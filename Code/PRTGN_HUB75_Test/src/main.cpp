#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "PINS.h"
#include "my_gif.h"
#include <AnimatedGIF.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;
AnimatedGIF gif;

// --- GIF Drawing Callback ---
// The AnimatedGIF library calls this function for every line of pixels it decodes
void GIFDraw(GIFDRAW *pDraw) {
    uint8_t *s;
    uint16_t *usPalette;
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > 128) iWidth = 128; // Enforce max width

    usPalette = pDraw->pPalette;
    
    // CORRECTED: Add frame offset (iY) to current line (y)
    y = pDraw->iY + pDraw->y; 

    s = pDraw->pPixels;
    int xOffset = pDraw->iX;

    if (pDraw->ucHasTransparency) {
        uint8_t ucTransparent = pDraw->ucTransparent;
        for (x = 0; x < iWidth; x++) {
            if (s[x] != ucTransparent) {
                dma_display->drawPixel(xOffset + x, y, usPalette[s[x]]);
            }
        }
    } else {
        for (x = 0; x < iWidth; x++) {
            dma_display->drawPixel(xOffset + x, y, usPalette[s[x]]);
        }
    }
}

void setup() {
    Serial.begin(115200);

    HUB75_I2S_CFG::i2s_pins pins = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };

    HUB75_I2S_CFG mxconfig(64, 32, 1, pins);
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness8(80);
    dma_display->clearScreen();

    // Initialize the GIF decoder
    gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
    // Open the GIF from the PROGMEM array
    if (gif.open((uint8_t *)test_gif, test_gif_size, GIFDraw)) {
        Serial.println("Playing GIF...");
        
        // playFrame() decodes and delays based on the GIF's built-in timing
        while (gif.playFrame(true, NULL)) {
            // Let the ESP32 handle background WiFi/Watchdog tasks
            yield(); 
        }
        gif.close();
    } else {
        Serial.println("Error opening GIF!");
        delay(1000);
    }
}