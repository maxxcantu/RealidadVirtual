/***************************************

3° + 4°: Capturar fotos cada cierto tiempo con interrupciones y procesarlas para detectar ausencia de persona

***************************************/

#include "WiFi.h"
#include "esp_camera.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <EEPROM.h>            // read and write from flash memory
#include <base64.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>

typedef enum MODOS{
  MODO_AUTO,
  MODO_MANUAL,
};
// MODOS modo_actual;

typedef enum WIFI{
  _LOCAL_WIFI,
  _AP_WIFI,
};

// Define system functioning mode 
volatile MODOS modo_actual;

// Define WIFI connection mode
volatile WIFI tipo_wifi = _LOCAL_WIFI;

// define the number of bytes you want to access
#define EEPROM_SIZE 1

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto_auto = true;
boolean takeNewPhoto_manual = false;

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

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

int pictureNumber = 0;

camera_fb_t * fb = NULL;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>Ultima foto</h2>
    <p>Capturar foto (puede demorar unos segundos) y actualizar pagina.</p>
    <p>
      <button onclick="rotatePhoto();">ROTAR</button>
      <button onclick="capturePhoto()">CAPTURAR FOTO</button>
      <button onclick="location.reload();">ACTUALIZAR PAGINA</button>
      <button onclick="changeMode();">MODO AUTO/MANUAL</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
  function changeMode() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/change", true);
    xhr.send();
  }
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>
)rawliteral";

// volatile int interruptCounter;
// int totalInterruptCounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define t 30 // Tiempo en segundos para capturar foto

void classifyImage(camera_fb_t *);
bool checkPhoto( fs::FS &fs);
camera_fb_t * takePhoto();
void initSD();
void saveSD(camera_fb_t * &);
void saveSPIFFS(camera_fb_t * &);
void changeMode();
// void control(MODOS);

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  //Accion a realizar en interrupcion
  takeNewPhoto_auto = true;
  // control(modo_actual);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void classifyImage(camera_fb_t * fb) {

  size_t size = fb->len;
  String buffer = base64::encode((uint8_t *) fb->buf, fb->len);
  
  String payload = "{\"inputs\": [{ \"data\": {\"image\": {\"base64\": \"" + buffer + "\"}}}]}";

  buffer = "";

  Serial.println(payload);
  
  // Generic model
  String model_id = "aaa03c23b3724a16a56b629203edc62c";

  HTTPClient http;
  http.begin("https://api.clarifai.com/v2/models/" + model_id + "/outputs");
  http.addHeader("Content-Type", "application/json");     
  http.addHeader("Authorization", "Key 989744553cda4d7fbaa53e54c542791d"); 
  int response_code = http.POST(payload);

  String response;
  
  if(response_code >0){
    Serial.print("HTTP codigo respuesta: ");
    Serial.println(response_code );
    Serial.println("String retornado: ");
    response = http.getString();
    Serial.println(response);
  } 
  else {
    Serial.print("HTTP POST Error: ");
    Serial.println(response_code);
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
  int out_strcmp;
  const char* no_pers[1] = {"Ninguna persona"};
  for (int j = 0; j < 5; j++) {
    Serial.println("entra FOR");
    Serial.print("-->");
    Serial.print(name[j]);
    Serial.print(" == ");
    Serial.println(no_pers[0]);
    out_strcmp = (strcmp(name[j], no_pers[0]));
    Serial.print("resultado strcmp(): ");
    Serial.println(out_strcmp);
    if (out_strcmp == 0) {
      Serial.println("entra IF");
      if (p[j] > 0.90) {
        cont++;
        // if (cont >= 3) {
        // Suena alarma
        Serial.println("-------------------------------------");
        Serial.println(" ALARMA ALARMA ALARMA !!!!!!!!");
        Serial.println("-------------------------------------");
        initSD();  
        saveSD(fb);
        // }
      }
    }
    else {
      Serial.println("no entra IF");
      cont = 0;
    }
  }
 
  esp_camera_fb_return(fb);

  Serial.println("\nFinalizo analisis de foto");
  // esp_deep_sleep_start();

}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

camera_fb_t * takePhoto() {
  // camera_fb_t * fb = NULL;
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  // do {
    // Take a photo with the camera
    Serial.println(" ");
    Serial.println("Capturando foto...");
    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Captura fallida");
      // break;
    }
    delay(10);

    return fb;
}

void initSD () {
  // SD init
  Serial.println("Starting SD Card...");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    // ESP.restart();
    return;
  }

  delay(10);

  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
}

void saveSD(camera_fb_t * &fb) {
  // Initialize EEPROM with predefined size
  // EEPROM.begin(EEPROM_SIZE);
  pictureNumber = pictureNumber + 1;

  // Path where new picture will be saved in SD Card
  String path = "/picture" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC; 
  
  Serial.printf(">Picture file name in SD: %s\n", path.c_str());
  File file_sd = fs.open(path.c_str(), FILE_WRITE);

  // Insert the data in the photo file_sd
  if (!file_sd) {
    Serial.println("Failed to open file_sd in writing mode");
  }
  else {
    file_sd.write(fb->buf, fb->len); // payload (image), payload length
    // Serial.println("");
    Serial.printf(">>Saved file to SD in path: %s\n", path.c_str());
    // EEPROM.write(0, pictureNumber);
    // EEPROM.commit();
  }
  delay(10);

  file_sd.close();
  esp_camera_fb_return(fb);
}

void saveSPIFFS(camera_fb_t * &fb) {
    // Photo file name
    Serial.printf(">Picture file name in SPIFFS: %s\n", FILE_PHOTO);
    File file_spiffs = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file_spiffs
    if (!file_spiffs) {
      Serial.println("Failed to open file_spiffs in writing mode");
    }
    else {
      file_spiffs.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print(">>Saved file to SPIFFS in path: ");
      Serial.println(FILE_PHOTO);
      // Serial.print(" - Size: ");
      // Serial.print(file.size());
      // Serial.println(" bytes");
    }
    delay(10);

    //Close the files
    file_spiffs.close();
    
    esp_camera_fb_return(fb);

    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);

    // check if file has been correctly saved in SPIFFS
    // ok = checkPhoto(SPIFFS);
  // } while ( !ok );  
}

void changeMode() {
  // modo_actual = modo_actual;
   if (modo_actual == MODO_AUTO) {
      modo_actual = MODO_MANUAL;
      Serial.println("Modo MANUAL activado !");
      // control(modo_actual);
    }
    else {
      modo_actual = MODO_AUTO;
      Serial.println("Modo AUTOMATICO activado !");
      // control(modo_actual);
    }
}

// void control(MODOS modo_actual) {
//   // Serial.print("Control con: ");
//   // Serial.println(modo_actual);
//   switch (modo_actual) {
//     case MODO_AUTO:
//       // timerAlarmEnable(timer);
//       // Serial.println("control en modo_auto---");
//       // timerWrite(timer, 0)
//       if (takeNewPhoto_auto) {
//         Serial.println("control en modo_auto dentro del if---");
//         // fb = takePhoto();
//         classifyImage(fb);
//         takeNewPhoto_auto = false;
//       }
//       else { esp_camera_fb_return(fb);}
//       break;

//     case MODO_MANUAL:
//     // Serial.println("control en modo_manual---");
//       // timerAlarmDisable(timer);  
//       // takeNewPhoto_manual = true; 
//       // while (modo_actual == MODO_MANUAL) {
//         if (takeNewPhoto_manual) {
//           Serial.println("control en modo_manual dentro del if---");
//           // fb = takePhoto();
//           saveSPIFFS(fb);
//           takeNewPhoto_manual = false;
//         }
//         else { esp_camera_fb_return(fb);}
//       // }
//       // delay(10);   
//       break;
//   }

//   delay(10);
// }

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("___________________________________________");

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, t*1000000, true);
  timerAlarmEnable(timer);

  modo_actual = MODO_AUTO;

  // Connect to Wi-Fi
  switch (tipo_wifi) {
    case _LOCAL_WIFI: 
    {
      const char* ssid = "EL RANCHO";
      const char* password = "sofimaxi2";

      WiFi.begin(ssid, password);
      Serial.print("Connecting to WiFi");
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      Serial.println();

      // Print ESP32 Local IP Address
      Serial.println("Conexion local establecida");
      Serial.print("Local - Direccion IP: http://");
      Serial.println(WiFi.localIP());
      break;
    }
      
    case _AP_WIFI:
      const char* ssid = "RVmejoras";
      const char* password = "123456";

      WiFi.softAP(ssid, password);
      IPAddress IP = WiFi.softAPIP();
      // Print ESP32 AP IP Address
      Serial.print("Listo para conectarse a red: " + String(ssid));
      Serial.println(" ");
      Serial.println("Contraseña: " + String(password));
      Serial.println("Acces Point - Direccion IP: http://");
      Serial.println(IP);
      break;
  }

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
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

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
    // return;
  }

  // Begin SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  
  // initSD();  

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto_manual = true;
    // control(modo_actual);
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/change", HTTP_GET, [](AsyncWebServerRequest * request) {
    changeMode();
    request->send_P(200, "text/plain", "Mode changed");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();
  Serial.println("Server started!");

  // control(modo_actual);
}
 

void loop() {

  fb = takePhoto();
  delay(10);

  switch (modo_actual) {
    case MODO_AUTO:
      // timerAlarmEnable(timer);
      // Serial.println("control en modo_auto---");
      // timerWrite(timer, 0)
      if (takeNewPhoto_auto) {
        Serial.println("control en modo_auto dentro del if---");
        // fb = takePhoto();
        classifyImage(fb);
        takeNewPhoto_auto = false;
      }
      else { esp_camera_fb_return(fb);}
      break;

    case MODO_MANUAL:
    // Serial.println("control en modo_manual---");
      // timerAlarmDisable(timer);  
      // takeNewPhoto_manual = true; 
      // while (modo_actual == MODO_MANUAL) {
        if (takeNewPhoto_manual) {
          Serial.println("control en modo_manual dentro del if---");
          // fb = takePhoto();
          saveSPIFFS(fb);
          takeNewPhoto_manual = false;
        }
        else { esp_camera_fb_return(fb);}
      // }
      // delay(10);   
      break;
  }

  // control(modo_actual);
  // while (modo_actual == MODO_MANUAL) {
  //   if (takeNewPhoto_manual) {
  //     fb = takePhoto();
  //     saveSPIFFS_SD(fb);
  //     takeNewPhoto_manual = false;
  //   }
  // }
      

  delay(10);
}