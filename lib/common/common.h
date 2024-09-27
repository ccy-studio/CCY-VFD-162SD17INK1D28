
#ifndef __COMMON_H
#define __COMMON_H

#include <Arduino.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define IO_I2C_SDA 19
#define IO_I2C_SCL 18

#define IO_8025_INT 2
#define IO_VFD_EN 3
#define IO_VFD_RST 4
#define IO_VFD_CS 5
#define IO_VFD_CP 6
#define IO_VFD_DA 7

#define IO_ADC 0
#define IO_BUZ 1

#define IO_KEY_1 8
#define IO_KEY_2 9
#define BTN_LEN 2

void log_with_prefix(const char* format, ...);

#define LOG_E(format, ...) log_with_prefix("[LOGL:E] " format, ##__VA_ARGS__)
#define LOG_I(format, ...) log_with_prefix("[LOGL:I] " format, ##__VA_ARGS__)
#define LOG_W(format, ...) log_with_prefix("[LOGL:W] " format, ##__VA_ARGS__)

#define ERR_LOOP \
    while (1) {  \
    }

#endif