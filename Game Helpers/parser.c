#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stability.h"
#include "questions.h"

///This logic should be called at your server's startup to populate the bank.

// Initializes the bank with an initial capacity
void init_bank(QuestionBank *bank, size_t initial_cap)
{
    bank->items = malloc(initial_cap * sizeof(Question));
    bank->size = 0;
    bank->capacity = initial_cap;
}

// Adds a question, resizing if necessary (Vector-like behavior)
void add_question(QuestionBank *bank, Question q)
{
    if (bank->size >= bank->capacity)
    {
        bank->capacity *= 2;
        bank->items = realloc(bank->items, bank->capacity * sizeof(Question));
    }
    bank->items[bank->size++] = q;
}

int load_questions(const char *filename, QuestionBank *bank)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Could not open file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file))
    {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        Question q;
        char *token = strtok(line, "|");

        if (token)
        {
            strncpy(q.question_text, token, sizeof(q.question_text));
            q.question_text[sizeof(q.question_text)-1] = '\0';

            // Load 4 options
            for (int i = 0; i < 4; i++)
            {
                token = strtok(NULL, "|");
                if (token)
                {
                    strncpy(q.options[i], token, sizeof(q.options[i]));
                    q.options[i][sizeof(q.options[i]) - 1] = '\0'; // الحماية التي أضفناها سابقاً
                }
            }

            // Load Correct Index
            token = strtok(NULL, "|");
            if (token) q.correct_index = atoi(token);

            add_question(bank, q);
        }
    }

    fclose(file);
    return 0;
}

void free_bank(QuestionBank *bank)
{
    free(bank->items);
}
