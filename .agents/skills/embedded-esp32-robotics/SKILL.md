---
name: embedded-esp32-robotics
description: Comprehensive guide and expert skill for ESP32 microcontroller firmware development, differential drive robotics, PID motor control, wheel encoder interrupts, multi-sensor acquisition (IR, TF-Luna LiDAR), NeoPixel LED indicators, and FreeRTOS task scheduling.
---

# ESP32 Robotics & Embedded Firmware Skill

This skill provides design patterns, mathematical models, and production-ready C++/Arduino/PlatformIO code for developing the real-time motion controller firmware on the **ESP32 NodeMCU** for the Rescue Robot platform.

---

## 1. System Architecture & Pin Mapping

### Pinout Guidelines (ESP32 NodeMCU)
- **Motor Left (TB6612FNG/L298N):**
  - `PWM_LEFT` (GPIO 18) - LEDC PWM channel 0 (20 kHz, 8-bit resolution)
  - `DIR_LEFT_1` (GPIO 19), `DIR_LEFT_2` (GPIO 21)
- **Motor Right:**
  - `PWM_RIGHT` (GPIO 22) - LEDC PWM channel 1 (20 kHz, 8-bit resolution)
  - `DIR_RIGHT_1` (GPIO 23), `DIR_RIGHT_2` (GPIO 25)
- **Wheel Encoders (Quadrature):**
  - Left Encoder: `ENC_L_A` (GPIO 34 - Input Only), `ENC_L_B` (GPIO 35 - Input Only)
  - Right Encoder: `ENC_R_A` (GPIO 32), `ENC_R_B` (GPIO 33)
- **Perception & Sensors:**
  - `IR_FRONT_L` (GPIO 26), `IR_FRONT_R` (GPIO 27), `IR_SIDE_L` (GPIO 14), `IR_SIDE_R` (GPIO 12)
  - TF-Luna LiDAR: Serial2 `RX2` (GPIO 16), `TX2` (GPIO 17) at 115200 baud
  - LiDAR Sweep Servo: `SERVO_PIN` (GPIO 13) - 50 Hz PWM
- **Indicators & Sound:**
  - NeoPixel WS2812B: `NEOPIXEL_PIN` (GPIO 4)
  - Active Buzzer: `BUZZER_PIN` (GPIO 5)

---

## 2. Encoder Interrupts & Differential Drive Odometry

### Interrupt Service Routine (ISR)
Place ISR handlers in `IRAM_ATTR` for minimal jitter and fast execution:

```cpp
#include <Arduino.h>

volatile int64_t encoder_left_ticks = 0;
volatile int64_t encoder_right_ticks = 0;

void IRAM_ATTR isr_encoder_left() {
    int b = digitalRead(35); // ENC_L_B
    if (b > 0) {
        encoder_left_ticks++;
    } else {
        encoder_left_ticks--;
    }
}

void IRAM_ATTR isr_encoder_right() {
    int b = digitalRead(33); // ENC_R_B
    if (b > 0) {
        encoder_right_ticks++;
    } else {
        encoder_right_ticks--;
    }
}

void setup_encoders() {
    pinMode(34, INPUT_PULLUP); // ENC_L_A
    pinMode(35, INPUT_PULLUP); // ENC_L_B
    pinMode(32, INPUT_PULLUP); // ENC_R_A
    pinMode(33, INPUT_PULLUP); // ENC_R_B

    attachInterrupt(digitalPinToInterrupt(34), isr_encoder_left, RISING);
    attachInterrupt(digitalPinToInterrupt(32), isr_encoder_right, RISING);
}
```

### Wheel Odometry Calculations
Constants for differential drive robot:
- `TICKS_PER_REV`: Number of encoder ticks per wheel revolution (e.g. 360 or 960)
- `WHEEL_DIAMETER_MM`: Diameter of the wheel (e.g. 65.0 mm)
- `WHEEL_CIRCUMFERENCE_MM = PI * WHEEL_DIAMETER_MM`
- `TICKS_PER_MM = TICKS_PER_REV / WHEEL_CIRCUMFERENCE_MM`
- `TRACK_WIDTH_MM`: Distance between left and right wheels (e.g. 140.0 mm)

```cpp
float ticks_to_mm(int64_t ticks) {
    return (float)ticks / TICKS_PER_MM;
}

int64_t mm_to_ticks(float mm) {
    return (int64_t)(mm * TICKS_PER_MM);
}

// Target ticks to rotate robot by specified degrees in-place
int64_t deg_to_ticks(float deg) {
    float arc_length_mm = (PI * TRACK_WIDTH_MM) * (deg / 360.0f);
    return mm_to_ticks(arc_length_mm);
}
```

---

## 3. Closed-Loop PID Speed & Position Controller

```cpp
struct PIDController {
    float kp = 2.0f;
    float ki = 0.05f;
    float kd = 0.15f;
    float integral = 0.0f;
    float last_error = 0.0f;
    float max_output = 255.0f;
    float min_output = -255.0f;

    float compute(float target, float current, float dt) {
        float error = target - current;
        integral += error * dt;
        
        // Anti-windup
        if (integral > 100.0f) integral = 100.0f;
        if (integral < -100.0f) integral = -100.0f;

        float derivative = (error - last_error) / dt;
        last_error = error;

        float output = (kp * error) + (ki * integral) + (kd * derivative);
        return constrain(output, min_output, max_output);
    }

    void reset() {
        integral = 0;
        last_error = 0;
    }
};
```

---

## 4. Multi-Sensor Perception (TF-Luna LiDAR & IR Array)

### TF-Luna LiDAR Reader over UART
TF-Luna sends 9-byte binary frames: `[0x59, 0x59, Dist_L, Dist_H, Strength_L, Strength_H, Temp_L, Temp_H, Checksum]`

```cpp
struct LunaData {
    uint16_t distance_cm;
    uint16_t strength;
    float temperature_c;
    bool valid;
};

LunaData read_tfluna(HardwareSerial &serial) {
    LunaData data = {0, 0, 0.0f, false};
    while (serial.available() >= 9) {
        if (serial.read() == 0x59) {
            if (serial.read() == 0x59) {
                uint8_t buffer[7];
                serial.readBytes(buffer, 7);
                
                uint8_t checksum = 0x59 + 0x59;
                for (int i = 0; i < 6; i++) checksum += buffer[i];

                if (checksum == buffer[6]) {
                    data.distance_cm = buffer[0] | (buffer[1] << 8);
                    data.strength = buffer[2] | (buffer[3] << 8);
                    data.temperature_c = (float)(buffer[4] | (buffer[5] << 8)) / 8.0f - 256.0f;
                    data.valid = true;
                    return data;
                }
            }
        }
    }
    return data;
}
```

---

## 5. FreeRTOS Multitasking Architecture

Split tasks on ESP32 dual cores to ensure motor control loop never starves:

```cpp
TaskHandle_t TaskMotorLoop;
TaskHandle_t TaskTelemetry;

void task_motor_control(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100 Hz Control Loop

    for (;;) {
        // Read encoders & update PID
        update_motion_pid();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void task_telemetry_comm(void *pvParameters) {
    for (;;) {
        parse_incoming_serial_commands();
        send_telemetry_packet();
        vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz Telemetry Loop
    }
}

void setup() {
    // Core 1 for High-Priority Real-time Motion PID
    xTaskCreatePinnedToCore(task_motor_control, "MotorTask", 4096, NULL, 3, &TaskMotorLoop, 1);
    
    // Core 0 for Communications & Sensor Polling
    xTaskCreatePinnedToCore(task_telemetry_comm, "TelemTask", 4096, NULL, 1, &TaskTelemetry, 0);
}
```

---

## 6. Safety Watchdog & Emergency Stop (E-STOP)

Always implement a safety timeout: if no heartbeat or movement command is received from Raspberry Pi within **500 ms**, stop all motors immediately.

```cpp
unsigned long last_command_time = 0;

void feed_watchdog() {
    last_command_time = millis();
}

void check_safety_watchdog() {
    if (millis() - last_command_time > 500) {
        // Timeout! Force full stop
        emergency_stop();
    }
}
```
