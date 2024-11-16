#include <Camera.h>
#include <SPI.h>
#include <EEPROM.h>
#include <DNNRT.h>
#include "Adafruit_ILI9341.h"

#include <SDHCI.h>
SDClass theSD;

/* LCD Settings */
#define TFT_RST 8
#define TFT_DC  9
#define TFT_CS  10

//キャプチャするウィンドウのサイズ すべて偶数で定義しないとエラー
#define DNN_IMG_W 80
#define DNN_IMG_H 60

#define CAM_IMG_W 320
#define CAM_IMG_H 240
//カメラ画像から切り取る範囲の定義
#define CAM_CLIP_X 80
#define CAM_CLIP_Y 60
#define CAM_CLIP_W 160
#define CAM_CLIP_H 120

#define LINE_THICKNESS 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);
DNNRT dnnrt;
// フルカラー画像用にバッファを3チャンネル対応
DNNVariable input(3 * DNN_IMG_W * DNN_IMG_H);

static String const label[2] = {"dansa", "notdansa"};

//ディスプレイに文字列を表示
void putStringOnLcd(String str, int color) {
  int len = str.length();
  tft.fillRect(0,224, 320, 240, ILI9341_BLACK);
  tft.setTextSize(2);
  int sx = 160 - len / 2 * 12;
  if (sx < 0) sx = 0;
  tft.setCursor(sx, 225);
  tft.setTextColor(color);
  tft.println(str);
}

void drawBox(uint16_t* imgBuf) {
  /* Draw target line */
  for (int x = CAM_CLIP_X; x < CAM_CLIP_X + CAM_CLIP_W; ++x) {
    for (int n = 0; n < LINE_THICKNESS; ++n) {
      *(imgBuf + CAM_IMG_W * (CAM_CLIP_Y + n) + x)              = ILI9341_RED;
      *(imgBuf + CAM_IMG_W * (CAM_CLIP_Y + CAM_CLIP_H - 1 - n) + x) = ILI9341_RED;
    }
  }
  for (int y = CAM_CLIP_Y; y < CAM_CLIP_Y + CAM_CLIP_H; ++y) {
    for (int n = 0; n < LINE_THICKNESS; ++n) {
      *(imgBuf + CAM_IMG_W * y + CAM_CLIP_X + n)                = ILI9341_RED;
      *(imgBuf + CAM_IMG_W * y + CAM_CLIP_X + CAM_CLIP_W - 1 - n) = ILI9341_RED;
    }
  }  
}

void CamCB(CamImage img) {

  if (!img.isAvailable()) {
    Serial.println("Image is not available. Try again");
    return;
  }

  CamImage small;
  CamErr err = img.clipAndResizeImageByHW(small
                     , CAM_CLIP_X, CAM_CLIP_Y
                     , CAM_CLIP_X + CAM_CLIP_W - 1
                     , CAM_CLIP_Y + CAM_CLIP_H - 1
                     , DNN_IMG_W, DNN_IMG_H);
  if (!small.isAvailable()){
    putStringOnLcd("Clip and Resize Error:" + String(err), ILI9341_RED);
    return;
  }

  small.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  uint16_t* tmp = (uint16_t*)small.getImgBuff();

  float *dnnbuf = input.data();
  for (int n = 0; n < DNN_IMG_H * DNN_IMG_W; ++n) {
    // RGB565形式から各チャネルを抽出
    uint8_t r = (tmp[n] & 0xF800) >> 11; // 赤（5ビット）
    uint8_t g = (tmp[n] & 0x07E0) >> 5;  // 緑（6ビット）
    uint8_t b = (tmp[n] & 0x001F);       // 青（5ビット）

    // 255で割って正規化
    dnnbuf[3 * n]     = (float)r / 255.0f; // 赤を0.0～1.0に正規化
    dnnbuf[3 * n + 1] = (float)g / 255.0f; // 緑を0.0～1.0に正規化
    dnnbuf[3 * n + 2] = (float)b / 255.0f; // 青を0.0～1.0に正規化
  }

  String gStrResult = "?";
  dnnrt.inputVariable(input, 0);
  dnnrt.forward();
  DNNVariable output = dnnrt.outputVariable(0);
  int index = output.maxIndex();
  
  if (index < 10) {
    gStrResult = String(label[index]) + String(":") + String(output[index]);
  } else {
    gStrResult = String("?:") + String(output[index]);
  }
  Serial.println(gStrResult);

  img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  uint16_t* imgBuf = (uint16_t*)img.getImgBuff(); 

  drawBox(imgBuf); 
  tft.drawRGBBitmap(0, 0, (uint16_t *)img.getImgBuff(), 320, 224);
  putStringOnLcd(gStrResult, ILI9341_YELLOW);
}

void setup() {   
  Serial.begin(115200);
 
  tft.begin();
  tft.setRotation(3);

  while (!theSD.begin()) { putStringOnLcd("Insert SD card", ILI9341_RED); }
  //学習モデルの読み込み
  File nnbfile = theSD.open("model.nnb");

  //学習済モデルでDNNRTを開始
  int ret = dnnrt.begin(nnbfile);
  if (ret < 0) {
    putStringOnLcd("dnnrt.begin failed" + String(ret), ILI9341_RED);
    return;
  }

  theCamera.begin();
  theCamera.startStreaming(true, CamCB);
}

void loop() { }
