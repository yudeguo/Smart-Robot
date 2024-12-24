#include <driver/spi_common.h>
#include "esp_lcd_gc9a01.h"
#include <esp_lcd_io_spi.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_ops.h>
#include "esp32s3_bsp_board.h"

#define LCD_HOST    SPI3_HOST
static esp_lcd_panel_handle_t panel_handle;
static esp_lcd_panel_io_handle_t io_handle = NULL;
void esp32s3_bsp_tft_display_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ESP32S3_BSP_SPI_PIN_MOSI,
        .sclk_io_num = ESP32S3_BSP_SPI_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
       // .max_transfer_sz = 240 *sizeof(uint16_t), // 每次传输一行),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = ESP32S3_BSP_SPI_PIN_DC,
        .cs_gpio_num = ESP32S3_BSP_SPI_PIN_CS,
        .pclk_hz = 10 * 1000 * 1000,
        .spi_mode = 0,
        .trans_queue_depth = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((spi_device_handle_t)LCD_HOST, &io_config, &io_handle));
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = ESP32S3_BSP_SPI_PIN_RST,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

esp_err_t esp32s3_draw_image(const void *color_data)
{
    return esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 240, 240, color_data);
}