
#define BLYNK_PRINT Serial

/* Includes ---------------------------------------------------------------- */
#include <rpcWiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleWioTerminal.h>
#include <TFT_eSPI.h> //TFT LCD library 
#include <bsec.h>

char auth[] = "token";
char ssid[] = "ssid";
char pass[] = "password";
BlynkTimer timer;



TFT_eSPI tft; //Initializing TFT LCD
TFT_eSprite spr = TFT_eSprite(&tft); //Initializing buffer
Bsec iaqSensor;
String output;
void checkIaqSensorStatus(void);

int t = 0, h = 0, a = 0, p = 0, s = 0, avg = 0;
int sum = 0; int count = 0;

long WIO_RESET_INTERVAL_IN_MINUTES = 60;


/**
   @brief      Arduino setup function
*/
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(WIO_MIC, INPUT);

  tft.begin(); //Start TFT LCD
  tft.setRotation(3); //Set TFT LCD rotation

  tft.setTextSize(2);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting to Blynk...", 20, 120);

  Blynk.begin(auth, ssid, pass);
  timer.setInterval(1000L, sendPrediction);
  Blynk.syncAll();

  tft.fillScreen(TFT_BLACK);

  //Setting the title header
  //tft.fillScreen(TFT_WHITE); //Fill background with white color
  tft.fillRect(0, 0, 320, 50, TFT_YELLOW); //Rectangle fill with dark green
  tft.setTextColor(TFT_BLACK); //Setting text color
  tft.setTextSize(3); //Setting text size
  tft.drawString("Basement", 50, 10); //Drawing a text string



  tft.drawFastVLine(150, 50, 160, TFT_DARKGREEN); //Drawing verticle line
  tft.drawFastHLine(0, 130, 320, TFT_DARKGREEN); //Drawing horizontal line
  tft.drawFastHLine(0, 210, 320, TFT_DARKGREEN); //Drawing horizontal line 2

  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Temperature", 10, 60);

  tft.setTextSize(1);
  tft.drawString("Humidity", 10, 140);

  tft.setTextSize(1);
  tft.drawString("IAQ", 160, 60);

  tft.setTextSize(1);
  tft.drawString("Noise", 160, 140);



  Wire.begin();
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  checkIaqSensorStatus();
  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  iaqSensor.updateSubscription(sensorList, 7, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

}

void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK) {
    if (iaqSensor.status < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
      while (true) {

      }

    } else {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme680Status != BME680_OK) {
    if (iaqSensor.bme680Status < BME680_OK) {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
      while (true) {

      }
    } else {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

long lastNotification = 0;
int humidityThreshold = 60;

BLYNK_WRITE(V0)
{
  //reads the slider value when it changes in the app
  humidityThreshold = param.asInt();
}

void sendPrediction() {

  avg = sum /  count ;

  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, h);
  Blynk.virtualWrite(V3, a);
  Blynk.virtualWrite(V4, avg);

  sum = 0;
  count = 0;


  Serial.printf("humidityThreshold=%d\n", humidityThreshold);
  Serial.printf("T=%d H=%d IAQ=%d P=%d S=%d\n", t, h, a, p, s);

  if (h > humidityThreshold ) {

    if (lastNotification == 0 || (millis() - lastNotification) > 60 * 60 * 1000) {
      lastNotification = millis();
      Blynk.notify("Basement is too humid. Check the dehumidifier");
    }

  }

  if (a > 200 ) {

    Blynk.notify("Basement IAQ is bad.");

  }

}


void renderSensorDataOnScreen() {

  //Setting temperature
  spr.createSprite(100, 30);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_LIGHTGREY);
  spr.setTextSize(3);
  spr.drawNumber(t, 5, 5);
  spr.drawString("F", 45, 5);
  spr.pushSprite(35, 80);
  spr.deleteSprite();


  //Setting humidity
  spr.createSprite(100, 30);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_LIGHTGREY);
  spr.setTextSize(3);
  spr.drawNumber(h, 5, 5);
  spr.drawString("%", 45, 5);
  spr.pushSprite(35, 160);
  spr.deleteSprite();



  //IAQ
  String iaqLabel = "Good";

  spr.createSprite(150, 50);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_LIGHTGREY);
  spr.setTextSize(3);


  if (a <= 50) {
    iaqLabel = "Good";
    spr.setTextColor(TFT_DARKGREEN);
  } else if (a > 50 && a <= 100) {
    spr.setTextColor(TFT_ORANGE);
    iaqLabel = "Moderate";
  } else if (a > 100 && a <= 200) {
    spr.setTextColor(TFT_RED);
    iaqLabel = "Unhealthy";
  } else if (a > 200 && a <= 300) {
    spr.setTextColor(TFT_PURPLE);
    spr.setTextSize(2);
    iaqLabel = "Very Unhealthy";
  } else {
    spr.setTextColor(TFT_MAROON);
    spr.setTextSize(2);
    iaqLabel = "Hazardous";
  }

  spr.drawString(iaqLabel, 5, 5);
  spr.setTextColor(TFT_LIGHTGREY);
  spr.setTextSize(2);
  spr.drawNumber(a, 110, 30);
  spr.pushSprite(165, 80);
  spr.deleteSprite();

  //Setting prediction
  spr.createSprite(150, 30);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_LIGHTGREY);
  spr.setTextSize(3);
  spr.drawNumber(avg, 5, 5);
  spr.pushSprite(165, 160);
  spr.deleteSprite();



}
/**
   @brief      Arduino main function. Runs the inferencing loop.
*/


void loop()
{

  s = analogRead(WIO_MIC);
  sum += s;
  count++;

  Blynk.run();
  timer.run();

  if (iaqSensor.run()) { // If new data is available

    t = (int) iaqSensor.temperature * 1.8 + 32;
    h =  iaqSensor.humidity;
    a = iaqSensor.iaq;
    p = (int) (iaqSensor.pressure / 100);


  } else {
    checkIaqSensorStatus();
  }

  renderSensorDataOnScreen();

  if ( millis() > WIO_RESET_INTERVAL_IN_MINUTES * 60 * 1000) {
    NVIC_SystemReset();
  }



}

