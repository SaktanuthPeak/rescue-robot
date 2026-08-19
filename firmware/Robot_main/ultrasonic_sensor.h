#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <Arduino.h>

void ultrasonic_init();
void ultrasonic_update();
int ultrasonic_read_distance_cm();

#endif