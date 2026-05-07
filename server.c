#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>      

#include "NET_CORE/unp.h"
#include "NET_CORE/config.h" 

#include "player.h"       
#include "stability.h"
#include "questions.h"
#include "StateMachine.h" 
#include "Protocol.h"

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
    long question_start_time_ms = 0; // Helper to track the 10-second timer

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
        tv.tv_usec = 100000; 

        int num = Select(maxfd + 1, &loop_set, NULL, NULL, &tv);

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
                    build_message(&msg, WAITING, "Welcome! Waiting for the game to start...");
                    send_message(conn_sock, &msg);
                    break;
                }
            }
        }

        
        for(int i = 0; i < MAX_CLIENTS; i++) 
        {
            int current_sock = clients[i].sockID;
            
            if(current_sock != -1 && FD_ISSET(current_sock, &loop_set))
            {
                int n = Recv(current_sock, buff, MAX_BUFFER, 0);
                
                if(n == 0) // Client Disconnected
                {
                    printf("Player %d disconnected\n", current_sock);
                    lock_data();
                    clients[i].sockID = -1;
                    fsm.activePlayers--;
                    FD_CLR(current_sock, &rset);
                    Close(current_sock);
                    unlock_data();
                }
                else if(n > 0)
                {
                    Message msg;
                    int status = deserialize(buff, n, &msg);

                    if (status == PROTO_OK && msg.type == ANSWER)
                    {
                        // Check if we are currently accepting answers
                        if (fsm.current == STATE_WAIT_FOR_ANSWERS && !clients[i].hasAnswered)
                        {
                            lock_data();
                            clients[i].hasAnswered = true;
                            clients[i].lastAnswer = atoi(msg.data); // Assuming client sends "1", "2", etc.
                            gettimeofday(&clients[i].responseTime, NULL); // Mark exact answer time
                            fsm.answersReceived++;
                            unlock_data();
                            
                            printf("Received answer from FD %d\n", current_sock);
                        }
                    }
                }
            }
        }

        switch (fsm.current)
        {
            case STATE_LOBBY:
                // Start game if we have at least 2 players (You can change this rule)
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

                // Reset player states for the new question
                fsm.answersReceived = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        clients[i].hasAnswered = false;
                        clients[i].lastAnswer = -1;
                    }
                }

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
                
                // Start the timer
                question_start_time_ms = get_current_time_ms();
                gettimeofday(&fsm.Q_sent_time, NULL);
                fsm.current = STATE_WAIT_FOR_ANSWERS;
                break;

            case STATE_WAIT_FOR_ANSWERS:
                // Check if 10 seconds have passed OR everyone has answered
                long current_time = get_current_time_ms();
                if ((current_time - question_start_time_ms >= 10000) || 
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
                            // Speed bonus logic! 
                            long response_ms = (clients[i].responseTime.tv_sec * 1000 + clients[i].responseTime.tv_usec / 1000) - question_start_time_ms;
                            int points = 10000 / (response_ms > 0 ? response_ms : 1);
                            clients[i].score += points;
                        }
                    }
                }
                unlock_data();

                // Move to the next question
                fsm.currentQuestionIdx++;
                fsm.current = STATE_SEND_QUESTION;
                break;

            case STATE_GAME_OVER:
                printf("Game Over! Shutting down...\n");
                
                Message end_msg;
                build_message(&end_msg, GAME_RESULT, "Game Over! Thanks for playing.");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].sockID != -1) {
                        send_message(clients[i].sockID, &end_msg);
                        Close(clients[i].sockID);
                    }
                }
                
                free_bank(&bank);
                return 0; // Exit the engine naturally
        }
    }

    free_bank(&bank);
    return 0;
}
