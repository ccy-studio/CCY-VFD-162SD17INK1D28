import math
import sys
import threading
import atexit
import time
from datetime import datetime
from typing import List
import psutil

from PyQt6.QtCore import Qt, QRegularExpression
from PyQt6.QtWidgets import QApplication, QMainWindow, QPushButton, QLabel, QVBoxLayout, QWidget, QComboBox, \
    QHBoxLayout, QSlider, QRadioButton, QButtonGroup, QLineEdit, QMessageBox, QSystemTrayIcon, QMenu
from PyQt6.QtGui import QRegularExpressionValidator, QIcon

import serial.tools.list_ports
from serial.tools.list_ports_common import ListPortInfo
import res_rc

lock = threading.Lock()
serial_connect_state = False  # 串口连接状态
serial_port_lists: List[ListPortInfo] = []  # 搜索到的串口
serial_connect = None  # 当前连接的串口
display_show_code = 1  # 显示内容：1.时间日期、2.网络监控、3.CPU内存、4.自定义

'''
 命令
 * Value： CMD:VAL:key:val   //数值数据
 * ACTION: CMD:ACTION:key,bool(1,0)   //控制数据
 * HEART: CMD:HEART  //心跳数据
'''


class AppWindow(QMainWindow):
    # __slots__ = ('app')

    def __init__(self):
        super().__init__()
        self.setWindowTitle('VFD 性能监控仪表盘(saisaiwa.com)')
        self.setFixedSize(500, 300)
        self.__center_window()

        # 创建系统托盘图标，使用默认图标
        self.tray_icon = QSystemTrayIcon(self)
        main_icon = QIcon(':/icon/resources/monitor.png')
        self.tray_icon.setIcon(main_icon)  # 使用系统主题图标
        self.setWindowIcon(main_icon)
        self.tray_icon.setVisible(True)

        # 创建托盘菜单
        tray_menu = QMenu(self)
        restore_action = tray_menu.addAction("恢复")
        exit_action = tray_menu.addAction("退出")

        # 连接信号
        restore_action.triggered.connect(self.restore_window)
        exit_action.triggered.connect(self.exit_app)

        self.tray_icon.setContextMenu(tray_menu)

        # 连接托盘图标的点击事件
        self.tray_icon.activated.connect(self.tray_icon_activated)

        # 布局开始
        vbox = QVBoxLayout()
        self.vbox = vbox
        # 创建容器并将布局添加到容器
        container = QWidget()
        container.setLayout(vbox)
        # 设置主窗口的中央部件
        self.setCentralWidget(container)

        # 1. 构建选择串口
        vh_box = QHBoxLayout()
        vbox.addLayout(vh_box)
        label = QLabel("选择串口")
        self.combobox = ComboBox()
        self.combobox.addItems(["自动选择"])
        self.combobox.setFixedWidth(100)
        vh_box.addWidget(label)
        vh_box.addWidget(self.combobox)
        self.btn_serial = QPushButton("打开")
        self.btn_serial.clicked.connect(self.__on_serial_click)
        vh_box.addWidget(self.btn_serial)
        vh_box.addStretch()

        # 2. 构建按钮组 - 电源开关，亮度调整
        vh_box = QHBoxLayout()
        vbox.addLayout(vh_box)
        self.btn_power = QPushButton("电源关")
        self.btn_sync_time = QPushButton("手动校时")
        self.btn_power.clicked.connect(self.__on_power_click)
        self.btn_sync_time.clicked.connect(self.__on_update_time_click)
        self.bl_slider = QSlider(Qt.Orientation.Horizontal)
        self.bl_slider.setMinimum(0)
        self.bl_slider.setMaximum(255)
        self.bl_slider.setSingleStep(50)
        self.bl_slider_label = QLabel("亮度: 0%")
        self.bl_slider.valueChanged.connect(self.__on_slider_update_change)
        self.bl_slider.setValue(100)
        self.__on_slider_update_change(self.bl_slider.value())
        vh_box.addWidget(self.btn_power)
        vh_box.addWidget(self.btn_sync_time)
        vh_box.addWidget(self.bl_slider)
        vh_box.addWidget(self.bl_slider_label)
        vh_box.addStretch()

        # 3. 构建显示内容选择
        vh_box = QHBoxLayout()
        vbox.addLayout(vh_box)
        self.display_group = QButtonGroup()
        radio_btn1 = QRadioButton("1.日期时间")
        radio_btn2 = QRadioButton("2.网络监控")
        radio_btn3 = QRadioButton("3.CPU内存")
        radio_btn4 = QRadioButton("4.自定义文字")
        self.display_group.addButton(radio_btn1)
        self.display_group.addButton(radio_btn2)
        self.display_group.addButton(radio_btn3)
        self.display_group.addButton(radio_btn4)
        self.display_group.buttonClicked.connect(self.__on_display_group_change)
        label = QLabel("显示内容设定")
        vh_box.addWidget(label)
        vh_box.addWidget(radio_btn1)
        vh_box.addWidget(radio_btn2)
        vh_box.addWidget(radio_btn3)
        vh_box.addWidget(radio_btn4)
        radio_btn1.setChecked(True)

        # 4. 构建输入自定义输入框
        self.edit_box1 = QWidget()
        vh_box = QHBoxLayout()
        self.edit_box1.setLayout(vh_box)
        vbox.addWidget(self.edit_box1)
        label = QLabel("第一行")
        vh_box.addWidget(label)
        self.edit_line1 = QLineEdit()
        self.edit_line2 = QLineEdit()
        # 设置正则表达式，限制输入为字母和数字，长度在1到16之间
        regex = QRegularExpression("^[a-zA-Z0-9 ]{1,16}$")
        edit_validator1 = QRegularExpressionValidator(regex, self.edit_line1)
        edit_validator2 = QRegularExpressionValidator(regex, self.edit_line2)
        self.edit_line2.setValidator(edit_validator2)
        self.edit_line1.setValidator(edit_validator1)
        vh_box.addWidget(self.edit_line1)

        self.edit_box2 = QWidget()
        vbox.addWidget(self.edit_box2)
        vh_box = QHBoxLayout()
        self.edit_box2.setLayout(vh_box)
        label = QLabel("第二行")
        vh_box.addWidget(label)
        vh_box.addWidget(self.edit_line2)

        # 默认隐藏
        self.edit_box1.hide()
        self.edit_box2.hide()

        # 底部填充
        vbox.addStretch()
        vbox.addWidget(QLabel("技术交流群:676436122"))

    def closeEvent(self, event):
        event.ignore()  # 忽略关闭事件
        self.hide()  # 隐藏窗口

    def tray_icon_activated(self, reason):
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.restore_window()

    def restore_window(self):
        self.show()  # 显示窗口
        self.setWindowState(Qt.WindowState.WindowNoState)  # 取消最小化状态

    def exit_app(self):
        QApplication.quit()  # 退出应用程序

    def __on_slider_update_change(self, val):
        self.bl_slider_label.setText(f"亮度：{val / 255 * 100:.2f}%")
        send_message(f"CMD:VAL:0x00:{val}")

    def __on_display_group_change(self, button: QRadioButton):
        text = button.text()
        if not text:
            return
        if text.startswith("4"):
            self.edit_box1.show()
            self.edit_box2.show()
        else:
            self.edit_box1.hide()
            self.edit_box2.hide()

        global display_show_code
        display_show_code = int(text[:1])
        send_message(f"CMD:VAL:0x11:{0 if display_show_code == 1 else 1}")

    def __on_serial_click(self, val):
        global serial_connect
        index = self.combobox.currentIndex() - 1
        if serial_connect_state:
            disconnect_serial()
            self.btn_serial.setText("打开")
        elif serial_port_lists and 0 <= index < len(serial_port_lists):
            serial_connect = serial_port_lists[index]
            connect_serial(serial_connect)
            if serial_connect_state:
                self.btn_serial.setText("断开")
        elif index == -1:
            if len(serial_port_lists) == 0:
                search_serial_list()
            if len(serial_port_lists) == 0:
                return
            serial_connect = next((port for port in serial_port_lists if "CH340" in port.description), None)
            if serial_connect is None:
                return
            connect_serial(serial_connect)
            if serial_connect_state:
                self.btn_serial.setText("断开")

    def __on_update_time_click(self, val):
        now = datetime.now()
        send_message(f"CMD:VAL:0x10:{now.year % 1000};{now.month};{now.day};{now.hour};{now.minute};{now.second}")

    def __on_power_click(self, val):
        state: bool = False
        if "关" in self.btn_power.text():
            state = False
            self.btn_power.setText("电源开")
        else:
            state = True
            self.btn_power.setText("电源关")
        send_message(f"CMD:ACTION:0x01:{1 if state else 0}")

    def __center_window(self):
        # 获取屏幕的尺寸
        screen_geometry = QApplication.primaryScreen().availableGeometry()
        screen_width = screen_geometry.width()
        screen_height = screen_geometry.height()

        # 计算窗口的位置
        x = (screen_width - self.width()) // 2
        y = (screen_height - self.height()) // 2

        # 移动窗口到计算的位置
        self.move(x, y)


def show_dialog_message(title, msg, icon):
    # 创建错误提示框
    msg_box = QMessageBox()
    msg_box.setIcon(icon)  # 设置图标为错误图标
    msg_box.setText(msg)  # 设置主文本
    msg_box.setWindowTitle(title)  # 设置窗口标题
    msg_box.setStandardButtons(QMessageBox.StandardButton.Ok)  # 设置按钮
    msg_box.exec()  # 显示消息框并等待用户响应


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


class ComboBox(QComboBox):
    def showPopup(self):
        # 清除原先的下拉数据
        self.clear()
        # 添加新数据
        search_serial_list()
        new_items = [port.name for port in serial_port_lists]
        new_items.insert(0, "自动选择")
        self.addItems(new_items)

        # 调用父类的 showPopup 方法以显示下拉列表
        super(ComboBox, self).showPopup()


def task_receive_serial():
    while True:
        receive_message()
        time.sleep(0.1)


def task_heart():
    """
    定时发送心跳数据
    :return:
    """
    while True:
        time.sleep(4)
        send_message(f"CMD:HEART")


def format_speed(bytes_per_second):
    """根据速度值格式化为 kB/s 或 MB/s"""
    if bytes_per_second < 1024:
        return f"{bytes_per_second:.2f}B/s"
    elif bytes_per_second < 1024 * 1024:
        return f"{bytes_per_second / 1024:.2f}Kb/s"
    else:
        return f"{bytes_per_second / (1024 * 1024):.2f}MB/s"


def get_network_speed():
    """获取网络接口的上传和下载速度"""
    bytes_sent = 0
    bytes_recv = 0
    # 获取所有网络接口的 I/O 统计信息
    net_io = psutil.net_io_counters(pernic=True)

    for k, v in net_io.items():
        bytes_sent += v.bytes_sent
        bytes_recv += v.bytes_recv
    return bytes_sent, bytes_recv


def task_monitor():
    row1 = ""
    row2 = ""
    # 获取初始的上传和下载字节数
    initial_sent, initial_recv = get_network_speed()
    time.sleep(1)  # 等待1秒
    while True:
        # 获取当前的上传和下载字节数
        current_sent, current_recv = get_network_speed()
        # 计算上传和下载速度
        upload_speed = current_sent - initial_sent
        download_speed = current_recv - initial_recv
        # 格式化速度
        formatted_upload = format_speed(upload_speed)
        formatted_download = format_speed(download_speed)
        # 输出结果
        # print(f"上传速度: {formatted_upload}, 下载速度: {formatted_download}")
        # 更新初始值
        initial_sent, initial_recv = current_sent, current_recv

        # 刷新数值
        if serial_connect_state and display_show_code != 1:
            if display_show_code == 2:
                # 网络监控
                row1 = f"NetUp:{formatted_upload}"
                row2 = f"NetDn:{formatted_download}"
            elif display_show_code == 3:
                # CPU内存
                cpu_usage = psutil.cpu_percent(interval=1, percpu=False)
                cpu_count = psutil.cpu_count()
                # 获取内存使用情况
                # CPU 100% F:3.2Ghz
                memory_info = psutil.virtual_memory()
                row1 = f"CPU {max(math.ceil(cpu_usage), 1)}% Core:{cpu_count}"
                row2 = f"Mem {int(memory_info.percent)}% {memory_info.total / (1024 ** 3):.2f}GB"
            elif display_show_code == 4:
                # 自定义
                row1 = window.edit_line1.text()
                row2 = window.edit_line2.text()

            send_message(f"CMD:VAL:0x03:{row1}")
            send_message(f"CMD:VAL:0x04:{row2}")
        time.sleep(1)


atexit.register(disconnect_serial)
if __name__ == '__main__':
    thread_serial_receive = threading.Thread(target=task_receive_serial, daemon=True, name='vfd task_receive_serial')
    thread_serial_heart = threading.Thread(target=task_heart, daemon=True, name='vfd task_heart')
    thread_monitor = threading.Thread(target=task_monitor, daemon=True, name='vfd monitor')
    thread_serial_receive.start()
    thread_serial_heart.start()
    thread_monitor.start()

    app = QApplication(sys.argv)
    window = AppWindow()
    window.show()
    sys.exit(app.exec())
