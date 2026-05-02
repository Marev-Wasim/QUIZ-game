#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


typedef struct
{
    int sockID;
    int score;
    struct timeval responseTime;
    int lastAnswer;
    bool hasAnswered;
} Player;
