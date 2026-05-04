# Protocol Module

## Overview

This module implements the communication protocol between the client and the server in the quiz game project.

It is responsible for:

* Building messages
* Serializing data before sending
* Receiving and parsing messages
* Ensuring safe and complete transmission over TCP

---

## Message Format

All messages follow this structure:

```
[type][length][data]
```

* **type (4 bytes)**: Message type (e.g. JOIN, QUESTION, ANSWER)
* **length (4 bytes)**: Size of the data in bytes
* **data (variable)**: The actual message content

All integers are converted to **network byte order** using:

* `htonl()` before sending
* `ntohl()` after receiving

---

## Message Structure

```c
typedef struct {
    int type;
    int length;
    char data[1024];
} Message;
```

* Maximum data size = **1024 bytes**

---

##  Functions

### 1. build_message

```c
void build_message(Message* msg, int type, const char* data);
```

* Initializes a message
* Sets type and length
* Copies data safely into the message

---

### 2. serialize

```c
int serialize(Message* msg, char* buffer);
```

* Converts a Message struct into a byte buffer
* Applies `htonl()` to type and length
* Returns total size of the serialized message

---

### 3. send_message

```c
int send_message(int sockfd, Message* msg);
```

* Serializes the message
* Sends it using `writen()` to ensure full transmission

---

### 4. receive_message

```c
int receive_message(int sockfd, Message* msg);
```

* Reads message from socket in two steps:

  1. Reads header (type + length)
  2. Reads data based on length
* Converts values using `ntohl()`
* Validates message before returning

---

### 5. recv_full

```c
int recv_full(int sockfd, char *buffer, int length);
```

* Ensures that exactly `length` bytes are received
* Handles partial reads from TCP

---

### 6. writen

```c
ssize_t writen(int fd, const void *vptr, size_t n);
```

* Ensures that all bytes are sent
* Handles interruptions (EINTR)

---

## Error Handling

The following error codes are used:

* `PROTO_OK` → Success
* `PROTO_ERR_DISCONNECT` → Connection closed
* `PROTO_ERR_LENGTH` → Invalid message length
* `PROTO_ERR_TYPE` → Unknown message type
* `PROTO_ERR_RECV` → Receive failure

---

## Important Notes

* TCP is a **stream protocol**, not message-based
  → Messages may arrive in parts

* Always use:

  * `send_message()` instead of `send()`
  * `receive_message()` instead of `recv()`
