#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>

#include <CallbackFunction.h>
#include <WemoManager.h>
#include <WemoSwitch.h>
#include <HTTPClient.h>

#define LED_BUILTIN 33
#define FLASH 4

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
const char *ifttt_key = "YOUR_KEY";
const char *ifttt_event = "richnotice";
const char *mdns_name = "esppatio";
const char *alexa_name = "camera one";
const char *alexa_name_url = "Camera+One";

// Camera wemo related declarations
WemoSwitch *cameraDevice = NULL;
WemoSwitch *ledDevice = NULL;
WemoManager wemoManager;
TaskHandle_t wemoTask;
void wemoLoop(void * pvParameters);
void cameraOn();
void cameraOff();
void LEDOn();
void LEDOff();
String httpGETRequest(const char* serverName);

TaskHandle_t cameraTask;
void cameraLoop(void * pvParameters);
void startCameraServer(int port);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(FLASH, OUTPUT);

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
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  
  // overwrite default settings
  s->set_framesize(s, FRAMESIZE_UXGA);
  s->set_exposure_ctrl(s, 0);
  s->set_aec_value(s, 1200);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  WiFi.begin(ssid, password);

  bool isHigh = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    digitalWrite(LED_BUILTIN, isHigh ? HIGH: LOW);
    isHigh = !isHigh;
  }
  Serial.println("");
  Serial.println("WiFi connected");

  if(!MDNS.begin(mdns_name)) {
     Serial.println("Error starting mDNS");
     return;
  }

  // emulate a wemo device
  wemoManager.begin();
  cameraDevice = new WemoSwitch(alexa_name, 80, cameraOn, cameraOff);
  wemoManager.addDevice(*cameraDevice);
//  ledDevice = new WemoSwitch("camera one flash", 81, LEDOn, LEDOff);
//  wemoManager.addDevice(*ledDevice);

  // Set up task to run the wemo server loop at core 0
  xTaskCreatePinnedToCore(
                  wemo_loop,   /* Task function. */
                  "Wemo",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &wemoTask,      /* Task handle to keep track of created task */
                  0);          /* pin task to core 0 */   
  delay(500);

  xTaskCreatePinnedToCore(
                  camera_loop,   /* Task function. */
                  "Camera",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &cameraTask,      /* Task handle to keep track of created task */
                  1);          /* pin task to core 0 */   
  delay(500);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10000);
}

void camera_loop(void * pvParameters) {
  Serial.print("Camera running on core ");
  Serial.println(xPortGetCoreID());
  
  startCameraServer(8080);

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  while(true) {
    delay(10000);
  }
}

void wemo_loop(void * pvParameters) {
  Serial.print("Wemo running on core ");
  Serial.println(xPortGetCoreID());
  while(true) {
    wemoManager.serverLoop();
  }
}

void cameraOn() {
  char url_template[] = "https://maker.ifttt.com/trigger/%s/with/key/%s/?value1=%s+On&value2=http%%3A%%2F%%2F%s.local%%3A8080%%2F&value3=http%%3A%%2F%%2F%s.local%%3A8080%%2Fcapture";
  const size_t buff_len = sizeof(url_template) + strlen(ifttt_event) + strlen(mdns_name) + strlen(mdns_name) + strlen(ifttt_key) + strlen(alexa_name_url) + 1;
  char *buff = (char *)malloc(buff_len);
  if (!buff) {
    Serial.println("Error: cannot allocate memory for the URL.");
    return;
  }
  size_t n = sprintf(buff, url_template, ifttt_event, ifttt_key, alexa_name_url, mdns_name, mdns_name);
  Serial.println(httpGETRequest(buff));
  free(buff);
  buff = NULL;

  // manually turn off the camera so it could be turn on again
  cameraDevice->turnOff();
}

void cameraOff(){
  // Do nothing
}

void LEDOn() {
  digitalWrite(FLASH, HIGH);
}

void LEDOff() {
  digitalWrite(FLASH, LOW);
}

String httpGETRequest(const char* serverName) {
  Serial.print("Sending request to ");
  Serial.println(serverName);

  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  String payload = "--"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
