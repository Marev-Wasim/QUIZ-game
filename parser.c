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

            // Load 4 options
            for (int i = 0; i < 4; i++)
            {
                token = strtok(NULL, "|");
                if (token) strncpy(q.options[i], token, sizeof(q.options[i]));
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

int main()
{
    QuestionBank bank;
    init_bank(&bank, 10); // Start with space for 10 questions

    printf("Loading question bank.\n");

    if (load_questions("QuestionBank.dat", &bank) == 0)
    {
        printf("Successfully loaded %zu questions.\n", bank.size);
    }
    else
    {
        printf("Failed to load questions.\n");
        return EXIT_FAILURE;
    }

    printf(bank.items[0].question_text);

    // Clean up before server shutdown
    free_bank(&bank);
    return 0;
}
