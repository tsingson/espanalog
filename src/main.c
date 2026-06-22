#include "chipinfo.h"

#include "driver/adc.h"
#include "driver/i2c.h"
#include "driver/ledc.h" // 新增头文件
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ledc_init.h" // 新增 LEDC 初始化头文件
#include <esp_log.h>
#include <ssd1306.h>
#include <stdint.h>
#include <stdio.h>

// 队列句柄
QueueHandle_t oled_queue; // 电阻值变化, 发给 SSD1306 显示

#define ADC_PIN ADC1_CHANNEL_6
#define LED_PIN 4 // D4 引脚
#define QUEUE_LENGTH 10

static QueueHandle_t adc_queue;

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
//

char *uint32_to_string_malloc(uint32_t value) {
  char *buffer = (char *)malloc(12); // 分配内存
  if (buffer != NULL) {
    snprintf(buffer, 12, "%lu", value);
  }
  return buffer; // 返回堆指针
}
/**
char* res = uint32_to_string_malloc(123456);
if (res) {
printf("%s\n", res);
free(res); // 务必手动释放
}
*/

// 消费者 Task：新增 PWM 控制逻辑
void uart_led_control_task(void *pvParameter) {
  int received_value;

  while (1) {
    if (xQueueReceive(adc_queue, &received_value, portMAX_DELAY)) {
      // 打印
      printf("Mapped Value: %d\n", received_value);

      // 映射 0-15 到 0-8191 (13位分辨率)
      // 公式: (value / 15) * 8191
      uint32_t duty = (received_value * 8191) / 15;

      char *res = uint32_to_string_malloc(received_value);

      if (res) {
        // ssd1306_clean();
        // ssd1306_print_str(0, 0, "LED change", false);
        // ssd1306_print_str(0, 27, "Mapped Value:", false);
        // ssd1306_print_str(0, 37, res, false);
        // ssd1306_display();
        // vTaskDelay(300 / portTICK_PERIOD_MS);

        if (xQueueSend(oled_queue, res, pdMS_TO_TICKS(10)) == pdPASS) {
          printf("Main Task: Sent: %s\n", res);
        }

        //
        // printf("---- Value: %s\n", res);
        free(res); // 务必手动释放
      }

      //
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
  }
}

// --- 消费者任务：负责处理显示 ---
void oled_task(void *pvParameters) {
  char buffer[20];
  while (1) {
    // 从队列中读取数据，如果没有数据则阻塞，直到有数据到来
    if (xQueueReceive(oled_queue, &buffer, portMAX_DELAY)) {
      printf("OLED Task: Received: %s\n", buffer);

      // 这里可以添加清屏逻辑，或者简单的覆盖显示
      // 简单实现：清屏后显示新内容
      ssd1306_clean();
      ssd1306_print_str(0, 0, "LED change", false);
      ssd1306_print_str(0, 27, "Mapped Value:", false);
      ssd1306_print_str(0, 37, buffer, false);
      ssd1306_display();
    }
  }
}

void app_main() {
  // 关键修复：加入延时以等待系统完全稳定，抑制上电乱码
  vTaskDelay(pdMS_TO_TICKS(500));

  // SSD1306
  init_ssd1306();

  ssd1306_print_str(0, 0, "LED controler", false);
  ssd1306_print_str(0, 27, "SSD1306 OLED", false);
  ssd1306_print_str(0, 37, "with ESP32", false);
  ssd1306_print_str(0, 46, "ESP-IDF", false);
  // ssd1306_print_str(0, 46, "Embedded C", false);

  ssd1306_display();
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  ssd1306_clean();

  // 初始化硬件
  ledc_init(LED_PIN);

  print_chip_info();

  // 创建队列
  adc_queue = xQueueCreate(QUEUE_LENGTH, sizeof(int));
  // 2. 创建队列 (长度为 5，每个元素是一个最多 20 字节的字符串)
  oled_queue = xQueueCreate(5, sizeof(char[20]));

  // 3. 创建 OLED 显示任务 (优先级为 2)
  xTaskCreate(oled_task, "oled_task", 2048, NULL, 2, NULL);

  // 创建任务
  xTaskCreate(adc_reader_task, "adc_reader", 2048, NULL, 5, NULL);
  xTaskCreate(uart_led_control_task, "uart_led_control", 2048, NULL, 5, NULL);
}
