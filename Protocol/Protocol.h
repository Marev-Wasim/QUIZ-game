#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include <string.h>
#include <arpa/inet.h>

//message types
#define JOIN 1
#define ASSIGN_ID 2
#define START_GAME 3
#define QUESTION 4
#define ANSWER 5
#define RESULT 6
#define GAME_RESULT 7
#define ERROR_MSG 8

#define PROTO_OK                0
#define PROTO_ERR_DISCONNECT   -1
#define PROTO_ERR_LENGTH       -2
#define PROTO_ERR_TYPE         -3
#define PROTO_ERR_RECV         -4

typedef struct {
    int type;
    int length;
    char data[1024];
} Message;

void build_message(Message* msg, int type, const char* data) ;
int deserialize(char *buffer, int n, Message *msg) ;
int serialize(Message* msg, char* buffer) ;
int send_message(int sockfd, Message* msg) ;
int recv_full(int sockfd, char *buffer, int length) ;
int receive_message(int sockfd, Message* msg) ;
ssize_t writen(int fd, const void *vptr, size_t n);
void Writen(int fd, void *ptr, size_t nbytes);

#endif // PROTOCOL_H_INCLUDED
