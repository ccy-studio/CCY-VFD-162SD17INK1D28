#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include "command.h"

#define DEFAULT_SEPARATOR ";"

typedef enum {
    CT_VFD_VAL_BRIGHTNESS = 0x00,  // VFD亮度调整
    CT_VFD_AC_EN = 0x01,           // VFD供电使能
    CT_VFD_AC_CLEAR = 0x02,        // VFD内容清空
    CT_VFD_VAL_INPUT_0 = 0x03,     // VFD输入0行
    CT_VFD_VAL_INPUT_1 = 0x04,     // VFD输入1行

    CT_DATGETIME_VAL_SET = 0x10,  // 日期时间设置
    CT_DISPLAY_VAL_TYPE = 0x11,   // 显示内容

} control_type;

typedef enum {
    CDIS_DATETIME = 0,  // 显示日期时间,温湿度
    CDIS_CUSTOM = 1,    // 显示自定义内容

} control_display;

typedef void (*ControllerReceiveAction)(control_type, cmd_action_t*);
typedef void (*ControllerReceiveValue)(control_type, cmd_value_t*);

class Controller {
   public:
    bool checkHeart() { return util.checkHeartIsSurvive(); }
    void init(ControllerReceiveValue valueCb,
              ControllerReceiveAction actionCb) {
        receiveValue = valueCb;
        receiveAction = actionCb;
        // 默认显示时间
        current_display_type = CDIS_DATETIME;
    }
    void decoder(const char* buf) {
        util.decode(buf);
        if (util.getCmdValue() != NULL) {
            cmd_value_t* val = util.getCmdValue();
            int number =
                (int)strtol(val->key, NULL, 16);  // 16进制字符串转换为整数
            if (number == CT_VFD_VAL_BRIGHTNESS ||
                number == CT_VFD_VAL_INPUT_0 || number == CT_VFD_VAL_INPUT_1 ||
                number == CT_DATGETIME_VAL_SET ||
                number == CT_DISPLAY_VAL_TYPE) {
                receiveValue((control_type)number, val);
            }
        } else if (util.getCmdAction() != NULL) {
            cmd_action_t* ac = util.getCmdAction();
            int number =
                (int)strtol(ac->key, NULL, 16);  // 16进制字符串转换为整数
            if (number == CT_VFD_AC_EN || number == CT_VFD_AC_CLEAR) {
                receiveAction((control_type)number, ac);
            }
        }
    }

    /**
     * 返回当前的显示内容类型
     */
    control_display getCurrentDispType() { return current_display_type; }
    void setCurrentDispType(control_display t) { current_display_type = t; }

   private:
    control_display current_display_type;
    CommandUtil util;
    ControllerReceiveValue receiveValue;
    ControllerReceiveAction receiveAction;
};

#endif