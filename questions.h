#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 512

typedef struct
{
    char question_text[256];
    char options[4][64];
    int correct_index;
} Question;

typedef struct
{
    Question *items;
    size_t size;
    size_t capacity;
} QuestionBank;