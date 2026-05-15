#ifndef SPEED_BONUS_H
#define SPEED_BONUS_H

#include <sys/time.h>
#include "player.h"

#define BASE_POINTS 100
#define MAX_BONUS 50
#define QUESTION_TIMEOUT 30  //seconds allowed per question

typedef struct
{
    struct timeval strat_time;
    struct timeval end_time;
} QuizTimer;

int calc_score (double response_time_sec);
int cmp_players (const void* a, const void* b);
void sort_leaderboard (Player* players, int num_players);

#endif
