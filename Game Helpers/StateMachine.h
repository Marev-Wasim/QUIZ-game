#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <sys/time.h>

typedef enum
{
    STATE_LOBBY, 
    STATE_SEND_QUESTION, 
    STATE_WAIT_FOR_ANSWERS, 
    STATE_CALCULATE_RESULTS, 
    STATE_GAME_OVER,
    STATE_SCORE_DISPLAY,  //----Added by Shahd----
    STATE_ASK_REPLAY,     //----Added by Shahd----
    STATE_WAIT_REPLAY     //----Added by Shahd---- 
} MainState;

typedef enum { LISTEN, PROCESSING, BROADCAST } SubState;

typedef struct
{
    MainState current;     /* The active FSM phase */
    SubState status;       /* Internal status (e.g., listening vs broadcasting) */
    
    int currentQuestionIdx; /* Index for the dynamic question array */
    int activePlayers;     /* Number of players currently in the LOBBY */
    int answersReceived;   /* Counter to trigger transition from WAIT_FOR_ANSWERS */
    
    struct timeval Q_sent_time; /* Used with gettimeofday() for speed bonuses */
    time_t stateStartTime;
} StateMachine;

void initialize(StateMachine *m);
void update(StateMachine *m);

#endif
