# VFD电脑性能监控器+USB扩展与TF读卡器

![IMG_0080.jpg](//image.lceda.cn/oshwhub/db6f66b521ab41e497c4e382af98fc63.jpg)

## 1. 项目功能介绍
手里弄到一批Futaba的16x2点阵屏,于是乎物尽其用做个小桌搭，但是现在社区里做VFD时钟的太多。我自己也做了很多钟，在做钟就有点搞笑了....
那么就加点不一样的功能，我希望他可以具有链接电脑的能力使其进行赋能一些特性
1. USB HUB扩展集线器
2. SD/TF卡读卡器
3. 温湿度传感器
4. 可显示日期时间
5. 配套上位机，随意传输显示内容
    6. 实时网速上下行
    7. CPU/内存使用率
    8. 自定义文字



## 3. 硬件设计

![容器 1.png](//image.lceda.cn/oshwhub/306a47e97fe443469d2da3badf6f335e.png)


### 3.1 VFD屏幕驱动
由于屏幕是集成驱动的，所以在设计电路的时候只需要关心：满足灯丝需求、满足SPI通讯的电平要求即可。
屏幕高压输入41V使用LGS6302B5这款升压芯片实现
灯丝实测使用直流供电效果几乎看不出，采用LDO ME6211C33M5G-N 3.3V供电降压

![image.png](//image.lceda.cn/oshwhub/0df1a351f8b84764b0ef72abb3b15a44.png)


### 3.2 集成外设
使用了集成ESP32-C3 模组为主控，构建自动下载电路方便调试。
整体中，从Type-C接口进行供电，当连接电脑的时候D+和D-数据线首先接入SL2.1A一款USB扩展IC，从其中扩展出4路HUB接口，其中一路固定连接到CH340C，剩下的3路其中一路给到TF读卡器IC使用，剩下的两路分别是扩展USB-A和Type-C接口。

![image.png](//image.lceda.cn/oshwhub/e3ee9bc3aafe45f39b0cfe50c80a72f7.png)

**实时时钟**
实时时钟采用高精度IC：RX8025T-UB，这款IC采用的i2c通讯，后备电池使用可充电电池设计，即上电后则开始充电，掉电后开始放电保持时钟不断走时。 使用两颗二极管构建简易供电选择器

![image.png](//image.lceda.cn/oshwhub/c7588c9363774feeb01f6b22ed07effd.png)

## 4.软件开发
> 软件部分分为两个部分
> 1. ESP32 PlatformIO
> 2. PyQt6 上位机

### 目录结构

![image.png](//image.lceda.cn/oshwhub/0770aeaeeb2d4e73983424b7b3f66ce2.png)


### 4.1 ESP32 PlatfromIO

![主流程.png](//image.lceda.cn/oshwhub/64cbb66cad194b53bbd6d68a2432bb9e.png)

![消息处理流程.png](//image.lceda.cn/oshwhub/d3dce4df47154f0295baef8a74e7bfd6.png)

**部分核心代码：**
````c++
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
````


### 4.2 Python PyQt6 上位机


![image.png](//image.lceda.cn/oshwhub/273f967b899e454f9f71f1393f400fed.png)


![image.png](//image.lceda.cn/oshwhub/b53d662a72e5463fb66fb53a180a33a8.png)

**部分核心代码**
````python
def task_heart():
    """
    定时发送心跳数据
    :return:
    """
    while True:
        time.sleep(4)
        send_message(f"CMD:HEART")


def search_serial_list():
    global serial_port_lists
    serial_port_lists = serial.tools.list_ports.comports()
    for port in serial_port_lists:
        print(f"端口: {port.device}, 描述: {port.description}")


def connect_serial(port_item: ListPortInfo):
    global serial_connect_state, serial_connect
    if serial_connect_state:
        return
    try:
        serial_connect = serial.Serial(port=port_item.name, baudrate=115200, bytesize=8, timeout=2,
                                       stopbits=serial.STOPBITS_ONE)
    except serial.SerialException as e:
        serial_connect = None
        print("串口被占用，打开失败")
        show_dialog_message("错误", "串口被占用,打开失败", QMessageBox.Icon.Critical)
        return
    if serial_connect.is_open:
        print(f"成功连接到 {serial_connect.name}")
        serial_connect_state = True
    else:
        serial_connect_state = False
        print("连接失败")


def disconnect_serial():
    global serial_connect, serial_connect_state
    if serial_connect_state and serial_connect is not None:
        serial_connect_state = False
        serial_connect.close()
        serial_connect = None
        print("串口关闭成功")


def send_message(msg):
    """
    串口发送字符串数据
    :param msg: 字符串数据
    :return:
    """
    if serial_connect_state:
        with lock:
            serial_connect.write(f"{msg}\n".encode())


def vfd_clear():
    """
    清空VFD的内容显示
    :return:
    """
    send_message(f"CMD:ACTION:0x02:1")


def receive_message():
    if serial_connect_state and serial_connect.in_waiting > 0:
        received_data = serial_connect.readline()
        if received_data:
            print(f"接收: {received_data.decode('utf-8')}")

````


## 6. 组装

![IMG_0077.jpg](//image.lceda.cn/oshwhub/c52790224f6f445aaaf0638e0f6efc36.jpg)


![IMG_0078.jpg](//image.lceda.cn/oshwhub/abf9f45891cb4e65b52f8af5ec50f891.jpg)


![IMG_0079.jpg](//image.lceda.cn/oshwhub/924454e7d3ac4485a3662ff060e9cf90.jpg)


![IMG_0082.jpg](//image.lceda.cn/oshwhub/0c10b9e7cf3b4c449f4a99694528f309.jpg)

## 7. 视频演示
https://www.bilibili.com/video/BV152xceQEYi/

<iframe src="//player.bilibili.com/player.html?bvid=BV152xceQEYi&page=1" scrolling="no" border="0" frameborder="no" framespacing="0" allowfullscreen="true"> </iframe>