#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

void encoder_init();
void encoder_reset();

long encoder_get_ticks_fl();
long encoder_get_ticks_fr();
long encoder_get_ticks_bl();
long encoder_get_ticks_br();

float encoder_get_speed_fl();
float encoder_get_speed_fr();
float encoder_get_speed_bl();
float encoder_get_speed_br();

void encoder_update_speeds(float dt);

// อ่านค่า Encoder ทั้งหมดแบบ atomic สำหรับงาน Telemetry/diagnostics
// เพื่อป้องกันการอ่านค่า long/float ขณะอีก task กำลังอัปเดตอยู่บน AVR
struct EncoderSnapshot {
    long ticks_fl;
    long ticks_fr;
    long ticks_bl;
    long ticks_br;
    float speed_fl;
    float speed_fr;
    float speed_bl;
    float speed_br;
};

void encoder_get_snapshot(EncoderSnapshot *snapshot);

#endif
