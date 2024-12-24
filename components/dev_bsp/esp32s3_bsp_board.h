#ifndef ESP32S3_BSP_BOARD_H
#define ESP32S3_BSP_BOARD_H
#include <esp_err.h>
#include <stdbool.h>

/*SDCARD INIT INFOS*/
#define ESP32S3_BSP_SDCARD_MISO_NUM     8
#define ESP32S3_BSP_SDCARD_MOSI_NUM     9
#define ESP32S3_BSP_SDCARD_SCLK_NUM     7
#define ESP32S3_BSP_SDCARD_CSN_NUM      21
#define ESP32S3_SDCARD_MOUNT_POINT      "/sdcard"

void esp32s3_bsp_sdcard_init(void);
bool esp32s3_get_sdcard_status(void);
bool esp32s3_bsp_sdcard_mount(void);

/*MICROPHONE INIT*/
#define ESP32S3_BSP_PDM_CLOCK_NUM           42
#define ESP32S3_BSP_PDM_DIN_NUM             41
#define ESP32S3_BSP_PDM_RX_FREQ_HZ          16000
void esp32s3_bsp_mic_init(void);
esp_err_t esp32s3_bsp_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);

/*Camera ov2610 init*/
#define ESP32S3_BSP_CAM_PIN_PWDN           (-1)
#define ESP32S3_BSP_CAM_PIN_RST            (-1)
#define ESP32S3_BSP_CAM_PIN_XCLK           10
#define ESP32S3_BSP_CAM_PIN_SIOD           40
#define ESP32S3_BSP_CAM_PIN_SIOC           39
#define ESP32S3_BSP_CAM_PIN_D9             48
#define ESP32S3_BSP_CAM_PIN_D8             11
#define ESP32S3_BSP_CAM_PIN_D7             12
#define ESP32S3_BSP_CAM_PIN_D6             14
#define ESP32S3_BSP_CAM_PIN_D5             16
#define ESP32S3_BSP_CAM_PIN_D4             18
#define ESP32S3_BSP_CAM_PIN_D3             17
#define ESP32S3_BSP_CAM_PIN_D2             15
#define ESP32S3_BSP_CAM_PIN_VSYNC          38
#define ESP32S3_BSP_CAM_PIN_HREF           47
#define ESP32S3_BSP_CAM_PIN_PCLK           13
#define ESP32S3_BSP_CAM_XCLK_FREQ_HZ       20000000
void esp32s3_bsp_camera_init(void);

/*Speaker init*/
#define ESP32S3_BSP_SPEAKER_PIN_BCK             1
#define ESP32S3_BSP_SPEAKER_PIN_WS              3
#define ESP32S3_BSP_SPEAKER_PIN_DATA            2
#define ESP32S3_BSP_SPEAKER_SAMPLE_RATE_HZ      16000
void esp32s3_bsp_speaker_init(void);
esp_err_t esp32s3_bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_write, uint32_t timeout_ms);
esp_err_t esp32s3_bsp_i2s_stop(void);

/* TFT Display GC9A01*/
#define ESP32S3_BSP_SPI_PIN_MOSI                 5
#define ESP32S3_BSP_SPI_PIN_CLK                  6
#define ESP32S3_BSP_SPI_PIN_DC                   4
#define ESP32S3_BSP_SPI_PIN_CS                   (-1)
#define ESP32S3_BSP_SPI_PIN_RST                  44
#define ESP32S3_BSP_TFT_H_RES                    240
#define ESP32S3_BSP_TFT_V_RES                    240

void esp32s3_bsp_tft_display_init(void);
esp_err_t esp32s3_draw_image(const void *color_data);

esp_err_t bsp_nvs_init(void);
void bsp_board_init(void);

#endif //ESP32S3_BSP_BOARD_H
