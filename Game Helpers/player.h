#ifndef PLAYER_H
#define PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>

#include "../NET_CORE/config.h"
#include "../NET_CORE/unp.h"

typedef struct
{
    int sockID;
    int score;
    char name[32];
    struct timeval responseTime; 
    int lastAnswer;
    bool hasAnswered;

    int msg_count;
    long long last_msg_time_ms;
} Player;

#endif // PLAYER_H
