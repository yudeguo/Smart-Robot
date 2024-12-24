#include <esp_vfs_fat.h>

#include "esp32s3_bsp_board.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/sdmmc_host.h"

typedef struct sdcard_info_t
{
    sdmmc_card_t *card;
    sdmmc_host_t host;
    bool is_mounted;
    SemaphoreHandle_t sdcard_mux;
}SDCARD_HANDLE;

static SDCARD_HANDLE sdcard_handle={NULL,SDSPI_HOST_DEFAULT(),false,NULL};

static bool bsp_sdcard_lock(uint32_t timeout_ms)
{
    assert(sdcard_handle.sdcard_mux && "bsp_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(sdcard_handle.sdcard_mux, timeout_ticks) == pdTRUE;
}

static void bsp_sdcard_unlock(void)
{
    assert(sdcard_handle.sdcard_mux && "bsp_display_start must be called first");
    xSemaphoreGiveRecursive(sdcard_handle.sdcard_mux);
}

bool esp32s3_get_sdcard_status(void)
{
    bsp_sdcard_lock(0);
    const bool status = sdcard_handle.is_mounted;
    bsp_sdcard_unlock();
    return status;
}

static void set_sdcard_status(bool status)
{
    bsp_sdcard_lock(0);
    sdcard_handle.is_mounted = status;
    bsp_sdcard_unlock();
}

bool esp32s3_bsp_sdcard_mount(void)
{
    const char mount_point [] = ESP32S3_SDCARD_MOUNT_POINT;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = ESP32S3_BSP_SDCARD_CSN_NUM;
    slot_config.host_id = sdcard_handle.host.slot;
    const esp_err_t ret = esp_vfs_fat_sdspi_mount(mount_point, &sdcard_handle.host, &slot_config, &mount_config, &sdcard_handle.card);
    set_sdcard_status(ret == ESP_OK);
    return (ret == ESP_OK);
}

void esp32s3_bsp_sdcard_init(void)
{
    sdcard_handle.sdcard_mux = xSemaphoreCreateRecursiveMutex();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = ESP32S3_BSP_SDCARD_MOSI_NUM,
        .miso_io_num = ESP32S3_BSP_SDCARD_MISO_NUM,
        .sclk_io_num = ESP32S3_BSP_SDCARD_SCLK_NUM,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(sdcard_handle.host.slot, &bus_cfg, SDSPI_DEFAULT_DMA));
}
