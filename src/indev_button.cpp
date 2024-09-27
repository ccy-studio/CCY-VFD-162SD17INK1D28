#include "indev_button.h"

QueueHandle_t index_btn_queue;

extern indev_btn_t indev_bts[BTN_LEN];

static void btn_scan_task(void* params) {
    size_t index;
    while (1) {
        for (index = 0; index < BTN_LEN; index++) {
            delay(100);
            indev_btn_t* btn = &indev_bts[index];
            u8 level = digitalRead(btn->pin);
            if (level && btn->state != INDEV_BTN_RELEASE) {
                // 释放
                btn->state = INDEV_BTN_RELEASE;
                btn->last_time = micros();
                // 发送事件
                xQueueSend(index_btn_queue, btn, portMAX_DELAY);
            } else if (!level && btn->state == INDEV_BTN_RELEASE) {
                // 按下
                btn->state = INDEV_BTN_PRESS;
                btn->last_time = micros();
                // 发送事件
                xQueueSend(index_btn_queue, btn, portMAX_DELAY);
            } else if (!level && btn->state == INDEV_BTN_PRESS &&
                       (micros() - btn->last_time) >
                           INDEV_BTN_LONG_TIME * 1000) {
                // 长按事件
                btn->state = INDEV_BTN_LONG_PRESS;
                btn->last_time = micros();
                // 发送事件
                xQueueSend(index_btn_queue, btn, portMAX_DELAY);
            }
        }
    }
}

void indev_create() {
    index_btn_queue = xQueueCreate(BTN_LEN * 2, sizeof(indev_btn_t));
    if (index_btn_queue == NULL) {
        LOG_E("Queue Create Fail\n");
        ERR_LOOP;
    }
    xTaskCreate(btn_scan_task, "BTN", 2048, NULL, 3, NULL);
}