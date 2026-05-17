#ifndef STABILITY_H
#define STABILITY_H

#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "player.h"
#include "StateMachine.h"
#include "../Protocol/Protocol.h"

#define MAX_MSG_PER_SECOND 5
#define CLIENT_TIMEOUT_MS 10000

typedef struct
{
    int flood_attempts;
    int invalid_packets;
    int disconnected_clients;
    int watchdog_resets;
} StabilityMetrics;

extern volatile sig_atomic_t got_sigint;
extern StabilityMetrics g_metrics;

void setup_stability();
long long get_current_time_ms();
void clean_zombie_processes(); // 

bool check_flood_control(int sock, int *msg_count, long long *last_msg_time_ms);
bool validate_message(Message *msg);
bool safe_receive_message(int sockfd, Message *msg);
bool check_client_timeout(long long last_activity);
void watchdog_check(StateMachine *fsm);
bool handle_lobby_cooldown(long long *cooldown_start, bool *in_cooldown);

// Player 
void cleanup_server(Player *clients, int max_clients, int listen_sock);
void reset_player(Player *p);
void set_nonblocking(int sock);

#endif
