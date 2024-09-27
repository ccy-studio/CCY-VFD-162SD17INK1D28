#ifndef __INDEV_BTN_H
#define __INDEV_BTN_H

#include "common.h"

extern QueueHandle_t index_btn_queue;

typedef enum {
    INDEV_BTN_RELEASE = 0,
    INDEV_BTN_PRESS,
    INDEV_BTN_LONG_PRESS
} index_btn_state;

typedef struct {
    u8 pin;
    index_btn_state state;
    u32 last_time;
} indev_btn_t;


#define INDEV_BTN_LONG_TIME 2000

void indev_create();


#endif