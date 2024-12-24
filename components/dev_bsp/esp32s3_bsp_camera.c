#include "esp32s3_bsp_board.h"
#include "esp_camera.h"

void esp32s3_bsp_camera_init(void)
{
    camera_config_t config={};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = ESP32S3_BSP_CAM_PIN_D2;
    config.pin_d1 = ESP32S3_BSP_CAM_PIN_D3;
    config.pin_d2 = ESP32S3_BSP_CAM_PIN_D4;
    config.pin_d3 = ESP32S3_BSP_CAM_PIN_D5;
    config.pin_d4 = ESP32S3_BSP_CAM_PIN_D6;
    config.pin_d5 = ESP32S3_BSP_CAM_PIN_D7;
    config.pin_d6 = ESP32S3_BSP_CAM_PIN_D8;
    config.pin_d7 = ESP32S3_BSP_CAM_PIN_D9;
    config.pin_xclk = ESP32S3_BSP_CAM_PIN_XCLK;
    config.pin_pclk = ESP32S3_BSP_CAM_PIN_PCLK;
    config.pin_vsync = ESP32S3_BSP_CAM_PIN_VSYNC;
    config.pin_href = ESP32S3_BSP_CAM_PIN_HREF;
    config.pin_sccb_sda = ESP32S3_BSP_CAM_PIN_SIOD;
    config.pin_sccb_scl = ESP32S3_BSP_CAM_PIN_SIOC;
    config.pin_pwdn = ESP32S3_BSP_CAM_PIN_PWDN;
    config.pin_reset = ESP32S3_BSP_CAM_PIN_RST;
    config.xclk_freq_hz = ESP32S3_BSP_CAM_XCLK_FREQ_HZ;
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 63;
    config.fb_count = 2;
    ESP_ERROR_CHECK(esp_camera_init(&config));
}
