//何も処理していない普通のカメラ画像を表示する
#include <Camera.h>

#define BAUDRATE       1000000

void CamCB(CamImage img) {
  img.resizeImageByHW(img,1/32,1/32);
  img.resizeImageByHW(img,32,32);

  if (img.isAvailable() == false) return;
 
  while (Serial.available() <= 0); 
  // taking a picture is started by receiving 'S'

  if (Serial.read() != 'S') return;
  delay(1); // wait for stable connection
  
  digitalWrite(LED0, HIGH);

  img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  char *buf = img.getImgBuff();

  for (int i = 0; i < img.getImgSize(); ++i, ++buf) {
    Serial.write(*buf);
  }
  
  digitalWrite(LED0, LOW);
}

void setup() {
  
  Serial.begin(BAUDRATE);
  while (!Serial) {};


  theCamera.begin();
  theCamera.startStreaming(true, CamCB);
  theCamera.setAutoISOSensitivity(false);
  
}

void loop() {
  /* do nothing here */
}