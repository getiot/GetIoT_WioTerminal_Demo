
#define BLYNK_PRINT Serial

/* Includes ---------------------------------------------------------------- */
#include <rpcWiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleWioTerminal.h>
#include <TFT_eSPI.h> //TFT LCD library 
#include <bsec.h>


#include <dehumidifier_inferencing.h>

int predicted_label = 1;
float prediction_threshold = 0.6;
int prediction_count = 0;
int prediction_sum = 0;
String unitStatus = "STAND BY";

/** Audio buffers, pointers and selectors */
typedef struct {
  int16_t *buffer;
  uint8_t buf_ready;
  uint32_t buf_count;
  uint32_t n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
unsigned int sampling_period_us = round(600000 * (1.0 / 16000));
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal



char auth[] = "blynk_token";
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
  timer.setInterval(10000L, sendPrediction);
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
  tft.drawString("Pressure", 160, 140);



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

   // summary of inferencing settings (from model_metadata.h)
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
    ei_printf("ERR: Failed to setup audio sampling\r\n");
    return;
  }

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

  float avg = (float)prediction_sum / (float) prediction_count ;
  if ( avg  < 0.5) {
    unitStatus = "RUNNING";
  } else {
    unitStatus = "STAND BY";
  }
  prediction_sum = 0;
  prediction_count = 0;
  Blynk.virtualWrite(V5, unitStatus);
  //V0 used for humidityThreshold
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, h);
  Blynk.virtualWrite(V3, a);
  Blynk.virtualWrite(V4, p);

//  sum = 0;
//  count = 0;


  Serial.printf("humidityThreshold=%d\n", humidityThreshold);
  Serial.printf("T=%d H=%d IAQ=%d P=%d S=%d P=%s\n", t, h, a, p, s, unitStatus.c_str());

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
  //spr.drawNumber(avg, 5, 5);
  spr.drawString(unitStatus.c_str(),5,5);
  spr.pushSprite(165, 160);
  spr.deleteSprite();



}
/**
   @brief      Arduino main function. Runs the inferencing loop.
*/


void loop()
{

//  s = analogRead(WIO_MIC);
//  sum += s;
//  count++;

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


  ei_printf("Recording...\n");

  bool m = microphone_inference_record();
  if (!m) {
    ei_printf("ERR: Failed to record audio...\n");
    return;
  }

  ei_printf("Recording done\n");

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = { 0 };

  EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", r);
    return;
  }

  // print the predictions
  float max_prediction = 0;
  ei_printf("Predictions ");
  ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
  ei_printf(": \n");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    if ( result.classification[ix].value >= max_prediction) {
      max_prediction = result.classification[ix].value;
      predicted_label = ix;
    }

  }

  if (max_prediction >= prediction_threshold) {
    ei_printf("Predicted activity = %d with score=%.5f (0= Running, 1= Standby)\n", predicted_label, max_prediction);
    prediction_count = prediction_count + 1;
    prediction_sum = prediction_sum + predicted_label;

  }
  renderSensorDataOnScreen();

  if ( millis() > WIO_RESET_INTERVAL_IN_MINUTES * 60 * 1000) {
    NVIC_SystemReset();
  }



}


/**
   @brief      Printf function uses vsnprintf and output using Arduino Serial

   @param[in]  format     Variable argument list
*/
void ei_printf(const char *format, ...) {
  static char print_buf[1024] = { 0 };

  va_list args;
  va_start(args, format);
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
  va_end(args);

  if (r > 0) {
    Serial.write(print_buf);
  }
}


/**
   @brief      Init inferencing struct and setup/start PDM

   @param[in]  n_samples  The n samples

   @return     { description_of_the_return_value }
*/
static bool microphone_inference_start(uint32_t n_samples)
{
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

  if (inference.buffer == NULL) {
    return false;
  }

  inference.buf_count  = 0;
  inference.n_samples  = n_samples;
  inference.buf_ready  = 0;
  

  return true;
}

/**
   @brief      Wait on new data

   @return     True when finished
*/
static bool microphone_inference_record(void)
{
  inference.buf_ready = 0;
  inference.buf_count = 0;

  if (inference.buf_ready == 0) {
    for (int i = 0; i < 8001; i++) {
      inference.buffer[inference.buf_count++] = map(analogRead(WIO_MIC), 0, 1023, -32768, 32767);
      delayMicroseconds(sampling_period_us);

      if (inference.buf_count >= inference.n_samples) {
        inference.buf_count = 0;
        inference.buf_ready = 1;
        break;
      }
    }
  }

  return true;
}

/**
   Get raw audio signal data
*/
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

  return 0;
}

/**
   @brief      Stop PDM and release buffers
*/
static void microphone_inference_end(void)
{
  free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif