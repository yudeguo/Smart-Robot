#include "esp32s3_bsp_board.h"
#include "driver/i2s_pdm.h"

static i2s_chan_handle_t i2s_rx_handle;



void esp32s3_bsp_mic_init(void)
{
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &i2s_rx_handle));
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(ESP32S3_BSP_PDM_RX_FREQ_HZ),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = ESP32S3_BSP_PDM_CLOCK_NUM,
            .din = ESP32S3_BSP_PDM_DIN_NUM,
            .invert_flags = {
                .clk_inv = false
            }
        }
    };
    pdm_rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    pdm_rx_cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(i2s_rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(i2s_rx_handle));
}

esp_err_t esp32s3_bsp_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    return i2s_channel_read(i2s_rx_handle, audio_buffer, len, bytes_read, timeout_ms);
}
