/*********

1° y 2°: Capturar imagen con boton de servidos web, guardarla en SD y visualiarla en servidor web

*********/

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

// define the number of bytes you want to access
#define EEPROM_SIZE 1

#define WIFI_LOCAL
// #define WIFI_AP

#if defined(WIFI_LOCAL)
  // Replace with your network credentials
  const char* ssid = "EL RANCHO";
  const char* password = "sofimaxi2";
#elif defined(WIFI_AP)
  const char* ssid = "RVmejoras";
  const char* password = "123456";
#endif


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto = false;

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
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
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
</html>)rawliteral";

void setup() {
    Serial.begin(115200);

  // Connect to Wi-Fi
  #if defined(WIFI_LOCAL)
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    }
    // Print ESP32 Local IP Address
    Serial.print("Conexion local establecida");
    Serial.println("Local - Direccion IP: http://");
    Serial.println(WiFi.localIP());

  #elif defined(WIFI_AP)  
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    // Print ESP32 AP IP Address
    Serial.print("Listo para conectarse a red: " + String(ssid));
    Serial.println(" ");
    Serial.println("Contraseña: " + String(password));
    Serial.println("Acces Point - Direccion IP: http://");
    Serial.println(IP);
  #endif



  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    // Serial.println("SPIFFS mounted successfully");
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
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

  // SD init
  //Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    ESP.restart();
    // return;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();
  Serial.println("Server started!");

}

void loop() {
  // put your main code here, to run repeatedly:
  if (takeNewPhoto) {
    capturePhoto();
    takeNewPhoto = false;
  }
  delay(1);
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

//Capture Photo and Save it to SD
void capturePhoto( void ) {
  camera_fb_t * fb = NULL;
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println(" ");
    Serial.println("Taking a photo...");
    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
    // EEPROM.write(0, 0);
    pictureNumber = EEPROM.read(0) + 1;

    // Path where new picture will be saved in SD Card
    String path = "/picture" + String(pictureNumber) +".jpg";

    fs::FS &fs = SD_MMC; 

    // Photo file name
    Serial.printf(">Picture file name in SPIFFS: %s\n", FILE_PHOTO);
    Serial.printf(">Picture file name in SD: %s\n", path.c_str());
    File file_spiffs = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
    File file_sd = fs.open(path.c_str(), FILE_WRITE);

    if (!file_spiffs) {
      Serial.println("Failed to open file_spiffs in writing mode");
    }
    else {
      file_spiffs.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print(">>Saved file to SPIFFS in path: ");
      Serial.print(FILE_PHOTO);
      // Serial.print(" - Size: ");
      // Serial.print(file.size());
      // Serial.println(" bytes");
    }

    // Insert the data in the photo file
    if (!file_sd) {
      Serial.println("Failed to open file_sd in writing mode");
    }
    else {
    file_sd.write(fb->buf, fb->len); // payload (image), payload length
    Serial.println("");
    Serial.printf(">>Saved file to SD in path: %s\n", path.c_str());
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
    }
    

    // Close the file_spiffs
    file_spiffs.close();
    file_sd.close();
    esp_camera_fb_return(fb);

    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}