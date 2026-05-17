#ifndef STABILITY_H
#define STABILITY_H

#include <sys/time.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

#define MAX_STATE_TIME_SEC      60
#define FLOOD_LIMIT             30
#define INACTIVE_LIMIT_SEC      15
#define HEARTBEAT_INTERVAL_SEC  5
#define MAX_PACKET_SIZE         4048
#define MAGIC_NUMBER            0xBEEF
#define DEBUG_MODE              1

// Global signal flags for Async-Signal-Safe handling
extern volatile sig_atomic_t got_sigint;
extern volatile sig_atomic_t got_sigchld;

typedef enum
{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_SECURITY

} LogLevel;

typedef enum
{
    ERR_SOCKET_CREATE,
    ERR_BIND_FAILED,
    ERR_LISTEN_FAILED,
    ERR_ACCEPT_FAILED,
    ERR_INVALID_PACKET,
    ERR_FLOOD_ATTACK

} ServerError;

typedef enum
{
    STATE_LOBBY,
    STATE_SEND_QUESTION,
    STATE_WAIT_FOR_ANSWERS,
    STATE_CALCULATE_RESULTS,
    STATE_GAME_OVER

} GameState;

typedef enum
{
    LISTEN,
    PLAYING

} NetStatus;

typedef struct
{
    unsigned short magic;
    unsigned short type;
    unsigned int size;
    unsigned int sequence;
    unsigned int version;

} PacketHeader;

typedef struct
{
    long totalQuestionsSent;
    long totalAnswersReceived;
    long totalGamesPlayed;
    long totalDisconnects;
    long totalConnections;
    long totalFloodViolations;
    long totalTimeouts;

} ServerStats;

typedef struct
{
    int sockID;

    int score;

    bool hasAnswered;

    int lastAnswer;

    struct timeval responseTime;

    time_t last_seen_time;

    int msg_count;

    long long last_msg_time_ms;

    bool awaiting_pong;

    time_t last_ping_time;

    unsigned int last_sequence;

    double latency_ms;

    char player_id[64];

} StablePlayer;

typedef struct
{
    GameState current;

    NetStatus status;

    size_t currentQuestionIdx;

    int activePlayers;

    int answersReceived;

    struct timeval Q_sent_time;

    time_t stateStartTime;

} StateMachine;

// Initializes signal handlers and protection systems
void setup_stability(void);

// Cleans up terminated child processes safely
void clean_zombie_processes(void);

// Returns current time in milliseconds
long long get_current_time_ms(void);

// Checks valid state range
bool is_valid_state(GameState state);

// Checks round completion conditions
bool check_game_round_status(StateMachine *fsm, long long question_start_time_ms, int timeout_sec);

// Safely increments player count
void safe_add_player(StateMachine *fsm);

// Safely decrements player count
void safe_remove_player(StateMachine *fsm);

// Writes logs into file
void server_log(const char *msg);

// Writes advanced logs with levels
void server_log_ex(LogLevel level, const char *msg);

// Prints runtime server statistics
void print_server_health(StateMachine *fsm, ServerStats *stats);

// Detects frozen FSM states
bool watchdog_check(StateMachine *fsm);

// Initializes analytics counters
void init_server_stats(ServerStats *stats);

// Converts state enum to string
const char* get_state_name(GameState state);

// Detects flooding attacks
bool check_flood_control(int client_fd, int *msg_count, long long *last_msg_time_ms);

// Detects inactive clients
bool check_client_timeout(StablePlayer *p);

// Changes FSM state safely
void change_state(StateMachine *fsm, GameState new_state);

// Resets player data safely
void reset_player(StablePlayer *p);

// Validates packet size
bool validate_packet_size(int n);

// Safely closes sockets
void safe_close(int *fd);

// Disconnects players safely
void disconnect_player(StablePlayer *p, fd_set *rset, StateMachine *fsm);

// Validates player structure
bool is_valid_player(StablePlayer *p);

// Checks heartbeat health
bool heartbeat_check(StablePlayer *p);

// Handles fatal server errors
void fatal_error(ServerError err, const char *details);

// Broadcasts packets to all players
void broadcast_message(StablePlayer players[], int max_players, void *msg, int (*send_func)(int, void *));
// Cleans all server resources
void cleanup_server(StablePlayer players[], int max_players, int listen_sock);

#endif
