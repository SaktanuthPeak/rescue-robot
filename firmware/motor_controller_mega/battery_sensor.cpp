#include "battery_sensor.h"
#include "robot_config.h"

void battery_init()
{
    pinMode(BATTERY_VOLTAGE_PIN, INPUT);
}

BatterySample battery_read()
{
    const uint16_t adc = static_cast<uint16_t>(analogRead(BATTERY_VOLTAGE_PIN));
    const float moduleVoltage =
        (static_cast<float>(adc) * ADC_REFERENCE_VOLTAGE) / ADC_COUNTS;
    const float batteryVoltage = moduleVoltage *
        ((BATTERY_R1_OHMS + BATTERY_R2_OHMS) / BATTERY_R2_OHMS);

    return BatterySample{
        adc,
        batteryVoltage > 0.0f
            ? static_cast<uint32_t>(batteryVoltage * 1000.0f + 0.5f)
            : 0,
    };
}
