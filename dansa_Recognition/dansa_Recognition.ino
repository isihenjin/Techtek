//段差認識用のモデルを用いると認識結果の値がおかしくなる不具合あり

/*典型的なエラー
・dnnrt.begin failed -1 :nnbfile をロードするためのメモリ領域がありません
  →ArduinoIDEの「ツール」→Memoryを1536KBに変更

  dnnrt.begin failed -1 :学習モデルサイズが大きすぎてSDカードからロードできない
*/

#include <Camera.h>
#include <SPI.h>
#include <EEPROM.h>
#include <DNNRT.h>
#include "Adafruit_ILI9341.h"
#include <Audio.h>
AudioClass *theAudio;
#include <SDHCI.h>
SDClass theSD;

/* LCD Settings */
#define TFT_RST 8
#define TFT_DC  9
#define TFT_CS  10

//キャプチャするウィンドウのサイズ すべて偶数で定義しないとエラー

////入力画像のサイズ（学習済モデルの画像サイズに合わせる？）
////CAM_CLIPで切り取る範囲の2^n乗サイズにしないとエラー
#define DNN_IMG_W 80
#define DNN_IMG_H 60

#define CAM_IMG_W 320
#define CAM_IMG_H 240
//カメラ画像から切り取る範囲の定義
#define CAM_CLIP_X 80
#define CAM_CLIP_Y 60
#define CAM_CLIP_W 160
#define CAM_CLIP_H 120
#define PIN1 0
#define LINE_THICKNESS 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

uint8_t buf[DNN_IMG_W*DNN_IMG_H];

DNNRT dnnrt;
DNNVariable input(DNN_IMG_W*DNN_IMG_H);
int delayTime=300;
bool allowBEEP=false;
int count=0;
static String const label[2] = {"dansa","notdansa"};

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
  dnnrt.inputVariable(input, 0);
  dnnrt.forward();
  DNNVariable output = dnnrt.outputVariable(0);
  int index = output.maxIndex();
  
  if (index < 2) {
    gStrResult = String(label[index]) + String(":") + String(output[index]);
    float normalized = (float)output[1]; // LONG_MAX を使って 0.0～1.0 に正規化
    if (normalized < 0.0f) normalized = 0.0f;         // 範囲外の保護
    if (normalized > 1.0f) normalized = 1.0f;

    delayTime = (int)(normalized * (1000 - 50) + 50);
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
  pinMode(PIN1 , OUTPUT);
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  theAudio=AudioClass::getInstance();
  theAudio->begin();
  puts("bigin");
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP,0,0);
  theAudio->setBeep(1,-20,700);
  while (!theSD.begin()) { putStringOnLcd("Insert SD card", ILI9341_RED); }
  //学習モデルの読み込み
  File nnbfile = theSD.open("model.nnb");

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

void loop() { 
  count++;
  if(count>delayTime){
  theAudio->setBeep(1,-20,1000);
  analogWrite(PIN1,255);
  delay(50);
  theAudio->setBeep(0,0,0);
  analogWrite(PIN1,0);
  count=0;
  }
  delay(1);
  }
