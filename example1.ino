#include "esp_camera.h"

/*
  这一步是定义摄像头接到哪些GPIO
  不同开发板要改这里
*/
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

void setup() {
  Serial.begin(115200);

  /*
    Step 1: 创建摄像头配置结构体
    这里决定：
      - 图像大小
      - 图像格式
      - 引脚连接
  */
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

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

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  /*
    Step 2: 设置相机参数
    JPEG格式最省内存
  */
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  /*
    Step 3: 初始化摄像头
    这一步失败就啥也干不了
  */
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.println("Camera init failed!");
    return;
  }

  Serial.println("Camera initialized!");
}

void loop() {

  /*
    Step 4: 获取一帧图像
    返回的是一个指针，指向图像数据
  */
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Capture failed");
    return;
  }

  Serial.println("Photo captured!");
  Serial.printf("Size: %d bytes\n", fb->len);

  /*
    这里fb->buf 就是 JPEG 图片数据
    你可以：
      - 保存到SD卡
      - 发送到WiFi
      - 发送到电脑
  */

  /*
    Step 5: 一定要释放帧缓冲
    否则内存会被占满
  */
  esp_camera_fb_return(fb);

  delay(3000);
}
