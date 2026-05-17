#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "../NET_CORE/config.h"
#include "../NET_CORE/unp.h"


#include "../Game Helpers/StateMachine.h"
#include "../Game Helpers/player.h"
#include "../Game Helpers/stability.h"
#include "../Game Helpers/questions.h"
#include "../Protocol/Protocol.h"
#include "../Game Helpers/speed_bonus.h"


void change_state(StateMachine *fsm, MainState new_state) {
    if (fsm) {
        fsm->current = new_state;
        fsm->stateStartTime = time(NULL); 
    }
}

int main()
{
    // Initialize Advanced Stability Layer
    setup_stability();

    // Question Bank Setup
    QuestionBank bank;
    init_bank(&bank, 10);
    printf("Loading question bank...\n");
    
       if (load_questions("../Game Helpers/QuestionBank.dat", &bank) != 0) {
        printf("Failed to load questions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    printf("Successfully loaded %zu questions.\n", bank.size);

    // Player Setup
    Player clients[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockID = -1;
        clients[i].score = 0;
        clients[i].hasAnswered = false;
        clients[i].lastAnswer = -1;
        clients[i].responseTime = 0;
    }

    int msg_count[MAX_CLIENTS] = {0};
    long long last_msg_time_ms[MAX_CLIENTS] = {0};

    // State Machine Initialization
    StateMachine fsm;
    memset(&fsm, 0, sizeof(StateMachine));
    fsm.status = LISTEN;
    fsm.activePlayers = 0;
    fsm.answersReceived = 0;
    change_state(&fsm, STATE_LOBBY);

    long long score_display_start_ms = 0;
    long long replay_question_start_ms = 0;
    long long lobby_delay_start_ms = 0;
    bool in_lobby_delay = false;

    // Network Socket Setup
    struct in_addr my_ip;
    struct sockaddr_in my_sock_addr;
    int listen_sock;

    int result = inet_pton(AF_INET, "0.0.0.0", &my_ip);
    if(result <= 0) {
        printf("Invalid IP Address format\n"); exit(1);
    }

    my_sock_addr.sin_addr = my_ip;
    my_sock_addr.sin_port = htons(PORT);
    my_sock_addr.sin_family = AF_INET;

    listen_sock = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Bind(listen_sock, (void*)&my_sock_addr, sizeof(struct sockaddr_in));
    Listen(listen_sock, 5);

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(listen_sock, &rset);

    printf(" Engine running securely with Optimized Time on port %d...\n", PORT);


    for(;;)
    {
        // Fatal Signal Check
        if (got_sigint) {
            cleanup_server(clients, MAX_CLIENTS, listen_sock);
            exit(EXIT_SUCCESS);
        }

        // System Health Checks
        clean_zombie_processes(); 
        watchdog_check(&fsm);

        //Timeout Checks
        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(clients[i].sockID != -1) {
                if (last_msg_time_ms[i] > 0 && check_client_timeout(last_msg_time_ms[i])) {
                    printf("[STABILITY] Player %d timed out.\n", clients[i].sockID);
                    FD_CLR(clients[i].sockID, &rset);
                    Close(clients[i].sockID);
                    
                    clients[i].sockID = -1;
                    clients[i].responseTime = 0;
                    last_msg_time_ms[i] = 0;
                    msg_count[i] = 0;
                    fsm.activePlayers--;
                }
            }
        }

        fd_set loop_set = rset;
        int maxfd = listen_sock;

        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(clients[i].sockID > maxfd) maxfd = clients[i].sockID;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; 

        int num = Select(maxfd + 1, &loop_set, NULL, NULL, &tv);

        // Handle New Connections
        if(FD_ISSET(listen_sock, &loop_set))
        {
            int conn_sock = Accept(listen_sock, NULL, NULL);
            set_nonblocking(conn_sock);
            printf("New player connected! FD: %d\n", conn_sock);

            for(int i = 0; i < MAX_CLIENTS; i++)
            {
                if(clients[i].sockID == -1)
                {
                    clients[i].sockID = conn_sock;
                    clients[i].score = 0;
                    clients[i].hasAnswered = false;
                    clients[i].lastAnswer = -1;
                    clients[i].responseTime = 0;

                    last_msg_time_ms[i] = get_current_time_ms();
                    msg_count[i] = 0;
                    
                    fsm.activePlayers++;
                    FD_SET(conn_sock, &rset);

                    Message msg;
                    build_message(&msg, WAITING, "Welcome! Waiting for more players to join...");
                    send_message(conn_sock, &msg);
                    break;
                }
            }
        }

        // Handle Incoming Data
        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            int current_sock = clients[i].sockID;

            if(current_sock != -1 && FD_ISSET(current_sock, &loop_set))
            {
                Message msg;
                
                if (!safe_receive_message(current_sock, &msg)) {
                    printf("[STABILITY] Evicting client on FD %d (Disconnect, Flood, or Corrupt Packet).\n", current_sock);
                    FD_CLR(current_sock, &rset);
                    Close(current_sock);
                    clients[i].sockID = -1;
                    fsm.activePlayers--;
                    continue; 
                }

                if (!check_flood_control(current_sock, &msg_count[i], &last_msg_time_ms[i])) {
                    FD_CLR(current_sock, &rset);
                    Close(current_sock);
                    clients[i].sockID = -1;
                    fsm.activePlayers--;
                    continue;
                }

                if (msg.type == ANSWER) {
                    if ((fsm.current == STATE_WAIT_FOR_ANSWERS || fsm.current == STATE_WAIT_REPLAY) && !clients[i].hasAnswered) {
                        clients[i].hasAnswered = true;
                        clients[i].lastAnswer = atoi(msg.data);
                        
                        // ⏱️ تسجيل وقت الإجابة باستخدام دالة الميلي ثانية
                        clients[i].responseTime = get_current_time_ms();
                        
                        fsm.answersReceived++;
                        printf("Received valid answer '%d' from FD %d\n", clients[i].lastAnswer, current_sock);
                    }
                }
            }
        }

        // Game State Machine Logic
        switch (fsm.current)
        {
            case STATE_LOBBY:
                if (handle_lobby_cooldown(&lobby_delay_start_ms, &in_lobby_delay)) {
                    break;
                }
                if (fsm.activePlayers >= 2) {
                    printf("Enough players joined. Starting Game!\n");
                    change_state(&fsm, STATE_SEND_QUESTION);
                }
                break;

            case STATE_SEND_QUESTION:
                if (fsm.currentQuestionIdx >= bank.size) {
                    change_state(&fsm, STATE_GAME_OVER);
                    break;
                }

                Question *q = &bank.items[fsm.currentQuestionIdx];
                char packet_data[1024];

                fsm.answersReceived = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        clients[i].hasAnswered = false;
                        clients[i].lastAnswer = -1;
                        clients[i].responseTime = 0; 
                    }
                }

                snprintf(packet_data, sizeof(packet_data), "%s\n1) %s\n2) %s\n3) %s\n4) %s",
                         q->question_text, q->options[0], q->options[1], q->options[2], q->options[3]);

                Message q_msg;
                build_message(&q_msg, QUESTION, packet_data);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        send_message(clients[i].sockID, &q_msg);
                    }
                }

                printf("Sent Question %d to all players.\n", fsm.currentQuestionIdx + 1);

                fsm.Q_sent_time = get_current_time_ms();
                
                change_state(&fsm, STATE_WAIT_FOR_ANSWERS);
                break;

            case STATE_WAIT_FOR_ANSWERS:
                {
                    long long current_time = get_current_time_ms();
                    
                    if ((current_time - fsm.Q_sent_time >= QUESTION_TIMEOUT * 1000) ||
                        (fsm.answersReceived >= fsm.activePlayers && fsm.activePlayers > 0))
                    {
                        change_state(&fsm, STATE_CALCULATE_RESULTS);
                    }
                }
                break;

            case STATE_CALCULATE_RESULTS:
                {
                    int correct_ans = bank.items[fsm.currentQuestionIdx].correct_index;

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].sockID != -1 && clients[i].hasAnswered) {
                            if (clients[i].lastAnswer == correct_ans) {
                                double response_time_sec = (double)(clients[i].responseTime - fsm.Q_sent_time) / 1000.0;
                                clients[i].score += calc_score(response_time_sec);
                            }
                        }
                    }

                    fsm.currentQuestionIdx++;
                    change_state(&fsm, STATE_SEND_QUESTION);
                }
                break;

            case STATE_GAME_OVER:
                printf("Game Over! Calculating final rankings...\n");
                {
                    typedef struct {
                        int sockID;
                        int score;
                        int origIdx;
                    } PlayerRank;

                    PlayerRank rankList[MAX_CLIENTS];
                    int active_count = 0;

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].sockID != -1) {
                            rankList[active_count].sockID = clients[i].sockID;
                            rankList[active_count].score = clients[i].score;
                            rankList[active_count].origIdx = i;
                            active_count++;
                        }
                    }

                    // Bubble Sort
                    for (int i = 0; i < active_count - 1; i++) {
                        for (int j = 0; j < active_count - i - 1; j++) {
                            if (rankList[j].score < rankList[j+1].score) {
                                PlayerRank temp = rankList[j];
                                rankList[j] = rankList[j+1];
                                rankList[j+1] = temp;
                            }
                        }
                    }

                    char leaderboard[512] = "";
                    snprintf(leaderboard, sizeof(leaderboard), "\n--- [ TOP 3 PLAYERS ] ---\n");
                    for (int i = 0; i < 3 && i < active_count; i++) {
                        char row[128];
                        snprintf(row, sizeof(row), "Place %d: Player (FD %d) - Score: %d\n", i + 1, rankList[i].sockID, rankList[i].score);
                        strcat(leaderboard, row);
                    }

                    for (int i = 0; i < active_count; i++) {
                        char final_payload[1024];
                        int current_place = i + 1;

                        if (current_place == 1) {
                            snprintf(final_payload, sizeof(final_payload),
                                     "*** YOU ARE THE WINNER!!! ***\nYour Final Score: %d\nYour Rank: %d\n%s", 
                                     rankList[i].score, current_place, leaderboard);
                        } else {
                            snprintf(final_payload, sizeof(final_payload),
                                     "Your Final Score: %d\nYour Rank: %d\n%s", 
                                     rankList[i].score, current_place, leaderboard);
                        }

                        Message end_msg;
                        build_message(&end_msg, GAME_RESULT, final_payload);
                        send_message(rankList[i].sockID, &end_msg);
                    }

                    printf("Displaying scores for 15 seconds...\n");
                    score_display_start_ms = get_current_time_ms();
                    change_state(&fsm, STATE_SCORE_DISPLAY);
                }
                break;

            case STATE_SCORE_DISPLAY:
                if (get_current_time_ms() - score_display_start_ms >= 15000) {
                    change_state(&fsm, STATE_ASK_REPLAY);
                }
                break;

            case STATE_ASK_REPLAY:
                printf("Asking players if they want to replay...\n");
                fsm.answersReceived = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        clients[i].hasAnswered = false;
                        clients[i].lastAnswer = -1;
                    }
                }

                Message replay_msg;
                char replay_text[] = "Do you want to play another round?\n1) Yes, let's go!\n2) No, I want to quit\n3) -\n4) -";
                build_message(&replay_msg, QUESTION, replay_text);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        send_message(clients[i].sockID, &replay_msg);
                    }
                }

                replay_question_start_ms = get_current_time_ms();
                change_state(&fsm, STATE_WAIT_REPLAY);
                break;

            case STATE_WAIT_REPLAY:
                {
                    long long replay_curr_time = get_current_time_ms();
                    if ((replay_curr_time - replay_question_start_ms >= 10000) ||
                        (fsm.answersReceived >= fsm.activePlayers && fsm.activePlayers > 0))
                    {
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (clients[i].sockID != -1) {
                                if (clients[i].lastAnswer != 1) {
                                    printf("Player %d chose not to replay. Disconnecting...\n", clients[i].sockID);
                                    FD_CLR(clients[i].sockID, &rset);
                                    Close(clients[i].sockID);
                                    
                                    clients[i].sockID = -1;
                                    fsm.activePlayers--;
                                } else {
                                    clients[i].score = 0;
                                    clients[i].hasAnswered = false;
                                    clients[i].lastAnswer = -1;
                                    clients[i].responseTime = 0;
                                    last_msg_time_ms[i] = get_current_time_ms(); 
                                }
                            }
                        }

                        if (fsm.activePlayers == 1) {
                            Message wait_msg;
                            build_message(&wait_msg, WAITING, "Your friend left! Waiting for a new challenger to join...");
                            for (int i = 0; i < MAX_CLIENTS; i++) {
                                if (clients[i].sockID != -1) {
                                    send_message(clients[i].sockID, &wait_msg);
                                }
                            }
                        }

                        printf("Remaining players: %d. Returning to Lobby.\n", fsm.activePlayers);
                        fsm.currentQuestionIdx = 0;
                        fsm.answersReceived = 0;
                        
                        // Trigger lobby cooldown
                        in_lobby_delay = true;
                        lobby_delay_start_ms = get_current_time_ms();
                        change_state(&fsm, STATE_LOBBY);
                    }
                }
                break;
        }
    }

    free_bank(&bank);
    return 0;
}
