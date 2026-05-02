//typedef enum { listen, start } Lobby;
//typedef enum { listen, nxtQ, brodcast } SendQuestion;
//typedef enum { listen, expire } WaitForAnswers;
//typedef enum { listen, compare, sendQ, gameOver} CalcResult;
//typedef enum { listen, display, lobby, close } GameOver;

//#include <sys/time.h>

typedef enum
{
    STATE_LOBBY, 
    STATE_SEND_QUESTION, 
    STATE_WAIT_FOR_ANSWERS, 
    STATE_CALCULATE_RESULTS, 
    STATE_GAME_OVER
} MainState;

typedef enum { LISTEN, PROCESSING, BROADCAST } SubState;

typedef struct {
    MainState current;     /* The active FSM phase */
    SubState status;       /* Internal status (e.g., listening vs broadcasting) */
    
    int currentQuestionIdx; /* Index for the dynamic question array */
    int activePlayers;     /* Number of players currently in the LOBBY */
    int answersReceived;   /* Counter to trigger transition from WAIT_FOR_ANSWERS */
    
    //struct timeval turnStartTime; /* Used with gettimeofday() for speed bonuses */
} StateMachine;

void initialize(StateMachine *m);
void update(StateMachine *m);