#ifndef PLAYER_H
#define PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>


typedef struct
{
    int sockID;
    int score;
    long long responseTime; //msec
    int lastAnswer;
    bool hasAnswered;
} Player;

#endif // PLAYER_H
