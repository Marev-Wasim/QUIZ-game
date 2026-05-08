#include "speed_bonus.h"
#include <math.h>

int calc_score (double response_time_sec)
{
    if (response_time_sec >= QUESTION_TIMEOUT)
    {
        return BASE_POINTS;
    }
    double time_remaining = (double) QUESTION_TIMEOUT - response_time_sec;
    double bonus= (time_remaining / QUESTION_TIMEOUT) * MAX_BONUS;

    return BASE_POINTS+(int)bonus;
}

int cmp_players (const void* a, const void* b)
{
    Player* playerA= (Player*) a;
    Player* playerB= (Player*) b;

    return (playerB->score - playerA->score);
}

void sort_leaderboard (Player* players, int num_players)
{
    qsort(players, num_players, sizeof(Player), cmp_players);
}
