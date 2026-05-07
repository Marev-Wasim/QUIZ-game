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
#include "StateMachine.h" // 🔴 Game Logic Lead's header

/* 🔴 TODO [Protocol Designer]:  DONE
 * Include your protocol header here once it is ready.
 * #include "protocol.h" 
 */
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
        exit(1);
    }
    printf("Successfully loaded %zu questions.\n", bank.size);

    // Server Setup
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

        /* 🔴 TODO [Game Logic Lead]: 
         * To implement the timer per question, replace the last NULL 
         * in Select() with a struct timeval timeout. 
         */
        int num = Select(maxfd + 1, &loop_set, NULL, NULL, NULL);

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
                    clients[i].responseTime.tv_sec = 0;
                    clients[i].responseTime.tv_usec = 0;
                    FD_SET(conn_sock, &rset); 
                    unlock_data();

                    /* 🔴 TODO [Protocol Designer]: DONE
                     * A new player just joined. Construct a "Welcome" or "Wait" packet 
                     * using your protocol structs, serialize it, and send it to 'conn_sock'.
                     */
                     Message msg;

                    build_message(&msg, WAITING, "Waiting for more players...");
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
                int n = Recv(current_sock, buff, MAX_BUFFER, 0);
                
                if(n == 0)
                {
                    printf("Player %d disconnected\n", current_sock);
                    lock_data();
                    clients[i].sockID = -1;
                    FD_CLR(current_sock, &rset);
                    Close(current_sock);
                    unlock_data();
                }
                else if(n > 0)
                {
                    /* 🔴 TODO [Protocol Designer]: DONE
                     * You have received 'n' raw bytes inside 'buff'.
                     * Write a function to deserialize this raw buffer into 
                     * your Protocol Struct so the Game Logic can process it.
                     */
                     Message msg;

                    /* Convert raw received bytes into a Message struct */
                    int status = deserialize(buff, n, &msg);

                    if (status != PROTO_OK)
                    {
                        printf("Invalid or corrupted packet received\n");
                        return;
                    }

                    /* 🔴 TODO [Game Logic Lead]:
                     * Once the packet is parsed by the protocol designer, check if it is an ANSWER.
                     * Compare the player's answer against bank.items[current_q].correct_index.
                     * Calculate the score based on get_current_time_ms() and update 
                     * clients[i].score.
                     */
                }
            }
        }

        /* 🔴 TODO [Protocol Designer] & 🔴 TODO [Game Logic Lead]: Protocol DONE
         * This area runs outside of the FD checks. Logic is needed here to check
         * if the question timer is up. If it is:
         * 1. (Game Logic Lead): Move to the next question index.
         * 2. (Protocol Designer): Build a Question Packet, serialize it, and loop 
         * through 'clients' array to broadcast it to all active players.
         */

        /* Current question number */
        static int current_question_index = 0;

        /* Check if there are still questions left */
        if (current_question_index >= bank.size)
        {
            printf("No more questions available.\n");
        }
        else
        {
            Message msg;

            /* Get current question */
            Question *q =
                &bank.items[current_question_index];

            char packet_data[1024];

            /* Build formatted question packet */
            snprintf(packet_data,
                     sizeof(packet_data),
                     "%s\n"
                     "A) %s\n"
                     "B) %s\n"
                     "C) %s\n"
                     "D) %s",
                     q->question_text,
                     q->options[0],
                     q->options[1],
                     q->options[2],
                     q->options[3]);

            /* Build protocol message */
            build_message(&msg, QUESTION, packet_data);

            /* Broadcast question to all active clients */
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].sockID != -1)
                {
                    int status =
                        send_message(clients[i].sockID, &msg);

                    if (status < 0)
                    {
                        printf("Failed to send question to client %d\n", i);
                    }
                }
            }

            /* Move to next question */
            current_question_index++;
        }
    }

    free_bank(&bank);
    return 0;
}
