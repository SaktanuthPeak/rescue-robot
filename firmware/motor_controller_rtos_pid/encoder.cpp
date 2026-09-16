#include "encoder.h"
#include "robot_config.h"

// จำนวนพัลส์สะสมนับผ่าน Hardware Interrupt
static volatile long ticks_fl = 0;
static volatile long ticks_fr = 0;
static volatile long ticks_bl = 0;
static volatile long ticks_br = 0;

static long last_ticks_fl = 0;
static long last_ticks_fr = 0;
static long last_ticks_bl = 0;
static long last_ticks_br = 0;

static float speed_fl = 0.0f;
static float speed_fr = 0.0f;
static float speed_bl = 0.0f;
static float speed_br = 0.0f;

// ----------------------------------------------------
// ISR สำหรับ Quadrature Encoder (Phase A บน Interrupt, Phase B เช็คทิศทาง)
// ----------------------------------------------------
void isr_enc_fl() {
    if (digitalRead(ENC_FL_B) == HIGH) {
        ticks_fl += ENC_FL_DIR;
    } else {
        ticks_fl -= ENC_FL_DIR;
    }
}

void isr_enc_fr() {
    if (digitalRead(ENC_FR_B) == HIGH) {
        ticks_fr += ENC_FR_DIR;
    } else {
        ticks_fr -= ENC_FR_DIR;
    }
}

void isr_enc_bl() {
    if (digitalRead(ENC_BL_B) == HIGH) {
        ticks_bl += ENC_BL_DIR;
    } else {
        ticks_bl -= ENC_BL_DIR;
    }
}

void isr_enc_br() {
    if (digitalRead(ENC_BR_B) == HIGH) {
        ticks_br += ENC_BR_DIR;
    } else {
        ticks_br -= ENC_BR_DIR;
    }
}

void encoder_init() {
    // กำหนดโหมดพินเป็น INPUT_PULLUP
    pinMode(ENC_FL_A, INPUT_PULLUP);
    pinMode(ENC_FL_B, INPUT_PULLUP);
    pinMode(ENC_FR_A, INPUT_PULLUP);
    pinMode(ENC_FR_B, INPUT_PULLUP);
    pinMode(ENC_BL_A, INPUT_PULLUP);
    pinMode(ENC_BL_B, INPUT_PULLUP);
    pinMode(ENC_BR_A, INPUT_PULLUP);
    pinMode(ENC_BR_B, INPUT_PULLUP);

    // ผูก Hardware Interrupt เข้ากับ Phase A ของแต่ละล้อ
    attachInterrupt(digitalPinToInterrupt(ENC_FL_A), isr_enc_fl, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_FR_A), isr_enc_fr, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_BL_A), isr_enc_bl, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_BR_A), isr_enc_br, RISING);
}

void encoder_reset() {
    noInterrupts();
    ticks_fl = 0;
    ticks_fr = 0;
    ticks_bl = 0;
    ticks_br = 0;
    interrupts();

    last_ticks_fl = 0;
    last_ticks_fr = 0;
    last_ticks_bl = 0;
    last_ticks_br = 0;

    speed_fl = 0.0f;
    speed_fr = 0.0f;
    speed_bl = 0.0f;
    speed_br = 0.0f;
}

long encoder_get_ticks_fl() {
    noInterrupts();
    long val = ticks_fl;
    interrupts();
    return val;
}

long encoder_get_ticks_fr() {
    noInterrupts();
    long val = ticks_fr;
    interrupts();
    return val;
}

long encoder_get_ticks_bl() {
    noInterrupts();
    long val = ticks_bl;
    interrupts();
    return val;
}

long encoder_get_ticks_br() {
    noInterrupts();
    long val = ticks_br;
    interrupts();
    return val;
}

float encoder_get_speed_fl() { return speed_fl; }
float encoder_get_speed_fr() { return speed_fr; }
float encoder_get_speed_bl() { return speed_bl; }
float encoder_get_speed_br() { return speed_br; }

void encoder_update_speeds(float dt) {
    if (dt <= 0.0f) return;

    noInterrupts();
    long cur_fl = ticks_fl;
    long cur_fr = ticks_fr;
    long cur_bl = ticks_bl;
    long cur_br = ticks_br;
    interrupts();

    // คำนวณความเร็ว (พัลส์ต่อวินาที ticks/sec)
    speed_fl = (float)(cur_fl - last_ticks_fl) / dt;
    speed_fr = (float)(cur_fr - last_ticks_fr) / dt;
    speed_bl = (float)(cur_bl - last_ticks_bl) / dt;
    speed_br = (float)(cur_br - last_ticks_br) / dt;

    last_ticks_fl = cur_fl;
    last_ticks_fr = cur_fr;
    last_ticks_bl = cur_bl;
    last_ticks_br = cur_br;
}
