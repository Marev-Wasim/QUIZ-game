#include "Protocol.h"
#include <string.h>
#include <errno.h>

void build_message(Message* msg, int type, const char* data) {
    msg->type = type;
    msg->length = strlen(data);
    strncpy(msg->data, data, 1023);
    msg->data[1023] = '\0';
}

int deserialize(char *buffer, int n, Message *msg) {

    if (n < 2 * sizeof(int))
        return PROTO_ERR_LENGTH;

    int type, length;

    memcpy(&type, buffer, sizeof(int));
    memcpy(&length, buffer + sizeof(int), sizeof(int));

    msg->type = ntohl(type);
    msg->length = ntohl(length);

    if (msg->length < 0 || msg->length > 1024)
        return PROTO_ERR_LENGTH;

    if (msg->length > n - 2*sizeof(int))
        return PROTO_ERR_LENGTH;

    if (msg->type < JOIN || msg->type > WAITING)
        return PROTO_ERR_TYPE;

    memcpy(msg->data,
           buffer + 2*sizeof(int),
           msg->length);

    msg->data[msg->length] = '\0';

    return PROTO_OK;
}

int serialize(Message* msg, char* buffer) {

    int type = htonl(msg->type);
    int length = htonl(msg->length);

    memcpy(buffer, &type, sizeof(int));
    memcpy(buffer + sizeof(int), &length, sizeof(int));
    memcpy(buffer + 2 * sizeof(int), msg->data, msg->length);

    return 2 * sizeof(int) + msg->length;
}

int send_message(int sockfd, Message* msg) {
    char buffer[2048];

    int total_size = serialize(msg,buffer);

    return writen(sockfd, buffer, total_size);
}

int recv_full(int sockfd, char *buffer, int length) {
    int total = 0;
    int bytes;

    while (total < length) {
        bytes = recv(sockfd, buffer + total, length - total, 0);

        if (bytes <= 0) {
            return -1; // error or connection closed
        }

        total += bytes;
    }

    return total;
}

int receive_message(int sockfd, Message* msg) {
    char header[8];

    // read header  (type + length)
    if (recv_full(sockfd, header, 8) < 0)
        return PROTO_ERR_DISCONNECT;

    int type, length;

    //deserialize part
    memcpy(&type, header, 4);
    memcpy(&length, header + 4, 4);

    msg->type = ntohl(type);
    msg->length = ntohl(length);

    // validation
    if (msg->length > 1024 || msg->length < 0)
        return PROTO_ERR_LENGTH;   //corrupted message
    //if (msg->type < JOIN || msg->type > ERROR_MSG) the old line
    if (msg->type < JOIN || msg->type > WAITING) //Edited by shahd
        return PROTO_ERR_TYPE;

    // read data
    if (recv_full(sockfd, msg->data, msg->length) < 0)
        return PROTO_ERR_RECV;

    msg->data[msg->length] = '\0';

    return PROTO_OK;
}

ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return(-1);
        }
        nleft -= nwritten;
        ptr
        += nwritten;
    }
    return(n);
}

void Writen(int fd, void *ptr, size_t nbytes)
{
    if (writen(fd, ptr, nbytes) != nbytes)
        err_sys("writen error");
}
