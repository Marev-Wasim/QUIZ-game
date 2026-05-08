#include "StateMachine.h"
#include <stdio.h>
#include <stdlib.h>

/* * Internal Helper: Calculates the elapsed time in seconds.
 * Integrated from: CALCULATE_RESULTS flow
 */
double calculate_delta(struct timeval start, struct timeval end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_usec - start.tv_usec) / 1000000.0;
}

void initialize(StateMachine *m)
{
    m->current = STATE_LOBBY;
    m->status = LISTEN;
    m->currentQuestionIdx = 0;
    m->activePlayers = 0;
    m->answersReceived = 0;
    printf("Game Brain Initialized: Waiting in LOBBY.\n");
}

void update(StateMachine *m)
{
    switch (m->current)
    {

        case STATE_LOBBY:
            /* Transition to SEND_QUESTION once players are ready */
            if (m->activePlayers >= 2)
            {
                m->current = STATE_SEND_QUESTION;
            }
            break;

        case STATE_SEND_QUESTION:
            /* Logic: Broadcast question to all sockets via send() loop */
            printf("Broadcasting Question %d...\n", m->currentQuestionIdx + 1);

            /* TIMING POINT: Call immediately after send() loop */
            gettimeofday(&m->Q_sent_time, NULL);

            m->status = LISTEN;
            m->current = STATE_WAIT_FOR_ANSWERS;
            break;

        case STATE_WAIT_FOR_ANSWERS:
            /* * Logic: The server's main loop calls recv().
             * When a correct answer is identified, capture end_time.
             */
            if (m->answersReceived >= m->activePlayers)
            {
                m->current = STATE_CALCULATE_RESULTS;
            }
            break;

        case STATE_CALCULATE_RESULTS:
            printf("Processing results for round %d...\n", m->currentQuestionIdx);

            /* * Logic: Iterate through players who answered correctly.
             * Compute delta and update scores.
             */

            /* Transition: Check if there are more questions in the .dat file */
            if (m->currentQuestionIdx < 10)  // Example limit
            {
                m->currentQuestionIdx++;
                m->answersReceived = 0;
                m->current = STATE_SEND_QUESTION;
            }
            else
            {
                m->current = STATE_GAME_OVER;
            }
            break;

        case STATE_GAME_OVER:
            /* Final leaderboard broadcast and cleanup */
            printf("Game Over. Returning to Lobby.\n");
            m->current = STATE_LOBBY;
            break;
    }
}
