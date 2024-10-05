import processing.serial.*; // シリアル通信のライブラリをインポート

processing.serial.Serial myPort; // Serialクラスを完全修飾名で指定

PImage img;
final static int WIDTH = 320;
final static int HEIGHT = 240;
boolean started = false;
int serialTimer = 0;
int total = 0;
int x = 0;
int y = 0;

void setup() {
  size(320, 240); 
  background(0);
  img = createImage(WIDTH, HEIGHT, RGB); 
  myPort = new processing.serial.Serial(this, processing.serial.Serial.list()[0], 1000000);  
  myPort.clear();
  println("setup finished");
  delay(2000); // wait for stable connection
}

void draw() {
  if (started == false) {
    started = true;  
    println("start");
    myPort.write('S'); // Start signal
    myPort.clear();
    total = 0;      
    delay(10);
    return;
  }

  final int interval = 1; // Adjust this interval for stable connection
  if (millis() - serialTimer > interval) {
    serialTimer = millis();   
    if (myPort.available() < 2) return; // Wait until at least 2 bytes are available
    
    while (myPort.available() >= 2) { // Ensure at least 2 bytes are available
      int lbyte = myPort.read(); // Read lower byte
      int ubyte = myPort.read(); // Read upper byte
      x = total % WIDTH;
      y = total / WIDTH;
      int value = (ubyte << 8) | (lbyte); // Combine bytes to form 16-bit RGB565 value
      char r = (char)(((value & 0xf800) >> 11) << 3);
      char g = (char)(((value & 0x07E0) >> 5)  << 2);
      char b = (char)((value & 0x001f) << 3);
      color c = color(r, g, b);
      img.set(x, y, c);
      ++total;

      if (total >= WIDTH * HEIGHT) {
        println("end");
        myPort.clear();
        started = false;
        total = 0;
        image(img, 0, 0);
        break;
      }
    }
  }
}
