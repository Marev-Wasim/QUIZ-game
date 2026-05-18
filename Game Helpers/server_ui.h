#ifndef SERVER_UI_H
#define SERVER_UI_H

#include "player.h"

// ================= ألوان الـ Terminal =================
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// تعريف دوال الواجهة
void display_player_joined(const char* name);
void display_game_started();
void display_round_status(int currentRoundIdx, Player clients[], int max_clients);
void display_game_ended(Player clients[], int max_clients);
void display_player_left(const char* name);

#endif
