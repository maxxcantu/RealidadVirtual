/*******************************

4°: Clasificacion y reconocimiento de imagenes con Clarifai

********************************/


#include "Arduino.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <base64.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include<string>

const char* ssid = "EL RANCHO";
const char* password = "sofimaxi2";

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void classifyImage() {
  
  // Capture picture
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
   
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("Picture captured!!");

  size_t size = fb->len;
  String buffer = base64::encode((uint8_t *) fb->buf, fb->len);
  
  String payload = "{\"inputs\": [{ \"data\": {\"image\": {\"base64\": \"" + buffer + "\"}}}]}";

  buffer = "";

  esp_camera_fb_return(fb);
  
  // Generic model
  String model_id = "aaa03c23b3724a16a56b629203edc62c";

  HTTPClient http;
  http.begin("https://api.clarifai.com/v2/models/" + model_id + "/outputs");
  http.addHeader("Content-Type", "application/json");     
  http.addHeader("Authorization", "Key 989744553cda4d7fbaa53e54c542791d"); 
  int response_code = http.POST(payload);

  String response;
  
  if(response_code >0){
    Serial.println(response_code );
    Serial.println("Returned String: ");
    response = http.getString();
    Serial.println(response);
  } 
  else {
    Serial.print("POST Error: ");
    Serial.print(response_code);
    return;
  }

  // Arduino assistant para determinar tamanio necesario del JSONdocument
  // DynamicJsonDocument doc(jsonSize);
  DynamicJsonDocument doc(24576);
  // deserializeJson(doc, response);
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* name[5];
  double p[5];

  for (int i=0; i < 5; i++) {
    // const String name = doc["outputs"][0]["data"]["concepts"][i]["name"];
    // double p = doc["outputs"][0]["data"]["concepts"][i]["value"];
    name[i] = doc["outputs"][0]["data"]["concepts"][i]["name"];
    p[i] = doc["outputs"][0]["data"]["concepts"][i]["value"];

    Serial.println("=====================");
    Serial.print("Name: ");    
    Serial.print(name[i]);
    Serial.println("/");
    Serial.print("Prob: ");
    Serial.println(p[i]);
    Serial.println();

    
  }
  
  int cont = 0;
  const char* no_pers[5] = {"Ninguna persona"};
  for (int j = 0; j < 5; j++) {
    Serial.println("entra FOR");
    Serial.println(name[j]);
    Serial.println("==");
    Serial.println(no_pers[0]);
    if (name[j] == no_pers[0]) {
      Serial.println("entra IF");
      // if (p[j] > 0.90) {
        // cont++;
        // if (cont >= 3) {
        // Suena alarma
        Serial.println(" ALARMA ALARMA ALARMA !!!!!!!!");
        // }
      // }
    }
    else {
      Serial.println("no entra IF");
    }
  }
 

  Serial.println("\nSleep....");
  esp_deep_sleep_start();

}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  classifyImage();
 
}

void loop() {

}