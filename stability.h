#ifndef STABILITY_H
#define STABILITY_H

#include <pthread.h>

// الميوتكس اللي بيحمي بيانات اللاعبين (الكل هيستخدمه)
extern pthread_mutex_t shared_mutex;

// دالة تفعيل الحماية والـ Signals
void setup_stability();

// دوال القفل والفتح عشان محدش يجاوب في نفس لحظة التاني
void lock_data();
void unlock_data();

// دالة حساب الوقت بالملي ثانية (عشان تحديد مين الأسرع)
long get_current_time_ms();

#endif
