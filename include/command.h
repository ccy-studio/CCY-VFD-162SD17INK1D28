#ifndef __COMMAND_H
#define __COMMAND_H

#include "common.h"

#define HEART_TIMEOUT 5000  // 心跳最大超时5秒

typedef enum { CMD_TYPE_VALUE = 0, CMD_TYPE_ACTION } cmd_type;

typedef struct {
    char key[30];
    char value[100];
} cmd_value_t;

typedef struct {
    char key[30];
    bool action;
} cmd_action_t;

typedef struct {
    u32 last_time;
    bool state;
} cmd_heart_t;

class CommandUtil {
   public:
    void decode(const char* buf);
    cmd_type getCmdType();
    cmd_value_t* getCmdValue();
    cmd_action_t* getCmdAction();
    cmd_heart_t heart;  // 心跳数据
    bool checkHeartIsSurvive();

   private:
    cmd_type cmd_t;
    void* params;
};

cmd_type CommandUtil::getCmdType() {
    return cmd_t;
}

/**
 * 命令格式
 *
 * Value： CMD:VAL:key:val   //数值数据
 * ACTION: CMD:ACTION:key,bool(1,0)   //控制数据
 * HEART: CMD:HEART  //心跳数据
 */
void CommandUtil::decode(const char* buf) {
    if (params != NULL) {
        free(params);
        params = NULL;
    }
    char* strCp = strdup(buf);
    char *cmd, *type, *p1, *p2;
    cmd = strtok(strCp, ":");
    if (cmd == NULL || strcmp(cmd, "CMD")) {
        goto release;
    }
    type = strtok(NULL, ":");
    if (type == NULL) {
        goto release;
    }

    if (!strcmp(type, "HEART")) {
        // 处理心跳数据
        heart.state = true;
        heart.last_time = micros();
        goto release;
    }

    p1 = strtok(NULL, ":");
    if (p1 == NULL) {
        goto release;
    }

    p2 = strtok(NULL, ":");
    if (p2 == NULL) {
        goto release;
    }

    if (!strcmp(type, "VAL")) {
        cmd_t = CMD_TYPE_VALUE;
        cmd_value_t* val = (cmd_value_t*)malloc(sizeof(cmd_value_t));
        strcpy(val->key, p1);
        strcpy(val->value, p2);
        for (p2 = strtok(NULL, ":"); p2 != NULL; p2 = strtok(NULL, ":")) {
            strcat(val->value, ":");
            strcat(val->value, p2);
        }
        params = val;
    } else if (!strcmp(type, "ACTION")) {
        cmd_t = CMD_TYPE_ACTION;
        cmd_action_t* ac = (cmd_action_t*)malloc(sizeof(cmd_action_t));
        strcpy(ac->key, p1);
        ac->action = atoi(p2);
        params = ac;
    } else {
        goto release;
    }

release:
    free(strCp);
}

cmd_value_t* CommandUtil::getCmdValue() {
    if (params != NULL && cmd_t == CMD_TYPE_VALUE) {
        return (cmd_value_t*)params;
    }
    return NULL;
}
cmd_action_t* CommandUtil::getCmdAction() {
    if (params != NULL && cmd_t == CMD_TYPE_ACTION) {
        return (cmd_action_t*)params;
    }
    return NULL;
}

/**
 * 判断心跳周期是否存活
 */
bool CommandUtil::checkHeartIsSurvive() {
    u32 diff = micros() - heart.last_time;
    heart.state = (diff / 1000) < HEART_TIMEOUT;
    return heart.state;
}

#endif