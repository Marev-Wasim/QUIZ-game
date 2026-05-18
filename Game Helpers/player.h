#ifndef PLAYER_H
#define PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>

// 🔗 ربط مكتبة الشبكة هنا لكي يراها ملف stability-3.c تلقائياً ويحل مشكلة err_sys و writen
#include "../NET_CORE/config.h"
#include "../NET_CORE/unp.h"

typedef struct
{
    int sockID;
    int score;
    char name[32];
    struct timeval responseTime; // ميكروسيكند كما في الكود الخاص بكِ
    int lastAnswer;
    bool hasAnswered;

    // 🛡️ المتغيرات المطلوبة لملف stability-3.c لكي يختفي الخطأ
    int msg_count;
    long long last_msg_time_ms;
} Player;

#endif // PLAYER_H
