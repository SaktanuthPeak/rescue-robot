#ifndef TM1638_H
#define TM1638_H

#include <Arduino.h>

void tm1638_begin();
uint8_t tm1638_read_buttons();
void tm1638_set_led(uint8_t value, uint8_t position);
void tm1638_display_pid(char parameter, float value, uint8_t stepIndex, bool dirty);

#endif
