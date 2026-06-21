#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/adc.h"
#include "driver/ledc.h" // 新增头文件

#define ADC_PIN ADC1_CHANNEL_6
#define LED_PIN 4        // D4 引脚
#define QUEUE_LENGTH 10

static QueueHandle_t adc_queue;

// LEDC 初始化函数
void ledc_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT, // 2^13 = 8192
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,              // 5kHz PWM
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LED_PIN,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

// 生产者 Task 保持不变
void adc_reader_task(void *pvParameter) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN, ADC_ATTEN_DB_11);
    int last_mapped_value = -1;
    while (1) {
        int raw_value = adc1_get_raw(ADC_PIN);
        int mapped_value = (raw_value * 15) / 4095;
        if (mapped_value != last_mapped_value) {
            xQueueSend(adc_queue, &mapped_value, 0);
            last_mapped_value = mapped_value;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 消费者 Task：新增 PWM 控制逻辑
void uart_logger_task(void *pvParameter) {
    int received_value;
    while (1) {
        if (xQueueReceive(adc_queue, &received_value, portMAX_DELAY)) {
            // 打印
            printf("Mapped Value: %d\n", received_value);

            // 映射 0-15 到 0-8191 (13位分辨率)
            // 公式: (value / 15) * 8191
            uint32_t duty = (received_value * 8191) / 15;

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }
    }
}

void app_main() {
    // 关键修复：加入延时以等待系统完全稳定，抑制上电乱码
    vTaskDelay(pdMS_TO_TICKS(500));

    // 初始化硬件
    ledc_init();

    // 创建队列
    adc_queue = xQueueCreate(QUEUE_LENGTH, sizeof(int));

    // 创建任务
    xTaskCreate(adc_reader_task, "adc_reader", 2048, NULL, 5, NULL);
    xTaskCreate(uart_logger_task, "uart_logger", 2048, NULL, 5, NULL);
}
