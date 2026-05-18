
#include <stdio.h>
#include <string.h>
#include "server_ui.h"
#include "../Game Helpers/speed_bonus.h"

// تلوين اسم اللاعب والـ joined باللون الأحمر بناءً على طلبك
void display_player_joined(const char* name) {
    printf(COLOR_RED "🔴 Player Joined: %s\n" COLOR_RESET, name);
}

// دالة جديدة لطباعة مغادرة اللاعب باللون الأحمر
void display_player_left(const char* name) {
    printf(COLOR_RED "❌ Player Left: %s\n" COLOR_RESET, name);
}

void display_game_started() {
    printf(COLOR_MAGENTA "\n--------------------------------------------------\n");
    printf("                  GAME STARTED                    \n");
    printf("--------------------------------------------------\n\n" COLOR_RESET);
}

void display_round_status(int currentRoundIdx, Player clients[], int max_clients) {
    printf(COLOR_CYAN "\n[Round %d Status]:\n" COLOR_RESET, currentRoundIdx + 1);
    for (int i = 0; i < max_clients; i++) {
        if (clients[i].sockID != -1) {
            // التحقق إذا كان الاسم فارغاً تجنباً لطباعة رموز غريبة قبل استقبال رسالة JOIN
            const char* player_name = (strlen(clients[i].name) > 0) ? clients[i].name : "Connecting...";

            if (clients[i].hasAnswered) {
                printf(COLOR_GREEN " ✔ %s: Answered\n" COLOR_RESET, player_name);
            } else {
                printf(COLOR_RED " ✘ %s: Did not answer\n" COLOR_RESET, player_name);
            }
        }
    }
    printf("\n");
}

void display_game_ended(Player clients[], int max_clients) {
    printf(COLOR_RED "\n                    GAME ENDED                    \n" COLOR_RESET);

    printf(COLOR_YELLOW "\n==================================================\n");
    printf("🏆 👑 🏆        FINAL LEADERBOARD       🏆 👑 🏆\n");
    printf("==================================================\n");

    Player temp_players[max_clients];
    int count = 0;
    for (int i = 0; i < max_clients; i++) {
        if (clients[i].sockID != -1) {
            temp_players[count++] = clients[i];
        }
    }

    // ترتيب اللاعبين تنازلياً حسب السكور
    sort_leaderboard(temp_players, count);

    for (int i = 0; i < count && i < 3; i++) {
        const char* p_name = (strlen(temp_players[i].name) > 0) ? temp_players[i].name : "Unknown Player";
        if (i == 0) printf(" 🥇 1st Place: %s - %d pts\n", p_name, temp_players[i].score);
        else if (i == 1) printf(" 🥈 2nd Place: %s - %d pts\n", p_name, temp_players[i].score);
        else if (i == 2) printf(" 🥉 3rd Place: %s - %d pts\n", p_name, temp_players[i].score);
    }
    printf("==================================================\n\n" COLOR_RESET);
}
