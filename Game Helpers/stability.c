#include "stability.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h> // تم إضافته لدعم gettimeofday
#include <time.h>     // تم إضافته لدعم time_t و time()
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ==========================
// Zombie Cleanup
// ==========================
void handle_zombies(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// ==========================
// Stability Setup
// ==========================
void setup_stability(void)
{
    struct sigaction sa_chld;
    struct sigaction sa_pipe;

    // Zombie Protection
    sa_chld.sa_handler = handle_zombies;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    // SIGPIPE Protection
    sa_pipe.sa_handler = SIG_IGN;
    sigemptyset(&sa_pipe.sa_mask);
    sa_pipe.sa_flags = 0;
    sigaction(SIGPIPE, &sa_pipe, NULL);

    printf("[Stability] Protection Layer Initialized.\n");
}

// ==========================
// Lock Functions
// ==========================
void lock_data(void)
{
    // السيرفر Single-threaded حالياً والـ select تحمي البيانات
}

void unlock_data(void)
{
    // السيرفر Single-threaded حالياً والـ select تحمي البيانات
}

// ==========================
// Time Utility
// ==========================
long get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000L);
}

// ==========================
// State Validation
// ==========================
bool is_valid_state(GameState state)
{
    return (state >= STATE_LOBBY && state <= STATE_GAME_OVER);
}

// ==========================
// Game Round Checker
// ==========================
bool check_game_round_status(StateMachine *fsm, long question_start_time_ms, int timeout_sec)
{
    if (fsm == NULL)
        return false;

    // No Players Protection
    if (fsm->activePlayers <= 0)
    {
        printf("[Stability] No active players. Resetting to Lobby.\n");
        fsm->current = STATE_LOBBY;
        fsm->currentQuestionIdx = 0;
        fsm->answersReceived = 0;
        fsm->stateStartTime = time(NULL); // تحديث الوقت لحماية الـ Watchdog
        return false;
    }

    // Prevent Overflow
    if (fsm->answersReceived > fsm->activePlayers)
    {
        fsm->answersReceived = fsm->activePlayers;
    }

    long current_time = get_current_time_ms();
    long elapsed = current_time - question_start_time_ms;

    if (elapsed < 0)
    {
        elapsed = 0;
    }

    // Timeout Check
    if (elapsed >= timeout_sec * 1000L)
    {
        return true;
    }

    // All Players Answered
    if (fsm->answersReceived >= fsm->activePlayers)
    {
        return true;
    }

    return false;
}

// ==========================
// Safe Player Add
// ==========================
void safe_add_player(StateMachine *fsm)
{
    if (fsm == NULL)
        return;
    fsm->activePlayers++;
}

// ==========================
// Safe Player Remove
// ==========================
void safe_remove_player(StateMachine *fsm)
{
    if (fsm == NULL)
        return;

    if (fsm->activePlayers > 0)
    {
        fsm->activePlayers--;
    }

    if (fsm->answersReceived > fsm->activePlayers)
    {
        fsm->answersReceived = fsm->activePlayers;
    }
}

// ==========================
// Logging System
// ==========================
void server_log(const char *msg)
{
    FILE *f = fopen("server.log", "a");
    if (f == NULL)
    {
        return;
    }

    time_t now = time(NULL);
    char *t = ctime(&now);

    if (t != NULL)
    {
        t[strcspn(t, "\n")] = '\0'; // إزالة سطر النزول الجديد المدمج في ctime
        fprintf(f, "[%s] %s\n", t, msg);
    }

    fclose(f);
}

// ==========================
// Health Monitor
// ==========================
void print_server_health(StateMachine *fsm, ServerStats *stats)
{
    if (fsm == NULL || stats == NULL)
    {
        return;
    }
printf("\n=================================\n");
    printf("       SERVER HEALTH STATUS      \n");
    printf("=================================\n");
    printf("Current State       : %d\n", fsm->current);
    printf("Active Players      : %d\n", fsm->activePlayers);
    printf("Answers Received    : %d\n", fsm->answersReceived);
    printf("Current Question    : %zu\n", fsm->currentQuestionIdx);
    printf("---------------------------------\n");
    printf("Total Questions     : %ld\n", stats->totalQuestionsSent);
    printf("Total Answers       : %ld\n", stats->totalAnswersReceived);
    printf("Total Games         : %ld\n", stats->totalGamesPlayed);
    printf("Total Disconnects   : %ld\n", stats->totalDisconnects);
    printf("Total Connections   : %ld\n", stats->totalConnections);
    printf("=================================\n");
}

// ==========================
// Watchdog System
// ==========================
bool watchdog_check(StateMachine *fsm)
{
    if (fsm == NULL)
        return false;

    time_t now = time(NULL);
    double elapsed = difftime(now, fsm->stateStartTime);

    if (elapsed > MAX_STATE_TIME_SEC)
    {
        printf("[WATCHDOG WARNING] State Timeout detected! Resetting Engine.\n");
        fsm->current = STATE_LOBBY;
        fsm->answersReceived = 0;
        fsm->currentQuestionIdx = 0;
        fsm->stateStartTime = now;
        return false; // تم اكتشاف تعليق وإعادة التوجيه
    }

    return true; // الحالة مستقرة وضمن الوقت المسموح
}

// ==========================
// Stats Initialization
// ==========================
void init_server_stats(ServerStats *stats)
{
    if (stats == NULL)
        return;

    stats->totalQuestionsSent = 0;
    stats->totalAnswersReceived = 0;
    stats->totalGamesPlayed = 0;
    stats->totalDisconnects = 0;
    stats->totalConnections = 0;
}
