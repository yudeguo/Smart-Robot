#include "esp32s3_bsp_board.h"
#include <driver/i2s_common.h>
#include <driver/i2s_std.h>

static i2s_chan_handle_t i2s_tx_chan;        // I2S tx channel handler

void esp32s3_bsp_speaker_init(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear_before_cb = true;
    tx_chan_cfg.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &i2s_tx_chan, NULL));
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(ESP32S3_BSP_SPEAKER_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = ESP32S3_BSP_SPEAKER_PIN_BCK,
            .ws   = ESP32S3_BSP_SPEAKER_PIN_WS,
            .dout = ESP32S3_BSP_SPEAKER_PIN_DATA,
            .din  = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false},
        }
    };
    tx_std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    tx_std_cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_chan, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
}

esp_err_t esp32s3_bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_write, uint32_t timeout_ms)
{
    return i2s_channel_write(i2s_tx_chan, audio_buffer, len, bytes_write, timeout_ms);
}

esp_err_t esp32s3_bsp_i2s_stop(void)
{
    return i2s_channel_disable(i2s_tx_chan);
}



