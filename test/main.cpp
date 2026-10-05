
#include "ESP32-VirtualMatrixPanel-I2S-DMA.h"
#include "esp32-hal-gpio.h"

#define PANEL_RES_X 64
#define PANEL_RES_Y 32

#define NUM_ROWS 2
#define NUM_COLS 2

#define SERPENT true
#define TOPDOWN false
#define VIRTUAL_MATRIX_CHAIN_TYPE CHAIN_BOTTOM_RIGHT_UP_ZZ
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

class EightPxBasePanel : public VirtualMatrixPanel {
public:
  using VirtualMatrixPanel::VirtualMatrixPanel;

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    for (int16_t i = 0; i < h; i++) {
      drawPixel(x, y + i, color);
    }
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    for (int16_t i = 0; i < w; i++) {
      drawPixel(x + i, y, color);
    }
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    for (int16_t i = x; i < x + w; i++) {
      for (int16_t j = y; j < y + h; j++) {
        drawPixel(i, j, color);
      }
    }
  }

protected:
  VirtualCoords getCoords(int16_t x, int16_t y) override;
};

// define custom getCoords 1/8
inline VirtualCoords EightPxBasePanel ::getCoords(int16_t x, int16_t y) {
  // if (x < 0 || x >= 128 || y < 0 || y >= 64) {
  //   coords.x = coords.y = -1;
  //   return coords;
  // }
  if (x < 0 || x >= (NUM_COLS * PANEL_RES_X) || y < 0 ||
      y >= (NUM_ROWS * PANEL_RES_Y)) {
    coords.x = coords.y = -1;
    return coords;
  }
  const uint8_t panelWidth = PANEL_RES_X;
  const uint8_t panelHeight = PANEL_RES_Y;
  const uint8_t dmaPanelWidth = panelWidth * 2;

  uint8_t gridRow = y / panelHeight;
  uint8_t gridCol = x / panelWidth;

  uint16_t panelIdx = gridRow * NUM_COLS + gridCol;

  uint8_t panelLocalX = x % panelWidth;
  uint8_t panelLocalY = y % panelHeight;

  panelLocalX = panelWidth - 1 - panelLocalX;

  uint16_t panelBase = panelIdx * dmaPanelWidth;
  uint8_t blockX = panelLocalX >> 3;
  uint8_t blockY = panelLocalY >> 3;
  uint8_t localX = panelLocalX & 0x7;
  uint8_t localY = panelLocalY & 0x7;

  static const uint8_t dmaBlockX[4][8] = {{127, 119, 111, 103, 63, 55, 47, 39},
                                          {95, 87, 79, 71, 31, 23, 15, 7},
                                          {127, 119, 111, 103, 63, 55, 47, 39},
                                          {95, 87, 79, 71, 31, 23, 15, 7}};

  static const uint8_t dmaBlockY[4] = {8, 8, 0, 0};

  coords.x = panelBase + dmaBlockX[blockY][blockX] - localX;
  coords.y = dmaBlockY[blockY] + localY;
  return coords;
}

MatrixPanel_I2S_DMA *dma_display = nullptr;

EightPxBasePanel *FourScanPanel = nullptr;

void testled();
void waitNextPoint();
void setup() {
  delay(250);

  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2,    // DO NOT CHANGE THIS
                         PANEL_RES_Y / 2,    // DO NOT CHANGE THIS
                         NUM_ROWS * NUM_COLS // DO NOT CHANGE THIS
  );

  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::ICN2038S;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(10); // range is 0-255

  dma_display->clearScreen();
  delay(500);

  FourScanPanel =
      new EightPxBasePanel((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X,
                           PANEL_RES_Y, VIRTUAL_MATRIX_CHAIN_TYPE);
  FourScanPanel->setFont(&FreeSansBold12pt7b);
}

// void testled() {
//   const int testPoints[][2] = {
//       {0, 0},   {8, 0},   {16, 0},  {24, 0}, {32, 0},  {40, 0},  {48, 0},
//       {56, 0},  {0, 8},   {8, 8},   {16, 8}, {24, 8},  {32, 8},  {40, 8},
//       {48, 8},  {56, 8},  {0, 16},  {8, 16}, {16, 16}, {24, 16}, {32, 16},
//       {40, 16}, {48, 16}, {56, 16}, {0, 24}, {8, 24},  {16, 24}, {24, 24},
//       {32, 24}, {40, 24}, {48, 24}, {56, 24}};

//   for (uint8_t i = 0; i < sizeof(testPoints) / sizeof(testPoints[0]); i++) {
//     FourScanPanel->clearScreen();
//     Serial.printf("SHOW ONLY Logic(%d,%d)\n", testPoints[i][0],
//                   testPoints[i][1]);
//     Serial.println("Send N to show next point.");
//     FourScanPanel->drawPixel(testPoints[i][0], testPoints[i][1],
//                              FourScanPanel->color565(255, 0, 0));
//     waitNextPoint();
//   }
// }

// void waitNextPoint() {
//   while (true) {
//     if (Serial.available()) {
//       char command = Serial.read();
//       if (command == 'N' || command == 'n') {
//         while (Serial.available()) {
//           Serial.read();
//         }
//         return;
//       }
//     }
//     delay(10);
//   }
// }

void loop() {
  // dma_display->clearScreen();

  uint16_t red = FourScanPanel->color565(255, 0, 0);
  uint16_t green = FourScanPanel->color565(0, 255, 0);
  uint16_t yellow = FourScanPanel->color565(255, 255, 0);
  uint16_t cyan = FourScanPanel->color565(0, 255, 255);
  // FourScanPanel->drawRect(0, 0, 128, 64, red);
  // FourScanPanel->drawFastVLine(64, 0, 64, red);
  // FourScanPanel->drawFastHLine(0, 32, 128, red);

  FourScanPanel->setCursor(12, 22);
  FourScanPanel->setTextColor(red);
  FourScanPanel->print("P1");

  // // Panel 2: Top-Right (x: 76, y: 22) - RED
  // FourScanPanel->setCursor(76, 22);
  // FourScanPanel->setTextColor(red);
  // FourScanPanel->print("P2");

  // // Panel 3: Bottom-Left (x: 12, y: 54) - YELLOW
  // FourScanPanel->setCursor(12, 54);
  // FourScanPanel->setTextColor(red);
  // FourScanPanel->print("P3");

  // // Panel 4: Bottom-Right (x: 76, y: 54) - CYAN
  // FourScanPanel->setCursor(76, 54);
  // FourScanPanel->setTextColor(red);
  // FourScanPanel->print("P4");

  delay(2000);
}
