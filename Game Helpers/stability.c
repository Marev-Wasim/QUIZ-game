#include "stability.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

// تعريف الميوتكس فعلياً في الذاكرة
pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

// تنظيف العمليات الميتة (Zombies)
void handle_zombies(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_stability() {
    signal(SIGCHLD, handle_zombies); // حماية من الزومبيز
    signal(SIGPIPE, SIG_IGN);        // حماية من انهيار السيرفر لو كلاينت قفل
}

void lock_data() { pthread_mutex_lock(&shared_mutex); }
void unlock_data() { pthread_mutex_unlock(&shared_mutex); }

long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}
