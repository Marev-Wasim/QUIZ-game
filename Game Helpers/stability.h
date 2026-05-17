#ifndef STABILITY_H
#define STABILITY_H

#include <sys/time.h>
#include <stdbool.h>
#include <time.h>

#define MAX_STATE_TIME_SEC 60
#define FLOOD_LIMIT 30

// ==========================
// Game States
// ==========================
typedef enum {
    STATE_LOBBY,
    STATE_SEND_QUESTION,
    STATE_WAIT_FOR_ANSWERS,
    STATE_CALCULATE_RESULTS,
    STATE_GAME_OVER
} GameState;

// ==========================
// Network Status
// ==========================
typedef enum {
    LISTEN,
    PLAYING
} NetStatus;

// ==========================
// State Machine
// ==========================
typedef struct {
    GameState current;
    NetStatus status;

    size_t currentQuestionIdx;

    int activePlayers;
    int answersReceived;

    struct timeval Q_sent_time;

    time_t stateStartTime;

} StateMachine;

// ==========================
// Server Statistics
// ==========================
typedef struct {

    long totalQuestionsSent;
    long totalAnswersReceived;

    long totalGamesPlayed;
    long totalDisconnects;

    long totalConnections;

} ServerStats;

// ==========================
// Stability Functions
// ==========================
void setup_stability(void);

void lock_data(void);
void unlock_data(void);

// ==========================
// Time Functions
// ==========================
long get_current_time_ms(void);

// ==========================
// Validation Functions
// ==========================
bool is_valid_state(GameState state);

// ==========================
// Game Round Check
// ==========================
bool check_game_round_status(
    StateMachine *fsm,
    long question_start_time_ms,
    int timeout_sec
);

// ==========================
// Player Safety
// ==========================
void safe_add_player(StateMachine *fsm);

void safe_remove_player(StateMachine *fsm);

// ==========================
// Logging
// ==========================
void server_log(const char *msg);

// ==========================
// Monitoring
// ==========================
void print_server_health(
    StateMachine *fsm,
    ServerStats *stats
);

// ==========================
// Watchdog
// ==========================
bool watchdog_check(
    StateMachine *fsm
);

// ==========================
// Stats Initialization
// ==========================
void init_server_stats(ServerStats *stats);

#endif
