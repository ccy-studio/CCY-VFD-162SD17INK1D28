#include "common.h"
#include <stdarg.h>


// 封装的日志输出函数
void log_with_prefix(const char *format, ...) {
    // 初始化变参列表
    va_list args;
    va_start(args, format);

    // 打印格式化的日志信息
    vprintf(format, args);

    // 清理变参列表
    va_end(args);
}