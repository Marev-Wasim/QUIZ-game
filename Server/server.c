#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>

#include "../NET_CORE/config.h"
#include "../NET_CORE/unp.h"

#include "../Game Helpers/player.h"
#include "../Game Helpers/stability.h"
#include "../Game Helpers/questions.h"
#include "../Game Helpers/StateMachine.h"
#include "../Protocol/Protocol.h"
#include "../Game Helpers/speed_bonus.h"

int main()
{
    // Stability Setup
    setup_stability();

    // Question Bank Setup
    QuestionBank bank;
    init_bank(&bank, 10);
    printf("Loading question bank...\n");
    if (load_questions("QuestionBank.dat", &bank) != 0)
    {
        printf("Failed to load questions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    printf("Successfully loaded %zu questions.\n", bank.size);

    // Server & Player Setup
    Player clients[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].sockID = -1;
        clients[i].score = 0;
        clients[i].hasAnswered = false;
        clients[i].lastAnswer = -1;
        clients[i].responseTime.tv_sec = 0;
        clients[i].responseTime.tv_usec = 0;
    }

    // State Machine Initialization
    StateMachine fsm;
    fsm.current = STATE_LOBBY;
    fsm.status = LISTEN;
    fsm.currentQuestionIdx = 0;
    fsm.activePlayers = 0;
    fsm.answersReceived = 0;
    long question_start_time_ms = 0;
    
    // New timers for the Replay feature
    long score_display_start_ms = 0;
    long replay_question_start_ms = 0;

    // Network Socket Setup
    struct in_addr my_ip;
    struct sockaddr_in my_sock_addr;
    int listen_sock;
    char buff[MAX_BUFFER];

    int result = inet_pton(AF_INET, "0.0.0.0", &my_ip);
    if(result == 0) {
        printf("Address is not a valid format\n"); exit(1);
    } else if(result == -1) {
        printf("Error converting string to address\n"); exit(1);
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

    printf("Engine running. Waiting for players on port %d...\n", PORT);

    for(;;)
    {
        fd_set loop_set = rset;
        int maxfd = listen_sock;

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if(clients[i].sockID > maxfd) maxfd = clients[i].sockID;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms heartbeat

        int num = Select(maxfd + 1, &loop_set, NULL, NULL, &tv);

        // Handle New Connections
        if(FD_ISSET(listen_sock, &loop_set))
        {
            int conn_sock = Accept(listen_sock, NULL, NULL);
            printf("New player connected! FD: %d\n", conn_sock);

            for(int i = 0; i < MAX_CLIENTS; i++)
            {
                if(clients[i].sockID == -1)
                {
                    lock_data();
                    clients[i].sockID = conn_sock;
                    clients[i].score = 0;
                    clients[i].hasAnswered = false;
                    clients[i].lastAnswer = -1;
                    fsm.activePlayers++;
                    FD_SET(conn_sock, &rset);
                    unlock_data();

                    Message msg;
                    build_message(&msg, WAITING, "Welcome! Waiting for more players to join...");
                    send_message(conn_sock, &msg);
                    break;
                }
            }
        }

        // Handle Incoming Data from Current Players
        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            int current_sock = clients[i].sockID;

            if(current_sock != -1 && FD_ISSET(current_sock, &loop_set))
            {
                // Use standard recv to prevent crash
                int n = recv(current_sock, buff, MAX_BUFFER, 0);

                if(n <= 0) // Disconnect or Error
                {
                    if (n == 0) printf("Player %d disconnected gracefully.\n", current_sock);
                    else printf("Player %d dropped (Error).\n", current_sock);

                    lock_data();
                    clients[i].sockID = -1;
                    fsm.activePlayers--;
                    FD_CLR(current_sock, &rset);
                    Close(current_sock);
                    unlock_data();
                }
                else if(n > 0) // Received Data
                {
                    Message msg;
                    int status = deserialize(buff, n, &msg);

                    if (status == PROTO_OK && msg.type == ANSWER)
                    {
                        // Check if we are waiting for normal questions OR replay question
                        if ((fsm.current == STATE_WAIT_FOR_ANSWERS || fsm.current == STATE_WAIT_REPLAY) && !clients[i].hasAnswered)
                        {
                            lock_data();
                            clients[i].hasAnswered = true;
                            clients[i].lastAnswer = atoi(msg.data);
                            gettimeofday(&clients[i].responseTime, NULL);
                            fsm.answersReceived++;
                            unlock_data();

                            printf("Received answer '%d' from FD %d\n", clients[i].lastAnswer, current_sock);
                        }
                    }
                }
            }
        }

        // Game State Machine Logic
        switch (fsm.current)
        {
            case STATE_LOBBY:
                // Start if at least 2 players
                if (fsm.activePlayers >= 2)
                {
                    printf("Enough players joined. Starting Game!\n");
                    fsm.current = STATE_SEND_QUESTION;
                }
                break;

            case STATE_SEND_QUESTION:
                if (fsm.currentQuestionIdx >= bank.size)
                {
                    fsm.current = STATE_GAME_OVER;
                    break;
                }

                Question *q = &bank.items[fsm.currentQuestionIdx];
                char packet_data[1024];

                fsm.answersReceived = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        clients[i].hasAnswered = false;
                        clients[i].lastAnswer = -1;
                    }
                }

                // Format Question
                snprintf(packet_data, sizeof(packet_data), "%s\n1) %s\n2) %s\n3) %s\n4) %s",
                         q->question_text, q->options[0], q->options[1], q->options[2], q->options[3]);

                Message q_msg;
                build_message(&q_msg, QUESTION, packet_data);

                // Broadcast
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        send_message(clients[i].sockID, &q_msg);
                    }
                }

                printf("Sent Question %d to all players.\n", fsm.currentQuestionIdx + 1);

                question_start_time_ms = get_current_time_ms();
                gettimeofday(&fsm.Q_sent_time, NULL);
                fsm.current = STATE_WAIT_FOR_ANSWERS;
                break;

            case STATE_WAIT_FOR_ANSWERS:
                long current_time = get_current_time_ms();
                if ((current_time - question_start_time_ms >= QUESTION_TIMEOUT * 1000) ||
                    (fsm.answersReceived >= fsm.activePlayers && fsm.activePlayers > 0))
                {
                    fsm.current = STATE_CALCULATE_RESULTS;
                }
                break;

            case STATE_CALCULATE_RESULTS:
                lock_data();
                int correct_ans = bank.items[fsm.currentQuestionIdx].correct_index;

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].sockID != -1 && clients[i].hasAnswered)
                    {
                        if (clients[i].lastAnswer == correct_ans)
                        {
                            double response_time_sec = (double)(clients[i].responseTime.tv_sec - fsm.Q_sent_time.tv_sec) + (double)(clients[i].responseTime.tv_usec - fsm.Q_sent_time.tv_usec) / 1000000.0;
                            clients[i].score += calc_score(response_time_sec);
                        }
                    }
                }
                unlock_data();

                fsm.currentQuestionIdx++;
                fsm.current = STATE_SEND_QUESTION;
                break;

            case STATE_GAME_OVER:
                printf("Game Over! Calculating final rankings...\n");

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

                // Bubble Sort descending
                for (int i = 0; i < active_count - 1; i++) {
                    for (int j = 0; j < active_count - i - 1; j++) {
                        if (rankList[j].score < rankList[j+1].score) {
                            PlayerRank temp = rankList[j];
                            rankList[j] = rankList[j+1];
                            rankList[j+1] = temp;
                        }
                    }
                }

                // Build Leaderboard
                char leaderboard[512] = "";
                snprintf(leaderboard, sizeof(leaderboard), "\n--- [ TOP 3 PLAYERS ] ---\n");
                for (int i = 0; i < 3 && i < active_count; i++) {
                    char row[128];
                    snprintf(row, sizeof(row), "Place %d: Player (FD %d) - Score: %d\n", i + 1, rankList[i].sockID, rankList[i].score);
                    strcat(leaderboard, row);
                }

                lock_data();
                for (int i = 0; i < active_count; i++) {
                    char final_payload[1024];
                    int current_place = i + 1;

                    if (current_place == 1) {
                        snprintf(final_payload, sizeof(final_payload),
                                 "*** YOU ARE THE WINNER!!! ***\n"
                                 "Your Final Score: %d\n"
                                 "Your Rank: %d\n"
                                 "%s", rankList[i].score, current_place, leaderboard);
                    } else {
                        snprintf(final_payload, sizeof(final_payload),
                                 "Your Final Score: %d\n"
                                 "Your Rank: %d\n"
                                 "%s", rankList[i].score, current_place, leaderboard);
                    }

                    Message end_msg;
                    build_message(&end_msg, GAME_RESULT, final_payload);
                    send_message(rankList[i].sockID, &end_msg);
                }
                unlock_data();

                // Non-blocking timer setup
                printf("Displaying scores for 15 seconds...\n");
                score_display_start_ms = get_current_time_ms();
                fsm.current = STATE_SCORE_DISPLAY;
                break;

            case STATE_SCORE_DISPLAY:
                // Wait 15 seconds without blocking the server
                if (get_current_time_ms() - score_display_start_ms >= 15000)
                {
                    fsm.current = STATE_ASK_REPLAY;
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

                // Send a fake QUESTION to trigger the client's answer logic
                Message replay_msg;
                char replay_text[] = "Do you want to play another round?\n1) Yes, let's go!\n2) No, I want to quit\n3) -\n4) -";
                build_message(&replay_msg, QUESTION, replay_text);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        send_message(clients[i].sockID, &replay_msg);
                    }
                }

                replay_question_start_ms = get_current_time_ms();
                fsm.current = STATE_WAIT_REPLAY;
                break;

            case STATE_WAIT_REPLAY:
                long replay_curr_time = get_current_time_ms();
                // Check if timeout (10 seconds) OR everyone answered
                if ((replay_curr_time - replay_question_start_ms >= 10000) ||
                    (fsm.answersReceived >= fsm.activePlayers && fsm.activePlayers > 0))
                {
                    lock_data();
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].sockID != -1) {
                            // If they chose anything other than 1, kick them out
                            if (clients[i].lastAnswer != 1) {
                                printf("Player %d chose not to replay. Disconnecting...\n", clients[i].sockID);
                                FD_CLR(clients[i].sockID, &rset);
                                Close(clients[i].sockID);
                                clients[i].sockID = -1;
                                fsm.activePlayers--;
                            } else {
                                // Reset stats for those who stay
                                clients[i].score = 0;
                                clients[i].hasAnswered = false;
                                clients[i].lastAnswer = -1;
                                clients[i].responseTime.tv_sec = 0;
                                clients[i].responseTime.tv_usec = 0;
                            }
                        }
                    }

                    // UX Feature: Notify remaining player if they are alone
                    if (fsm.activePlayers == 1) {
                        Message wait_msg;
                        build_message(&wait_msg, WAITING, "Your friend left! Waiting for a new challenger to join...");
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (clients[i].sockID != -1) {
                                send_message(clients[i].sockID, &wait_msg);
                            }
                        }
                    }
                    unlock_data();

                    printf("Remaining players: %d. Returning to Lobby.\n", fsm.activePlayers);
                    fsm.current = STATE_LOBBY;
                    fsm.currentQuestionIdx = 0;
                    fsm.answersReceived = 0;
                }
                break;
        }
    }

    free_bank(&bank);
    return 0;
}
