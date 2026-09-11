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

#endif
