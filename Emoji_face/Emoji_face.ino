#ifdef KENDRYTE_K210
#include <SPIClass.h>
#else
#include <SPI.h>
#endif

#include<string.h>
#include "Seeed_FS.h"
#include"LIS3DHTR.h"
#include"TFT_eSPI.h"
#include"RawImage.h"


LIS3DHTR<TwoWire>  lis;
TFT_eSPI tft;

#define debug(...)        //Serial.printf(__VA_ARGS__);
#define debug_begin(baud) //Serial.begin(baud); while (!Serial);
#define screen_height   240
#define screen_width    320


uint8_t raw[screen_height * screen_width];

void writeToBuffer(int x, int y, Raw8 * img) {
  // debug("%p\n", img);
  // debug("%d, %d\n", img->width, img->height);
  for (int j = y; j < y + img->height(); j++) {
    for (int i = x; i < x + img->width(); i++) {
      raw[j * screen_width + i] = img->get(i - x, j - y);
    }
  }
}


void clearBuffer() {
  memset(raw, 0, sizeof(raw));
}

void flushBuffer() {
  tft.pushImage(0, 0, tft.width(), tft.height(), raw);
}

bool flag_left = false;
void button_handler_left() {
  flag_left = true;
}

bool flag_middle = false;
void button_handler_middle() {
  flag_middle = true;
}

bool flag_right = false;
void button_handler_right() {
  flag_right = true;
}

#define FILLTER_N 20
int filter(int16_t val) {
  int32_t filter_sum = 0;
  for (uint8_t i = 0; i < FILLTER_N; i++) {
    filter_sum += val;
    delay(1);
  }
  return (int)(filter_sum / FILLTER_N);
}


const char* face[] = {"face/defaultface.bmp", "face/face0.bmp", "face/face1.bmp", "face/face2.bmp", "face/face3.bmp"};

const char* eye[] = {"face/cool.bmp", "face/happy.bmp", "face/heart.bmp", "face/robot.bmp", "face/sad.bmp",
                     "face/star.bmp", "face/cross.bmp", "face/kaws.bmp", "face/red.bmp"
                    };

void setup() {
  debug_begin(115200);
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_1), button_handler_left, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2), button_handler_middle, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3), button_handler_right, FALLING);
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI, 16000000)) {
    while (1);
  }
  
  tft.begin();
  tft.setRotation(3);

  tft.fillScreen(TFT_BLACK);

  lis.begin(Wire1);
  lis.setOutputDataRate(LIS3DHTR_DATARATE_25HZ);
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G);
}

int8_t eye_count = 0;

void loop() {
  float x_raw = lis.getAccelerationX();
  float y_raw = lis.getAccelerationY();

  int16_t x_value = 100 * x_raw;
  int16_t y_value = 100 * y_raw;

  delay(30);

  int16_t x_axis = map(filter(x_value), -15, 15, 73, 77); // mapped x-axis
  int16_t y_axis = map(filter(y_value), -15, 15, 53, 57); // mapped y-axis

  Raw8 * eyes = newImage<uint8_t>(eye[eye_count]);
  writeToBuffer(x_axis, y_axis, eyes);
  writeToBuffer(x_axis + 120, y_axis, eyes);
  delete[] eyes;

  Raw8 * mouth = newImage<uint8_t>("face/mouth.bmp");
  writeToBuffer(112, 175, mouth);
  delete[] mouth;
  flushBuffer();
  clearBuffer();

  if (flag_left) {
    flag_left = false;
    eye_count++;
    if (eye_count == 9) eye_count = 0;
  }
  if (flag_right) {
    flag_right = false;
    eye_count--;
    if (eye_count == -1) eye_count = 8;
  }

  if (flag_middle) {
    flag_middle = false;
    for (uint8_t cnt = 1; cnt < 5; cnt++) {
      drawImage<uint8_t>(face[cnt], 75, 55);
    }
  }

}
