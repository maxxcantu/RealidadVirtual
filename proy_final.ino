/*******************************

Proyecto Pan Tilt + mejoras

*******************************/
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
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>
#include <ESP32Servo.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <EEPROM.h>            // read and write from flash memory
#include <base64.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include <ESP32Time.h>

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
// volatile WIFI tipo_wifi = _AP_WIFI;

#define DUMMY_SERVO1_PIN 12  //We need to create 2 dummy servos.
#define DUMMY_SERVO2_PIN 13  //So that ESP32Servo library does not interfere with pwm channel and timer used by esp32 camera.

#define PAN_PIN 14
#define TILT_PIN 15

Servo dummyServo1;
Servo dummyServo2;
Servo panServo;
Servo tiltServo;

#define LIGHT_PIN 4
const int PWMLightChannel = 4;
#define ALARM_PIN 16

// define the number of bytes you want to access
#define EEPROM_SIZE 1

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

boolean takeNewPhoto_auto = false;
boolean takeNewPhoto_manual = false;

int pictureNumber = 0;

camera_fb_t * fb = NULL;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsServoInput("/ServoInput");
uint32_t cameraClientId = 0;

#define t 20 // Tiempo en segundos para capturar foto
#define cont_max 3 // Limite contador para alarma

ESP32Time rtc;

// Config servidor NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3*3600; // GMT -3 para Argentina (en segundos)
const int daylightOffset_sec = 0;

volatile int cont = 0;
volatile int out_strcmp;
bool last = false;

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

// Web app
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Proyecto Realidad Virtual</title>
    <style>
        body { text-align:center; }
        .vert { margin-bottom: 10%; }
        .hori{ margin-bottom: 0%; }
        
        .noselect {
          -webkit-touch-callout: none;
          /* iOS Safari */
          -webkit-user-select: none;
          /* Safari */
          -khtml-user-select: none;
          /* Konqueror HTML */
          -moz-user-select: none;
          /* Firefox */
          -ms-user-select: none;
          /* Internet Explorer/Edge */
          user-select: none;
          /* Non-prefixed version, currently supported by Chrome and Opera */
        }

        .slider {
            -webkit-appearance: none;
            appearance: none;
            width: 100%;
            height: 20px;
            border-radius: 10px;
            background: #d3d3d3;
            outline: none;
            opacity: 0.7;
            -webkit-transition: .2s;
            transition: opacity .2s;
        }

        .slider:hover {
            opacity: 1;
        }

        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background: #0879A6;
            cursor: pointer;
        }

        .slider::-moz-range-thumb {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background: #0879A6;
            cursor: pointer;
        }

       

        b {
            grid-area: flash-text;
            font-size: 30px;
            margin-bottom: -15px;
            
        }

        h2 {
            text-transform: uppercase;
            text-align: center;
            font-size: 30px;
            font-family: 'Raleway', sans-serif;
        }

        .logo {
            width: 60%;
            align-self: center;
        }

        .slidecontainer-pan {
            grid-area: sl-pan;
        }

        .slidecontainer-tilt {
            grid-area: sl-tilt;
            width: 55px;
        }

        .slidecontainer-flash {
            grid-area: sl-flash;
            height: 20px;
        }

        .slidecontainer-pan input {
            transform: rotate(180deg);
        }

        .slidecontainer-tilt input {
            /* grid-area: sl-tilt; */
            padding: 0;
            width: 240px;
            /* height: 20px; */
            /* transform-origin: 125px 125px;  */
            transform: translate(-100px)  rotate(90deg); 
        }

        .cam {
            grid-area: camara;
        }

        .vacio {
            grid-area: vac;
        }

        .grilla {
            display: grid;
            place-content: center;
            grid-template-areas: 
            "sl-tilt    camara"
            "vac        sl-pan"
            "flash-text  flash-text"
            "sl-flash   sl-flash";
            grid-template-rows: 2fr 0.5fr 0.2fr 0.5fr;
            align-items: center
        }

        .titulo {
            display: flex;
            flex-direction: column;
            width: 100%;
            align-self: center;
        }

        @media screen and (max-width: 480px) {
            .logo {
                width: 100%;
                align-self: center;
            }
            .grilla {
                place-content: normal;
                grid-template-columns: 1fr 10fr;
            }
        
        }
    </style>
</head>

<body class="noselect" align="center" style="background-color:white;">
    <section class="titulo">
        <h2 style="color: #000000; text-align:center; margin-bottom: 0">Control Wi-Fi de cámara pan-tilt</h2>
        <h2 style="text-align:center; margin-top: 0">&#128247 &#128663</h2>
    </section>

    <section id="mainGrid" class="grilla">
        <section>
            <img id="cameraImage" class="cam" src="" style="width:100%">
        </section>
        <section class="slidecontainer-pan">
            <input type="range" min="0" max="180" value="90" class="slider" id="Pan"
                oninput='sendButtonInput("Pan",value)'>
        </section>
        <section class="slidecontainer-tilt">
            <!-- <b>PAN</b> -->
            <input type="range" min="0" max="180" value="90" class="slider" id="Tilt"
                oninput='sendButtonInput("Tilt",value)'>
        </section>
        <section class="vacio"></section>
        <b>&#128294</b>
        <section class="slidecontainer-flash">
            <input type="range" min="0" max="255" value="0" class="slider" id="Light"
                oninput='sendButtonInput("Light",value)'>
        </section>
    </section>

    <div id="container">
      <h2>Ultima foto capturada</h2>
      <p>Capturar foto (puede demorar unos segundos) y actualizar pagina.</p>
      <p>
        <button onclick="rotatePhoto();">ROTAR</button>
        <button onclick="capturePhoto()">CAPTURAR FOTO</button>
        <button onclick="location.reload();">ACTUALIZAR PAGINA</button>
        <button onclick="changeMode();">MODO AUTO/MANUAL</button>
      </p>
    </div>
    <div><img src="saved-photo" id="photo" width="40%"></div>

    <script>
        var webSocketCameraUrl = "ws:\/\/" + window.location.hostname + "/Camera";
        var webSocketServoInputUrl = "ws:\/\/" + window.location.hostname + "/ServoInput";
        var websocketCamera;
        var websocketServoInput;

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

        function initCameraWebSocket() 
        {
            websocketCamera = new WebSocket(webSocketCameraUrl);
            websocketCamera.binaryType = 'blob';
            websocketCamera.onopen = function (event) { };
            websocketCamera.onclose = function (event) { setTimeout(initCameraWebSocket, 2000); };
            websocketCamera.onmessage = function (event) {
                var imageId = document.getElementById("cameraImage");
                imageId.src = URL.createObjectURL(event.data);
            };
        }

        function initServoInputWebSocket() {
            websocketServoInput = new WebSocket(webSocketServoInputUrl);
            websocketServoInput.onopen = function (event) 
            {
                var panButton = document.getElementById("Pan");
                sendButtonInput("Pan", panButton.value);
                var tiltButton = document.getElementById("Tilt");
                sendButtonInput("Tilt", tiltButton.value);
                var lightButton = document.getElementById("Light");
                sendButtonInput("Light", lightButton.value);
            };
            websocketServoInput.onclose = function (event) { setTimeout(initServoInputWebSocket, 2000); };
            websocketServoInput.onmessage = function (event) { };
        }

        function initWebSocket() {
            initCameraWebSocket();
            initServoInputWebSocket();
        }

        function sendButtonInput(key, value) {
            var data = key + "," + value;
            websocketServoInput.send(data);
        }

        window.onload = initWebSocket;
        document.getElementById("mainGrid").addEventListener("touchend", function (event) {
            event.preventDefault()
        });      
    </script>
</body>
</html>
)rawliteral";

// ISR
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  //Accion a realizar en interrupcion
  takeNewPhoto_auto = true;
  // control(modo_actual);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void classifyImage(camera_fb_t * &foto) {
  // camera_fb_t * foto = takePhoto();
  Serial.println();
  Serial.println("Analizando foto...");
  

  size_t size = foto->len;
  String buffer = base64::encode((uint8_t *) foto->buf, foto->len);
  
  String payload = "{\"inputs\": [{ \"data\": {\"image\": {\"base64\": \"" + buffer + "\"}}}]}";

  buffer = "";

  // Serial.println(payload);
  
  // Generic model
  String model_id = "aaa03c23b3724a16a56b629203edc62c";
  

  // if(WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    // http.setTimeout(1500);
    http.begin("https://api.clarifai.com/v2/models/" + model_id + "/outputs");
    http.addHeader("Content-Type", "application/json");     
    http.addHeader("Authorization", "Key 989744553cda4d7fbaa53e54c542791d"); 
    int response_code = http.POST(payload);
    // delay(2000);

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
      Serial.println(http.errorToString(response_code).c_str());
      return;
    }
  // }
  // // else {
  //   Serial.println("wifi no conectado!!!");
  //   return;
  // }

  // Arduino assistant para determinar tamanio necesario del JSONdocument
  // DynamicJsonDocument doc(jsonSize);
  DynamicJsonDocument doc(16384);
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
    Serial.print("Nombre: ");    
    Serial.println(name[i]);
    // Serial.println("/");
    Serial.print("Prob: ");
    Serial.println(p[i]);
    // Serial.println();
  }
  
  const char* no_pers[1] = {"Ninguna persona"};
  for (int j = 0; j < 5; j++) {
    // Serial.println("entra FOR");
    // Serial.print("-->");
    // Serial.print(name[j]);
    // Serial.print(" == ");
    // Serial.println(no_pers[0]);
    out_strcmp = (strcmp(name[j], no_pers[0]));
    // Serial.print("resultado strcmp(): ");
    // Serial.println(out_strcmp);
    if (out_strcmp == 0) {
      // Serial.println("entra IF");
      if (p[j] > 0.90) {
        cont++;
        last = true;
        Serial.print("Contador: ");
        Serial.println(cont);
        if ((cont >= cont_max) && (last)) {
          // Suena alarma
          Serial.println("---------------------------------------");
          Serial.println("---- ALARMA ALARMA ALARMA !!!!!!!! ----");
          Serial.println("---------------------------------------");
          for (int k = 0; k < 3; k++) {
            digitalWrite(ALARM_PIN, HIGH);
            delay(1000);
            digitalWrite(ALARM_PIN, LOW);
            delay(1000);
          } 
          // initSD();  
          saveSD(foto);
          cont = 0;
        }
        esp_camera_fb_return(foto);
        Serial.println("\nFinalizo analisis de foto");
        return;
      }
    }
    if ((cont >  0) && (last == false)) {
        cont = 0;
    }    
  }
  
  last = false;
  esp_camera_fb_return(foto);

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
    // Serial.println(" ");
    // Serial.println("Capturando foto...");
    fb = esp_camera_fb_get();
    

    if (!fb) {
      Serial.println("Captura fallada !");
      // break;
    }
    else {
      // Serial.println("Foto capturada");
    }
    delay(10);

    return fb;
}

void initSD () {
  // SD init
  // Serial.println("Starting SD Card...");
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

  Serial.println("Tarjeta SD iniciada");
}

void saveSD(camera_fb_t * &fb) {
  // Initialize EEPROM with predefined size
  // EEPROM.begin(EEPROM_SIZE);
  // pictureNumber = pictureNumber + 1;

  String fecha_hora = rtc.getTime("%d-%m-%Y--%H:%M:%S");

  // Path where new picture will be saved in SD Card
  // String path = "/picture" + String(pictureNumber) +".jpg";
  String path = "/np-" + fecha_hora +".jpg";
  // Serial.println(path);

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

//Capture Photo and Save it to SD
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

    // check if file has been correctly saved in SPIFFS
    // ok = checkPhoto(SPIFFS);
  // } while ( !ok );
  return;

  
}

void changeMode() {
  // modo_actual = modo_actual;
   if (modo_actual == MODO_AUTO) {
      modo_actual = MODO_MANUAL;
      Serial.println();
      Serial.println("Modo MANUAL activado !");
      // control(modo_actual);
    }
    else {
      modo_actual = MODO_AUTO;
      Serial.println();
      Serial.println("Modo AUTOMATICO activado !");
      // control(modo_actual);
    }
}

/*
void control(MODOS modo_actual) {
  Serial.print("Control con: ");
  Serial.println(modo_actual);
  switch (modo_actual) {
    case MODO_AUTO:
      timerAlarmEnable(timer);
      Serial.println("control en modo_auto---");
      // timerWrite(timer, 0)
      if (takeNewPhoto_auto) {
        Serial.println("control en modo_auto dentro del if---");
        fb = takePhoto();
        delay(10);
        classifyImage(fb);
        takeNewPhoto_auto = false;
      }
      break;

    case MODO_MANUAL:
    Serial.println("control en modo_manual---");
      timerAlarmDisable(timer);  
      takeNewPhoto_manual = true; 
      while (modo_actual == MODO_MANUAL) {
        if (takeNewPhoto_manual) {
          Serial.println("control en modo_manual dentro del if---");
          fb = takePhoto();
          delay(10);
          saveSPIFFS_SD(fb);
          takeNewPhoto_manual = false;
        }
      }
      delay(10);   
      break;
  }

  delay(10);
}
*/

void onServoInputWebSocketEvent(AsyncWebSocket *server,
                                AsyncWebSocketClient *client,
                                AwsEventType type,
                                void *arg,
                                uint8_t *data,
                                size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      // Serial.printf("WebSocket client #%u disconnected\n", client->id());
      panServo.write(90);
      tiltServo.write(90);
      ledcWrite(PWMLightChannel, 0);
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        std::string myData = "";
        myData.assign((char *)data, len);
        Serial.printf("Key,Value = [%s]\n", myData.c_str());
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        if (value != "") {
          int valueInt = atoi(value.c_str());
          if (key == "Pan") {
            panServo.write(valueInt);
          } else if (key == "Tilt") {
            tiltServo.write(valueInt);
          } else if (key == "Light") {
            ledcWrite(PWMLightChannel, valueInt);
          }
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server,
                            AsyncWebSocketClient *client,
                            AwsEventType type,
                            void *arg,
                            uint8_t *data,
                            size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      // Serial.printf("WebSocket client #%u disconnected\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;
  }
}

void setUpPinModes() {
  dummyServo1.attach(DUMMY_SERVO1_PIN);
  dummyServo2.attach(DUMMY_SERVO2_PIN);
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);

  //Set up flash light
  ledcSetup(PWMLightChannel, 1000, 8);
  pinMode(LIGHT_PIN, OUTPUT);
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);

  //Set up alarm light
  pinMode(ALARM_PIN, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
}

void setUpWifi() {
  switch (tipo_wifi) {
    case _LOCAL_WIFI: 
    {
    //   const char* ssid = "EL RANCHO";
    //   const char* password = "sofimaxi2";

      const char* ssid = "Maxx Samsung";
      const char* password = "fwpb7977";

      WiFi.begin(ssid, password);
      Serial.print("Conectando a WiFi");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
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
      const char* ssid = "RealidadVirtual";
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
}

void setUpTimer() {
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, t*1000000, true);
  timerAlarmEnable(timer);
}

void setupCamera() {
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
  config.xclk_freq_hz = 5000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 10;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Falla en inicializacion de camara - Error: 0x%x", err);
    // ESP.restart();
    return;
  }

  if (psramFound()) {
    heap_caps_malloc_extmem_enable(20000);
    // Serial.printf("PSRAM initialized. malloc to take memory from psram above this size");
    Serial.println("Camara inicializada");
  }
}

void setUpSpiffs() {
   // Begin SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    // Serial.println("SPIFFS mounted successfully");
    Serial.println("SPIFFS montada exitosamente");
  }
}

void sendCameraPicture(camera_fb_t * fb) {
  if (cameraClientId == 0) {
    return;
  }
  // unsigned long startTime1 = millis();
  //capture a frame
  // camera_fb_t *fb = esp_camera_fb_get();
  // if (!fb) {
  //   Serial.println("Frame buffer could not be acquired");
  //   return;
  // }

  // unsigned long startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  // esp_camera_fb_return(fb);

  //Wait for message to be delivered
  while (true) {
    AsyncWebSocketClient *clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull())) {
      break;
    }
    delay(1);
  }

  // unsigned long startTime3 = millis();
  //Serial.printf("Time taken Total: %d|%d|%d\n", startTime3 - startTime1, startTime2 - startTime1, startTime3 - startTime2);
}

void setTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("___________________________________________");

  setUpPinModes();
  setUpWifi();

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
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    // ESP.restart();
    return;
  }

  setUpTimer();
  // setupCamera();
  setUpSpiffs();
  initSD ();
  setTime();

  modo_actual = MODO_AUTO;

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not Found");
  });

  wsCamera.onEvent(onCameraWebSocketEvent); //Atiende evento de 'camara'
  server.addHandler(&wsCamera);

  wsServoInput.onEvent(onServoInputWebSocketEvent); //Atiende evento de 'servmotores'
  server.addHandler(&wsServoInput);

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
    Serial.println("Pagina actualizada!");
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();
  Serial.println("Servidor HTTP inicializado");

  // control(modo_actual);
  switch (modo_actual) {
    case MODO_AUTO:
      Serial.println();
      Serial.println("Modo Automatico !");
      Serial.println();
      break;
    case MODO_MANUAL:
      Serial.println();
      Serial.println("Modo Manual !");
      Serial.println();
      break;
  }
}

void loop() {
  // while (modo_actual == MODO_MANUAL) {
  //   if (takeNewPhoto_manual) {
  //     fb = takePhoto();
  //     saveSPIFFS_SD(fb);
  //     takeNewPhoto_manual = false;
  //   }
  // }
  // delay(10);

  wsCamera.cleanupClients();
  wsServoInput.cleanupClients();
  fb = takePhoto();
  delay(10);
  sendCameraPicture(fb);

  // if ((takeNewPhoto_auto) || (takeNewPhoto_manual)) {

    switch (modo_actual) {
      case MODO_AUTO:
        // timerAlarmEnable(timer);
        // Serial.println("control en modo_auto---");
        // timerWrite(timer, 0)
        if (takeNewPhoto_auto) {
          // Serial.println("control en modo_auto dentro del if---");
          // fb = takePhoto();
          // delay(10);
          classifyImage(fb);
          takeNewPhoto_auto = false;
        }
        else { esp_camera_fb_return(fb); }
        break;

      case MODO_MANUAL:
      // Serial.println("control en modo_manual---");
        // timerAlarmDisable(timer);  
        // takeNewPhoto_manual = true; 
        // while (modo_actual == MODO_MANUAL) {
          if (takeNewPhoto_manual) {
            // Serial.println("control en modo_manual dentro del if---");
            // fb = takePhoto();
            // delay(10);
            saveSPIFFS(fb);
            // saveSD(fb);
            takeNewPhoto_manual = false;
          }
          else { esp_camera_fb_return(fb); }
        // } 
        break;
    }
  // }
  

  delay(10);
}
