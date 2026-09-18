#include "encoder.h"
#include "robot_config.h"

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

void isr_enc_fl() { ticks_fl += digitalRead(ENC_FL_B) == HIGH ? ENC_FL_DIR : -ENC_FL_DIR; }
void isr_enc_fr() { ticks_fr += digitalRead(ENC_FR_B) == HIGH ? ENC_FR_DIR : -ENC_FR_DIR; }
void isr_enc_bl() { ticks_bl += digitalRead(ENC_BL_B) == HIGH ? ENC_BL_DIR : -ENC_BL_DIR; }
void isr_enc_br() { ticks_br += digitalRead(ENC_BR_B) == HIGH ? ENC_BR_DIR : -ENC_BR_DIR; }

void encoder_init()
{
    pinMode(ENC_FL_A, INPUT_PULLUP);
    pinMode(ENC_FL_B, INPUT_PULLUP);
    pinMode(ENC_FR_A, INPUT_PULLUP);
    pinMode(ENC_FR_B, INPUT_PULLUP);
    pinMode(ENC_BL_A, INPUT_PULLUP);
    pinMode(ENC_BL_B, INPUT_PULLUP);
    pinMode(ENC_BR_A, INPUT_PULLUP);
    pinMode(ENC_BR_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENC_FL_A), isr_enc_fl, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_FR_A), isr_enc_fr, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_BL_A), isr_enc_bl, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_BR_A), isr_enc_br, RISING);
}

void encoder_reset()
{
    noInterrupts();
    ticks_fl = ticks_fr = ticks_bl = ticks_br = 0;
    interrupts();
    last_ticks_fl = last_ticks_fr = last_ticks_bl = last_ticks_br = 0;
    speed_fl = speed_fr = speed_bl = speed_br = 0.0f;
}

long encoder_get_ticks_fl() { noInterrupts(); long value = ticks_fl; interrupts(); return value; }
long encoder_get_ticks_fr() { noInterrupts(); long value = ticks_fr; interrupts(); return value; }
long encoder_get_ticks_bl() { noInterrupts(); long value = ticks_bl; interrupts(); return value; }
long encoder_get_ticks_br() { noInterrupts(); long value = ticks_br; interrupts(); return value; }

float encoder_get_speed_fl() { return speed_fl; }
float encoder_get_speed_fr() { return speed_fr; }
float encoder_get_speed_bl() { return speed_bl; }
float encoder_get_speed_br() { return speed_br; }

void encoder_update_speeds(float dt)
{
    if (dt <= 0.0f) return;

    noInterrupts();
    const long cur_fl = ticks_fl;
    const long cur_fr = ticks_fr;
    const long cur_bl = ticks_bl;
    const long cur_br = ticks_br;
    interrupts();

    speed_fl = static_cast<float>(cur_fl - last_ticks_fl) / dt;
    speed_fr = static_cast<float>(cur_fr - last_ticks_fr) / dt;
    speed_bl = static_cast<float>(cur_bl - last_ticks_bl) / dt;
    speed_br = static_cast<float>(cur_br - last_ticks_br) / dt;
    last_ticks_fl = cur_fl;
    last_ticks_fr = cur_fr;
    last_ticks_bl = cur_bl;
    last_ticks_br = cur_br;
}
