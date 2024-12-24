#include "esp32s3_bsp_board.h"
#include "nvs_flash.h"
#include <esp_err.h>

esp_err_t bsp_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return ret;
}

void bsp_board_init(void)
{
    bsp_nvs_init();
    esp32s3_bsp_sdcard_init();
    esp32s3_bsp_mic_init();
    //esp32s3_bsp_camera_init();
    esp32s3_bsp_speaker_init();
    esp32s3_bsp_tft_display_init();
}




