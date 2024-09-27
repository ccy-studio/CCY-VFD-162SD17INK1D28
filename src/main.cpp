#include <Arduino.h>
#include "Adafruit_SHT4x.h"
#include "M162SD1XAA.h"
#include "Wire.h"
#include "common.h"
#include "controller.h"
#include "indev_button.h"
#include "rx8025t.h"

indev_btn_t indev_bts[BTN_LEN] = {
    {.pin = IO_KEY_1, .state = INDEV_BTN_RELEASE},
    {.pin = IO_KEY_2, .state = INDEV_BTN_RELEASE}};

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
M162SD13AA vfd;
Controller uart_control;

static sensors_event_t humidity, temp;
static char buf1[16], buf2[16];
static rx8025_timeinfo timeinfo;

void init_hal();
void sensor_get();
void on_btn_click(void* params);
void on_controll_receive_ac(control_type type, cmd_action_t* ac_msg);
void on_controll_receive_val(control_type type, cmd_value_t* val_msg);
void on_thread_uart_rx(void* params);

void setup() {
    Serial.begin(115200);
    Wire.begin(IO_I2C_SDA, IO_I2C_SCL);

    pinMode(IO_8025_INT, INPUT_PULLUP);
    pinMode(IO_VFD_EN, OUTPUT);
    pinMode(IO_VFD_CS, OUTPUT);
    pinMode(IO_VFD_RST, OUTPUT);

    pinMode(IO_KEY_1, INPUT_PULLUP);
    pinMode(IO_KEY_2, INPUT_PULLUP);

    pinMode(IO_BUZ, OUTPUT);
    pinMode(IO_ADC, INPUT);

    init_hal();
}

void loop() {
    control_display disp = uart_control.getCurrentDispType();
    if (disp == CDIS_DATETIME) {
        sensor_get();
        // 默认的时间
        rx8025_time_get(&timeinfo);
        memset(buf1, ' ', sizeof(buf1));
        memset(buf2, ' ', sizeof(buf2));
        static const char* formart_style1 = " %02d/%02d %02d:%02d:%02d ";
        static const char* formart_style2 = " %02d/%02d %02d %02d %02d ";
        static bool invert = false;

        // 09-19 12:12:12
        sprintf(buf1, invert ? formart_style1 : formart_style2, timeinfo.month,
                timeinfo.day, timeinfo.hour, timeinfo.min, timeinfo.sec);
        // T:12.5° H:122
        sprintf(buf2, "T:%.2fC H:%.1f%%%", temp.temperature,
                humidity.relative_humidity);

        invert = !invert;

        vfd.writeString(0, buf1);
        vfd.writeString(1, buf2);
    } else {
        if (!uart_control.checkHeart()) {
            uart_control.setCurrentDispType(CDIS_DATETIME);
            LOG_W("Heart timeout set display to default\n");
            return;
        }
    }

    delay(500);
}

void init_hal() {
    sht4.begin(&Wire);
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    rx8025_init(&Wire);

    vfd.powerOn();
    delay(500);
    vfd.init(IO_VFD_CS);
    vfd.setDimming(0xff);
    // vfd.setGrayLevel(0, 0xff);
    // vfd.setGrayLevel(1, 0xff);

    vfd.clear();
    vfd.writeString(0, "Hello! Start...");
    vfd.writeString(1, "Saisaiwa");
    delay(2000);
    vfd.clear();

    indev_create();
    xTaskCreate(on_btn_click, "BTNON", 2048, NULL, 2, NULL);
    uart_control.init(on_controll_receive_val, on_controll_receive_ac);

    xTaskCreate(on_thread_uart_rx, "RX", 6000, NULL, 1, NULL);
}

/**
 * 串口接收数据
 */
void on_thread_uart_rx(void* params) {
    while (1) {
        if (Serial.available() > 0) {
            String data = Serial.readStringUntil('\n');  // 读取直到换行符的数据
            LOG_I("%s\n", data.c_str());
            uart_control.decoder(data.c_str());
        }
    }
}

void on_controll_receive_ac(control_type type, cmd_action_t* ac_msg) {
    bool en = ac_msg->action;
    if (type == CT_VFD_AC_EN) {
        if (en) {
            vfd.powerOn();
            delay(100);
            vfd.init(IO_VFD_CS);
            vfd.setDimming(0xff);
        } else {
            vfd.clear();
            vfd.switchOff();
            delay(100);
            vfd.powerOff();
        }
    } else if (type == CT_VFD_AC_CLEAR) {
        if (uart_control.getCurrentDispType() != CDIS_DATETIME) {
            memset(buf1, 0, sizeof(buf1));
            memset(buf2, 0, sizeof(buf2));
            vfd.clear();
        }
    }
}
void on_controll_receive_val(control_type type, cmd_value_t* val_msg) {
    if (type == CT_DISPLAY_VAL_TYPE) {
        // 切换画面
        uart_control.setCurrentDispType((control_display)atoi(val_msg->value));
    } else if (type == CT_DATGETIME_VAL_SET) {
        // 设定时间日期
        char* cp = strdup(val_msg->value);
        u8 date_num[6] = {0};
        u8 i = 0;
        char* n;
        for (n = strtok(cp, DEFAULT_SEPARATOR); n != NULL;
             n = strtok(NULL, DEFAULT_SEPARATOR)) {
            if (n == NULL) {
                LOG_E("CT_DATGETIME_VAL_SET, Value illegally!");
                free(cp);
                return;
            }
            if (i >= 6) {
                free(cp);
                return;
            }
            date_num[i++] = atoi(n);
        }
        rx8025_set_time(date_num[0], date_num[1], date_num[2], 0, date_num[3],
                        date_num[4], date_num[5]);
        free(cp);
    } else if (type == CT_VFD_VAL_BRIGHTNESS) {
        if (val_msg->value != NULL) {
            u8 bl = atoi(val_msg->value);
            vfd.setDimming(bl > 10 ? bl : 10);
        }
    } else if (type == CT_VFD_VAL_INPUT_0 || type == CT_VFD_VAL_INPUT_1) {
        if (uart_control.getCurrentDispType() != CDIS_CUSTOM) {
            return;
        }
        if (val_msg->value == NULL) {
            return;
        }
        if (type == CT_VFD_VAL_INPUT_0) {
            memset(buf1, ' ', sizeof(buf1));
            memcpy(buf1, val_msg->value, strlen(val_msg->value));
        } else {
            memset(buf2, ' ', sizeof(buf2));
            memcpy(buf2, val_msg->value, strlen(val_msg->value));
        }
        vfd.writeString(0, buf1);
        vfd.writeString(1, buf2);
    }
}

/**
 * 按键扫描
 */
void on_btn_click(void* params) {
    indev_btn_t btn;
    while (1) {
        if (xQueueReceive(index_btn_queue, &btn, portMAX_DELAY) == pdPASS) {
            // 处理接收到的按键事件
            if (btn.pin == IO_KEY_1) {
                if (btn.state == INDEV_BTN_PRESS) {
                    LOG_I("按键1按下\n");
                }
            } else if (btn.pin == IO_KEY_2) {
                if (btn.state == INDEV_BTN_PRESS) {
                    LOG_I("按键2按下\n");
                } else if (btn.state == INDEV_BTN_LONG_PRESS) {
                    LOG_I("按键2按 长按\n");
                }
            }
        }
    }
}

/**
 * 读取温湿度传感器的数据
 */
void sensor_get() {
    sht4.getEvent(&humidity, &temp);
}
