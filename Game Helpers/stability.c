#include "stability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

volatile sig_atomic_t got_sigint = 0;

StabilityMetrics g_metrics = {0};

static void handle_sigint(int sig)
{
    (void)sig;
    got_sigint = 1;
}

void setup_stability()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_sigint;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    printf("[STABILITY] Initialized Successfully\n");
}

long long get_current_time_ms()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return
        ((long long)tv.tv_sec * 1000LL) +
        ((long long)tv.tv_usec / 1000LL);
}

void set_nonblocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);

    if (flags == -1)
        return;

    fcntl(
        sock,
        F_SETFL,
        flags | O_NONBLOCK
    );
}

void reset_player(Player *p)
{
    if (!p)
        return;

    p->sockID = -1;
    p->score = 0;
    p->hasAnswered = false;
    p->lastAnswer = -1;
    p->msg_count = 0;
    p->last_msg_time_ms = 0;

    memset(
        &p->responseTime,
        0,
        sizeof(struct timeval)
    );
}

bool check_flood_control(
    int sock,
    int *msg_count,
    long long *last_msg_time_ms
)
{
    long long current_time =
        get_current_time_ms();

    if (*last_msg_time_ms == 0)
    {
        *last_msg_time_ms = current_time;
        *msg_count = 1;
        return true;
    }

    if (
        current_time - *last_msg_time_ms <
        1000LL
    )
    {
        (*msg_count)++;

        if (*msg_count > MAX_MSG_PER_SECOND)
        {
            g_metrics.flood_attempts++;

            printf(
                "[SECURITY] Flood detected on socket %d\n",
                sock
            );

            return false;
        }
    }
    else
    {
        *msg_count = 1;
        *last_msg_time_ms = current_time;
    }

    return true;
}

bool validate_message(Message *msg)
{
    if (!msg)
        return false;

    if (
        msg->length < 0 ||
        msg->length > 1024
    )
    {
        g_metrics.invalid_packets++;
        return false;
    }

    if (
        msg->type < JOIN ||
        msg->type > WAITING
    )
    {
        g_metrics.invalid_packets++;
        return false;
    }

    return true;
}

bool safe_receive_message(
    int sockfd,
    Message *msg
)
{
    int result =
        receive_message(sockfd, msg);

    if (result != PROTO_OK)
    {
        return false;
    }

    if (!validate_message(msg))
    {
        return false;
    }

    return true;
}

bool check_client_timeout(
    long long last_activity
)
{
    long long current_time =
        get_current_time_ms();

    return
        (
            current_time -
            last_activity
        ) > CLIENT_TIMEOUT_MS;
}

void watchdog_check(
    StateMachine *fsm
)
{
    time_t now = time(NULL);

    if (
        fsm->current ==
        STATE_WAIT_FOR_ANSWERS &&
        (
            now -
            fsm->stateStartTime
        ) > 30
    )
    {
        g_metrics.watchdog_resets++;

        printf(
            "[WATCHDOG] Freeze detected. Recovering...\n"
        );

        change_state(
            fsm,
            STATE_CALCULATE_RESULTS
        );
    }
}

bool handle_lobby_cooldown(
    long long *cooldown_start,
    bool *in_cooldown
)
{
    if (!(*in_cooldown))
        return false;

    long long current_time =
        get_current_time_ms();

    if (
        current_time -
        *cooldown_start >= 4000LL
    )
    {
        *in_cooldown = false;

        printf(
            "[STABILITY] Cooldown Finished\n"
        );

        return false;
    }

    return true;
}

void cleanup_server(
    Player *clients,
    int max_clients,
    int listen_sock
)
{
    printf(
        "[STABILITY] Graceful Shutdown Started\n"
    );

    Message exit_msg;

    build_message(
        &exit_msg,
        WAITING,
        "Server shutting down"
    );
for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].sockID != -1)
        {
            send_message(
                clients[i].sockID,
                &exit_msg
            );

            shutdown(
                clients[i].sockID,
                SHUT_RDWR
            );

            close(
                clients[i].sockID
            );

            reset_player(
                &clients[i]
            );
        }
    }

    shutdown(
        listen_sock,
        SHUT_RDWR
    );

    close(listen_sock);

    printf(
        "[STABILITY] Shutdown Complete\n"
    );
}
