
#ifndef __RX8025_H
#define __RX8025_H

#include "common.h"
#include "Wire.h"

#ifdef __cplusplus
extern "C" {
#endif


#define RX8025_ADDR 0x32

typedef struct {
    u8 year;
    u8 month;
    u8 day;
    u8 week;
    u8 hour;
    u8 min;
    u8 sec;
} rx8025_timeinfo;


void rx8025_init(TwoWire* theWire);

/**
 * set timeinfo
 */
void rx8025_set_time(u8 year,
                     u8 month,
                     u8 day,
                     u8 week,
                     u8 hour,
                     u8 min,
                     u8 sec);

/**
 * get latest time
 */
void rx8025_time_get(rx8025_timeinfo* timeinfo);

/**
 * formart HHmmss
 */
void formart_time(rx8025_timeinfo* timeinfo, char* buf);
/**
 * formart YYMMdd
 */
void formart_date(rx8025_timeinfo* timeinfo, char* buf);

#ifdef __cplusplus
}
#endif

#endif