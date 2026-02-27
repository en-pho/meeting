#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "esp_system.h"   // reset reason

/************** USER SETTINGS **************/
#define SLEEP_MINUTES 5
#define WIFI_TIMEOUT 15000

#define LED_PIN 2
#define LED_OFF_DELAY 1

// /* 可选：固定IP（不想固定就注释掉） */
// //#define USE_STATIC_IP
// IPAddress local_IP(192,168,1,222);
// IPAddress gateway(192,168,1,1);
// IPAddress subnet(255,255,255,0);

// /* 可选：局域网域名 */
const char* HOSTNAME = "control.aurorum.co";

/************** CAMERA PINS **************/
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_Y2      11
#define CAM_PIN_Y3      9
#define CAM_PIN_Y4      8
#define CAM_PIN_Y5      10
#define CAM_PIN_Y6      12
#define CAM_PIN_Y7      18
#define CAM_PIN_Y8      17
#define CAM_PIN_Y9      16
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

Preferences prefs;
WebServer server(80);

String ssid, pass, url, token;

/************** CAMERA INIT **************/
bool initCamera(){
  Serial.println("Init camera...");

  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = CAM_PIN_Y2;
  c.pin_d1 = CAM_PIN_Y3;
  c.pin_d2 = CAM_PIN_Y4;
  c.pin_d3 = CAM_PIN_Y5;
  c.pin_d4 = CAM_PIN_Y6;
  c.pin_d5 = CAM_PIN_Y7;
  c.pin_d6 = CAM_PIN_Y8;
  c.pin_d7 = CAM_PIN_Y9;
  c.pin_xclk = CAM_PIN_XCLK;
  c.pin_pclk = CAM_PIN_PCLK;
  c.pin_vsync = CAM_PIN_VSYNC;
  c.pin_href = CAM_PIN_HREF;
  c.pin_sccb_sda = CAM_PIN_SIOD;
  c.pin_sccb_scl = CAM_PIN_SIOC;
  c.pin_pwdn = CAM_PIN_PWDN;
  c.pin_reset = CAM_PIN_RESET;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  // 你现在用 VGA；如果 HTTPS 还炸，先临时改 QVGA 验证稳定性
  c.frame_size = FRAMESIZE_VGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;

  esp_err_t err = esp_camera_init(&c);
  if(err != ESP_OK){
    Serial.print("Camera init FAILED, err=");
    Serial.println((int)err);
    return false;
  }

  Serial.println("Camera OK");
  return true;
}

/************** CONFIG PORTAL **************/
String page(){
  return R"(
  <html><body>
  <h2>Camera Setup</h2>
  <form method='POST' action='/save'>
  WiFi SSID:<br><input name='s'><br>
  Password:<br><input name='p'><br>
  Server URL:<br><input name='u'><br>
  API Token:<br><input name='t'><br>
  <input type='submit'>
  </form></body></html>
  )";
}

void startPortal(){
  Serial.println("Starting config AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CarCam-Setup");

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", [](){ server.send(200,"text/html",page()); });

  server.on("/save", HTTP_POST, [](){
    prefs.begin("cfg", false);
    prefs.putString("s", server.arg("s"));
    prefs.putString("p", server.arg("p"));
    prefs.putString("u", server.arg("u"));
    prefs.putString("t", server.arg("t"));
    prefs.end();
    server.send(200,"text/plain","Saved. Rebooting.");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

/************** WIFI CONNECT **************/
bool connectWiFi(){
  Serial.println("Connecting WiFi...");

  // 避免省电模式导致 TLS/WiFi 不稳定（S3 上常见）
  WiFi.setSleep(false);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<WIFI_TIMEOUT){
    Serial.print(".");
    delay(500);
  }

  Serial.println();

  if(WiFi.status()==WL_CONNECTED){
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi FAILED");
  return false;
}

/************** HTTPS UPLOAD **************/
bool sendPhoto(){
  Serial.printf("Heap before capture: %u\n", ESP.getFreeHeap());
  Serial.printf("PSRAM free: %u\n", ESP.getFreePsram());

  Serial.println("Capturing...");
  camera_fb_t* fb = esp_camera_fb_get();
  if(!fb){
    Serial.println("Capture failed");
    return false;
  }

  Serial.print("JPEG bytes: ");
  Serial.println((unsigned)fb->len);

  uint8_t* jpg = (uint8_t*)malloc(fb->len);
  if(!jpg){
    Serial.println("malloc failed");
    esp_camera_fb_return(fb);
    return false;
  }

  size_t jpgLen = fb->len;
  memcpy(jpg, fb->buf, jpgLen);

  // 关键：尽早释放 camera buffer，给 TLS 留空间
  esp_camera_fb_return(fb);

  Serial.printf("Heap before HTTPS: %u\n", ESP.getFreeHeap());
  Serial.println("Uploading HTTPS...");

  // 喂一下调度，避免长时间阻塞引发看门狗（尤其 TLS 握手时）
  delay(10);

  WiFiClientSecure client;
  client.setInsecure();

  // 降低“卡死概率”：握手/读写超时
  client.setTimeout(10);
  client.setHandshakeTimeout(10);

  HTTPClient http;
  http.setTimeout(8000);

  // begin 也可能失败（DNS/TLS/内存）
  if(!http.begin(client, url)){
    Serial.println("http.begin failed");
    free(jpg);
    return false;
  }

  http.addHeader("Content-Type","image/jpeg");
  http.addHeader("X-API-Key", token);
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "ESP32S3-CarCam/1.0");

  int code = http.POST(jpg, jpgLen);

  free(jpg);
  http.end();

  Serial.print("Server response: ");
  Serial.println(code);

  Serial.printf("Heap after POST: %u\n", ESP.getFreeHeap());
  Serial.printf("PSRAM free: %u\n", ESP.getFreePsram());

  // -1 / 0 常见代表连接/解析失败，但现在至少你能看到码了
  return code==200;
}

/************** SETUP **************/
void setup(){
  Serial.begin(115200);
  delay(500);

  // 打印重启原因：这是你现在定位“为什么自己断”的关键证据
  Serial.println();
  Serial.print("Reset reason (CPU0): ");
  Serial.println((int)esp_reset_reason());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("\nBooting...");

  delay(LED_OFF_DELAY*1000);
  digitalWrite(LED_PIN, LOW);

  prefs.begin("cfg", true);
  ssid = prefs.getString("s","");
  pass = prefs.getString("p","");
  url  = prefs.getString("u","");
  token= prefs.getString("t","");
  prefs.end();

  if(ssid==""){
    Serial.println("No config found");
    startPortal();
    return;
  }

  if(!connectWiFi()){
    startPortal();
    return;
  }

  if(!initCamera()){
    startPortal();
    return;
  }

  if(!sendPhoto()){
    Serial.println("Upload failed");
    // 失败不立刻死循环；直接进入睡眠，避免反复重启刷屏/断连
  }

  Serial.println("Sleeping...");
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_MINUTES*60ULL*1000000ULL);
  esp_deep_sleep_start();
}

/************** LOOP **************/
void loop(){
  server.handleClient();
}
