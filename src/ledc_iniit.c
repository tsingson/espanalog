
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
// LEDC 初始化函数
void ledc_init(int LED_PIN) {
  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                    .duty_resolution =
                                        LEDC_TIMER_13_BIT, // 2^13 = 8192
                                    .timer_num = LEDC_TIMER_0,
                                    .freq_hz = 5000, // 5kHz PWM
                                    .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {.gpio_num = LED_PIN,
                                        .speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = LEDC_CHANNEL_0,
                                        .timer_sel = LEDC_TIMER_0,
                                        .duty = 0,
                                        .hpoint = 0};
  ledc_channel_config(&ledc_channel);
}
