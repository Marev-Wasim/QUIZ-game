#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <sys/time.h>
#include <time.h>

typedef enum
{
    STATE_LOBBY,
    STATE_SEND_QUESTION,
    STATE_WAIT_FOR_ANSWERS,
    STATE_CALCULATE_RESULTS,
    STATE_GAME_OVER,
    STATE_SCORE_DISPLAY,
    STATE_ASK_REPLAY,
    STATE_WAIT_REPLAY
} MainState;

typedef enum { LISTEN, PROCESSING, BROADCAST } SubState;

typedef struct
{
    MainState current;
    SubState status;

    int currentQuestionIdx;
    int activePlayers;
    int answersReceived;

    long long Q_sent_time;
    time_t stateStartTime;
} StateMachine;

void initialize(StateMachine *m);
void update(StateMachine *m);

// 🔄 تعريف الدالة هنا لكي يراها ملف stability-3.c في السطر 208
void change_state(StateMachine *fsm, MainState new_state);

#endif
