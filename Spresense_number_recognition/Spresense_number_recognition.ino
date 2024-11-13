#include <Camera.h>
#include <SPI.h>
#include <EEPROM.h>
#include <DNNRT.h>   //組み込みAIライブラリ
#include "Adafruit_ILI9341.h"  //ディスプレイ

#include <SDHCI.h>
SDClass theSD;

/* LCD Settings */
#define TFT_RST 8
#define TFT_DC  9
#define TFT_CS  10

//入力画像のサイズ（学習済モデルの画像サイズに合わせる）
#define DNN_IMG_W 28
#define DNN_IMG_H 28
//カメラ画像サイズ
#define CAM_IMG_W 320
#define CAM_IMG_H 240
//カメラ画像から切り取る範囲の定義
#define CAM_CLIP_X 104
#define CAM_CLIP_Y 0
#define CAM_CLIP_W 112
#define CAM_CLIP_H 224

#define LINE_THICKNESS 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

uint8_t buf[DNN_IMG_W*DNN_IMG_H];

DNNRT dnnrt;
DNNVariable input(DNN_IMG_W*DNN_IMG_H);


static uint8_t const label[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

//ディスプレイに文字列を表示
void putStringOnLcd(String str, int color) {
  int len = str.length();
  tft.fillRect(0,224, 320, 240, ILI9341_BLACK);
  tft.setTextSize(2);
  int sx = 160 - len/2*12;
  if (sx < 0) sx = 0;
  tft.setCursor(sx, 225);
  tft.setTextColor(color);
  tft.println(str);
}

void drawBox(uint16_t* imgBuf) {
  /* Draw target line */
  for (int x = CAM_CLIP_X; x < CAM_CLIP_X+CAM_CLIP_W; ++x) {
    for (int n = 0; n < LINE_THICKNESS; ++n) {
      *(imgBuf + CAM_IMG_W*(CAM_CLIP_Y+n) + x)              = ILI9341_RED;
      *(imgBuf + CAM_IMG_W*(CAM_CLIP_Y+CAM_CLIP_H-1-n) + x) = ILI9341_RED;
    }
  }
  for (int y = CAM_CLIP_Y; y < CAM_CLIP_Y+CAM_CLIP_H; ++y) {
    for (int n = 0; n < LINE_THICKNESS; ++n) {
      *(imgBuf + CAM_IMG_W*y + CAM_CLIP_X+n)                = ILI9341_RED;
      *(imgBuf + CAM_IMG_W*y + CAM_CLIP_X + CAM_CLIP_W-1-n) = ILI9341_RED;
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
                     , CAM_CLIP_X + CAM_CLIP_W -1
                     , CAM_CLIP_Y + CAM_CLIP_H -1
                     , DNN_IMG_W, DNN_IMG_H);
  if (!small.isAvailable()){
    putStringOnLcd("Clip and Reize Error:" + String(err), ILI9341_RED);
    return;
  }

  small.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  uint16_t* tmp = (uint16_t*)small.getImgBuff();

  float *dnnbuf = input.data();
  float f_max = 0.0;
  for (int n = 0; n < DNN_IMG_H*DNN_IMG_W; ++n) {
    dnnbuf[n] = (float)((tmp[n] & 0x07E0) >> 5);
    if (dnnbuf[n] > f_max) f_max = dnnbuf[n];
  }
  
  /* normalization */
  for (int n = 0; n < DNN_IMG_W*DNN_IMG_H; ++n) {
    dnnbuf[n] /= f_max;
  }
  
  String gStrResult = "?";

  //推論の実行
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

  //推論結果のディスプレイ表示
  drawBox(imgBuf); 
  tft.drawRGBBitmap(0, 0, (uint16_t *)img.getImgBuff(), 320, 224);
  putStringOnLcd(gStrResult, ILI9341_YELLOW);
}


void setup() {   
  Serial.begin(115200);
 
  tft.begin();
  tft.setRotation(3);

  while (!theSD.begin()) { putStringOnLcd("Insert SD card", ILI9341_RED); }
  
  //学習済モデルの読み込み
  File nnbfile = theSD.open("result_v1.nnb");
  //ファイルが読み込めてるか確認
  // if (nnbfile = NULL){
  //   putStringOnLcd("file open failed" , ILI9341_RED);
  // }else{
  //   putStringOnLcd("file opened! " +String(nnbfile), ILI9341_RED);
  // }

  //学習済モデルでDNNRTを開始
  //DNNRTを学習済モデルで初期化
  int ret = dnnrt.begin(nnbfile);
  if (ret < 0) {
    putStringOnLcd("dnnrt.begin failed" + String(ret), ILI9341_RED);
    return;
  }

  theCamera.begin();
  theCamera.startStreaming(true, CamCB);
}

void loop() { }