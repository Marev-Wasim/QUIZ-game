#include "stability.h"

#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>

// Global flags for safe signal handling (Async-Signal-Safe)
volatile sig_atomic_t got_sigint = 0;
volatile sig_atomic_t got_sigchld = 0;

void handle_sigint(int sig)
{
    (void)sig;
    got_sigint = 1;
}

void handle_zombies(int sig)
{
    (void)sig;
    got_sigchld = 1;
}

// Reaps zombie processes safely outside signal handler
void clean_zombie_processes(void)
{
    if (got_sigchld)
    {
        got_sigchld = 0;
        while(waitpid(-1, NULL, WNOHANG) > 0);
    }
}

// Initializes all stability protections
void setup_stability(void)
{
    struct sigaction sa_chld;
    struct sigaction sa_pipe;
    struct sigaction sa_int;

    sa_chld.sa_handler = handle_zombies;

    sigemptyset(&sa_chld.sa_mask);

    sa_chld.sa_flags =
        SA_RESTART | SA_NOCLDSTOP;

    sigaction(
        SIGCHLD,
        &sa_chld,
        NULL
    );

    sa_pipe.sa_handler = SIG_IGN;

    sigemptyset(&sa_pipe.sa_mask);

    sa_pipe.sa_flags = 0;

    sigaction(
        SIGPIPE,
        &sa_pipe,
        NULL
    );

    sa_int.sa_handler = handle_sigint;

    sigemptyset(&sa_int.sa_mask);

    sa_int.sa_flags = 0;

    sigaction(
        SIGINT,
        &sa_int,
        NULL
    );

    printf(
        "[STABILITY] Full protection layer initialized.\n"
    );
}

// Returns current time in milliseconds with overflow protection
long long get_current_time_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return
        ((long long)tv.tv_sec * 1000LL) +
        ((long long)tv.tv_usec / 1000LL);
}

// Validates FSM states
bool is_valid_state(GameState state)
{
    return
        (state >= STATE_LOBBY &&
         state <= STATE_GAME_OVER);
}

// Validates incoming packet size
bool validate_packet_size(int n)
{
    return
        (n > 0 &&
         n <= MAX_PACKET_SIZE);
}

// Validates player structure
bool is_valid_player(StablePlayer *p)
{
    return
        (p != NULL &&
         p->sockID >= 0);
}

// Checks current round status
bool check_game_round_status(
    StateMachine *fsm,
    long long question_start_time_ms,
    int timeout_sec
)
{
    if(fsm == NULL)
        return false;

    if(fsm->activePlayers <= 0)
    {
        printf(
            "[WARNING] No active players.\n"
        );

        change_state(
            fsm,
            STATE_LOBBY
        );

        return false;
    }

    long long current_time =
        get_current_time_ms();

    long long elapsed =
        current_time -
        question_start_time_ms;

    if(elapsed >= (long long)timeout_sec * 1000LL)
    {
        return true;
    }

    if(fsm->answersReceived >=
       fsm->activePlayers)
    {
        return true;
    }

    return false;
}

// Safely increments player count
void safe_add_player(StateMachine *fsm)
{
    if(fsm == NULL)
        return;

    fsm->activePlayers++;
}

// Safely decrements player count
void safe_remove_player(StateMachine *fsm)
{
    if(fsm == NULL)
        return;

    if(fsm->activePlayers > 0)
    {
        fsm->activePlayers--;
    }

    if(fsm->answersReceived >
       fsm->activePlayers)
    {
        fsm->answersReceived =
            fsm->activePlayers;
    }
}

// Resets all player values
void reset_player(StablePlayer *p)
{
    if(p == NULL)
        return;

    p->score = 0;

    p->hasAnswered = false;

    p->lastAnswer = -1;

    p->responseTime.tv_sec = 0;

    p->responseTime.tv_usec = 0;

    p->msg_count = 0;

    p->last_msg_time_ms =
        get_current_time_ms();

    p->last_seen_time =
        time(NULL);

    p->awaiting_pong = false;

    p->last_ping_time = time(NULL);

    p->last_sequence = 0;

    p->latency_ms = 0.0;
}

// Writes logs into server.log
void server_log(const char *msg)
{
    FILE *f =
        fopen("server.log", "a");

    if(f == NULL)
        return;

    time_t now = time(NULL);

    char *t = ctime(&now);

    if(t != NULL)
    {
        t[strcspn(t, "\n")] = '\0';

        fprintf(
            f,
            "[%s] %s\n",
            t,
            msg
        );
    }fclose(f);
}

// Writes advanced logs with levels
void server_log_ex(
    LogLevel level,
    const char *msg
)
{
    char final_msg[1024];

    const char *level_str;

    switch(level)
    {
        case LOG_INFO:
            level_str = "INFO";
            break;

        case LOG_WARNING:
            level_str = "WARNING";
            break;

        case LOG_ERROR:
            level_str = "ERROR";
            break;

        case LOG_SECURITY:
            level_str = "SECURITY";
            break;

        default:
            level_str = "UNKNOWN";
    }

    snprintf(
        final_msg,
        sizeof(final_msg),
        "[%s] %s",
        level_str,
        msg
    );

    server_log(final_msg);
}

// Converts state enums into readable strings
const char* get_state_name(GameState state)
{
    switch(state)
    {
        case STATE_LOBBY:
            return "LOBBY";

        case STATE_SEND_QUESTION:
            return "SEND_QUESTION";

        case STATE_WAIT_FOR_ANSWERS:
            return "WAIT_FOR_ANSWERS";

        case STATE_CALCULATE_RESULTS:
            return "CALCULATE_RESULTS";

        case STATE_GAME_OVER:
            return "GAME_OVER";

        default:
            return "UNKNOWN_STATE";
    }
}

// Safely changes FSM states
void change_state(
    StateMachine *fsm,
    GameState new_state
)
{
    if(fsm == NULL)
        return;

    char msg[256];

    snprintf(
        msg,
        sizeof(msg),
        "STATE TRANSITION: %s -> %s",
        get_state_name(fsm->current),
        get_state_name(new_state)
    );

    printf("[ENGINE] %s\n", msg);

    server_log_ex(
        LOG_INFO,
        msg
    );

    fsm->current = new_state;

    fsm->stateStartTime =
        time(NULL);
}

// Displays runtime diagnostics
void print_server_health(
    StateMachine *fsm,
    ServerStats *stats
)
{
    if(fsm == NULL || stats == NULL)
        return;

    printf("\n=================================\n");

    printf("SERVER HEALTH STATUS\n");

    printf("=================================\n");

    printf("Current State       : %s\n",
           get_state_name(fsm->current));

    printf("Active Players      : %d\n",
           fsm->activePlayers);

    printf("Answers Received    : %d\n",
           fsm->answersReceived);

    printf("Current Question    : %zu\n",
           fsm->currentQuestionIdx);

    printf("---------------------------------\n");

    printf("Total Questions     : %ld\n",
           stats->totalQuestionsSent);

    printf("Total Answers       : %ld\n",
           stats->totalAnswersReceived);

    printf("Total Games         : %ld\n",
           stats->totalGamesPlayed);

    printf("Total Disconnects   : %ld\n",
           stats->totalDisconnects);

    printf("Total Connections   : %ld\n",
           stats->totalConnections);

    printf("=================================\n");
}

// Detects frozen states
bool watchdog_check(StateMachine *fsm)
{
    if(fsm == NULL)
        return false;

    if(fsm->current == STATE_LOBBY)
        return true;

    time_t now = time(NULL);

    double elapsed =
        difftime(
            now,
            fsm->stateStartTime
        );

    if(elapsed > MAX_STATE_TIME_SEC)
    {
        printf(
            "[WATCHDOG] State freeze detected.\n"
        );

        change_state(
            fsm,
            STATE_LOBBY
        );

        fsm->status = LISTEN;

        fsm->answersReceived = 0;

        fsm->currentQuestionIdx = 0;

        return false;
    }

    return true;
}

// Initializes analytics counters
void init_server_stats(ServerStats *stats)
{
    if(stats == NULL)
        return;

    memset(
        stats,
        0,
        sizeof(ServerStats)
    );
}

// Prevents flooding attacks
bool check_flood_control(
    int client_fd,
    int *msg_count,
    long long *last_msg_time_ms
)
{
    long long now =
        get_current_time_ms();

    if(now - *last_msg_time_ms < 1000LL)
    {
        (*msg_count)++;
    }
    else
    {
        *msg_count = 1;

        *last_msg_time_ms = now;
    }if(*msg_count > FLOOD_LIMIT)
    {
        printf(
            "[SECURITY] Flood detected from FD %d\n",
            client_fd
        );

        return false;
    }

    return true;
}

// Detects inactive clients
bool check_client_timeout(
    StablePlayer *p
)
{
    if(p == NULL)
        return false;

    time_t now = time(NULL);

    double idle =
        difftime(
            now,
            p->last_seen_time
        );

    return
        (idle > INACTIVE_LIMIT_SEC);
}

// Checks heartbeat health
bool heartbeat_check(
    StablePlayer *p
)
{
    if(p == NULL)
        return false;

    time_t now = time(NULL);

    double elapsed =
        difftime(
            now,
            p->last_ping_time
        );

    if(p->awaiting_pong &&
       elapsed > HEARTBEAT_INTERVAL_SEC)
    {
        return false;
    }

    return true;
}

// Safely closes sockets
void safe_close(int *fd)
{
    if(fd == NULL)
        return;

    if(*fd != -1)
    {
        close(*fd);

        *fd = -1;
    }
}

// Safely disconnects players
void disconnect_player(
    StablePlayer *p,
    fd_set *rset,
    StateMachine *fsm
)
{
    if(p == NULL)
        return;

    if(rset != NULL && p->sockID != -1)
    {
        FD_CLR(
            p->sockID,
            rset
        );
    }

    safe_close(
        &p->sockID
    );

    safe_remove_player(fsm);

    reset_player(p);
}

// Handles fatal server errors
void fatal_error(
    ServerError err,
    const char *details
)
{
    char msg[512];

    snprintf(
        msg,
        sizeof(msg),
        "FATAL ERROR [%d]: %s",
        err,
        details
    );

    server_log_ex(
        LOG_ERROR,
        msg
    );

    fprintf(
        stderr,
        "%s\n",
        msg
    );

    exit(EXIT_FAILURE);
}

// Broadcasts packets to all clients
void broadcast_message(
    StablePlayer players[],
    int max_players,
    void *msg,
    int (*send_func)(int, void *)
)
{
    if(players == NULL ||
       send_func == NULL)
    {
        return;
    }

    for(int i = 0; i < max_players; i++)
    {
        if(players[i].sockID != -1)
        {
            send_func(
                players[i].sockID,
                msg
            );
        }
    }
}

// Releases all server resources
void cleanup_server(
    StablePlayer players[],
    int max_players,
    int listen_sock
)
{
    for(int i = 0; i < max_players; i++)
    {
        if (players[i].sockID != -1)
        {
            safe_close(
                &players[i].sockID
            );
        }
    }

    safe_close(
        &listen_sock
    );

    server_log_ex(
        LOG_INFO,
        "Server resources cleaned successfully."
    );
}
