#include <Camera.h>
#include <SPI.h>
#include "Adafruit_ILI9341.h"  //ディスプレイ

/* LCD Settings */
#define TFT_RST 8
#define TFT_DC  9
#define TFT_CS  10

#define CAM_IMG_W 320
#define CAM_IMG_H 240

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

void CamCB(CamImage img) {
  if (!img.isAvailable()) {
    Serial.println("Image is not available. Try again");
    return;
  }

  img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  uint16_t* imgBuf = (uint16_t*)img.getImgBuff(); 

  // カメラ画像をディスプレイにそのまま表示
  tft.drawRGBBitmap(0, 0, imgBuf, CAM_IMG_W, CAM_IMG_H);
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);

  // カメラの初期化とストリーミング開始
  theCamera.begin();
  theCamera.startStreaming(true, CamCB);
}

void loop() { }
