#include <errno.h>
#include "../NET_CORE/unp.h"
#include "../NET_CORE/config.h"
#include "../Protocol/Protocol.h" // عدلنا المسار عشان يقرأ من فولدر البروتوكول
#include <sys/select.h>

// ================= ألوان الـ Terminal =================
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_RESET   "\x1b[0m"
#define CLEAR_SCREEN  "\033[2J\033[H"

// تعريف احتياطي للـ WAITING
#ifndef WAITING
#define WAITING 9
#endif

// مش محتاجين نعرف الـ PORT هنا خلاص، هيتقرأ من config.h
#define SERVER_IP "127.0.0.1"

// (Typing Animation)
void type_text(const char* text, const char* color) {
    printf("%s", color);
    for (int i = 0; text[i] != '\0'; i++) {
        putchar(text[i]);
        fflush(stdout);
        usleep(15000);
    }
    printf("%s\n", COLOR_RESET);
}

void print_banner() {
    printf(CLEAR_SCREEN);
    printf(COLOR_MAGENTA);
    printf("==================================================\n");
    printf("               ✨ QUIZ GAME ✨             \n");
    printf("==================================================\n");
    printf(COLOR_RESET);
}

int main() {
    char player_name[64];

    print_banner();

    printf(COLOR_YELLOW "Enter your Hero Name: " COLOR_RESET);
    fgets(player_name, sizeof(player_name), stdin);
    player_name[strcspn(player_name, "\n")] = 0; // مسح الـ newline

    // =========================================================
    // استخدام الـ Book Helpers (Wrappers)
    // لاحظي إننا شلنا كل الـ (if < 0) لأن الـ Wrappers بتعمل كده لوحدها
    // =========================================================

    int sockfd = Socket(AF_INET, SOCK_STREAM, 0); // Socket بحرف كابيتال

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr)); // دالة من unp.h لتصفير الستراكت
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT); // استخدمنا PORT اللي جاية من config.h

    // الدالة الكابيتال بتعمل التشيك لوحدها
    Inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    printf(COLOR_BLUE "\nConnecting to the arena...\n" COLOR_RESET);

    // الدالة الكابيتال بتعمل التشيك وتقفل لوحدها لو السيرفر مش شغال
    Connect(sockfd, (SA *)&serv_addr, sizeof(serv_addr));

    printf(COLOR_GREEN "Connected successfully! Get ready, %s!\n" COLOR_RESET, player_name);

    fd_set readfds;
    int max_sd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

    // الـ Game Loop الرئيسي
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // استخدمنا Select بحرف كابيتال من الـ unp
        Select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // 1. لو في رسالة جاية من السيرفر
        if (FD_ISSET(sockfd, &readfds)) {
            Message msg;
            int status = receive_message(sockfd, &msg);

            // هنا خلينا الكلاينت يطبع رقم الإيرور عشان لو فصل نعرف المشكلة فين
            if (status == PROTO_ERR_DISCONNECT || status < 0) {
                printf(COLOR_RED "\n[!] Connection Error! Status Code: %d\n" COLOR_RESET, status);
                break;
            }

            switch (msg.type) {
                case WAITING:
                    printf(COLOR_YELLOW "\n⏳ %s\n" COLOR_RESET, msg.data);
                    break;

                case QUESTION:
                    print_banner();
                    printf(COLOR_BLUE "Target locked, %s! Answer quickly!\n\n" COLOR_RESET, player_name);
                    type_text(msg.data, COLOR_CYAN);
                    printf(COLOR_YELLOW "\n> Your Answer (1-4): " COLOR_RESET);
                    fflush(stdout);
                    break;

                case GAME_RESULT:
                    print_banner();
                    type_text(msg.data, COLOR_GREEN);
                    printf(COLOR_YELLOW "\nWaiting for the next round...\n" COLOR_RESET);
                    break;

                default:
                    printf(COLOR_CYAN "Server: %s\n" COLOR_RESET, msg.data);
                    break;
            }
        }

        // 2. لو اللاعب دخل إجابة من الكيبورد
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[10];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;

                if (strlen(input) > 0 && input[0] >= '1' && input[0] <= '4') {
                    Message answer_msg;
                    build_message(&answer_msg, ANSWER, input);
                    send_message(sockfd, &answer_msg);
                    printf(COLOR_GREEN "✓ Answer locked in! Waiting for others...\n" COLOR_RESET);
                } else {
                    printf(COLOR_RED "Invalid input! Please enter a number between 1 and 4.\n" COLOR_RESET);
                    printf(COLOR_YELLOW "> Your Answer (1-4): " COLOR_RESET);
                    fflush(stdout);
                }
            }
        }
    }

    Close(sockfd); // Close بحرف كابيتال
    return 0;
}
