#include "motor.h"
#include "PS2X_lib.h"
#include "PS2_Controller.h"
#include "ultrasonic_sensor.h"
#include "servo_controller.h"

uint8_t ps2_data[6];
PS2_Status status_left = STOP;
PS2_Status status_right = STOP;
PS2_Status gripper_status = Release;

// PS2X library instance
PS2X ps2x;

// deadzone for analog stick
const uint8_t PS2_DEADZONE = 12;

const unsigned long PS2_POLL_INTERVAL_MS = 15;
const unsigned long DEBUG_PRINT_INTERVAL_MS = 250;

unsigned long last_ps2_poll_ms = 0;
unsigned long last_debug_print_ms = 0;

PS2_Status get_status_from_sticks(int x, int y, PS2_Status center_value)
{
    // map to PS2_Status
    if (x == 0 && y == 1) return BACKWARD;
    else if (x == 0 && y == -1) return FORWARD;
    else if (x == -1 && y == 0) return LEFT;
    else if (x == 1 && y == 0) return RIGHT;
    else if (x == -1 && y == 1) return BACKWARD_LEFT;
    else if (x == 1 && y == 1) return BACKWARD_RIGHT;
    else if (x == -1 && y == -1) return FORWARD_LEFT;
    else if (x == 1 && y == -1) return FORWARD_RIGHT;
    else return center_value;
}


void apply_motor_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD:
        motor_forward();
        break;
    case BACKWARD:
        motor_backward();
        break;
    case LEFT:
        motor_slide_left();
        break;
    case RIGHT:
        motor_slide_right();
        break;
    case FORWARD_LEFT:
        motor_forward_left();
        break;
    case FORWARD_RIGHT:
        motor_forward_right();
        break;
    case BACKWARD_LEFT:
        motor_backward_left();
        break;
    case BACKWARD_RIGHT:
        motor_backward_right();
        break;
    default:
        motor_stop();
        break;
    }
}

void apply_arm_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD:
        arm_forward();
        break;
    case BACKWARD:
        arm_backward();
        break;
    case LEFT:
        arm_turn_left();
        break;
    case RIGHT:
        arm_turn_right();
        break;
    case FORWARD_LEFT:
        arm_turn_left();
        break;
    case FORWARD_RIGHT:
        arm_turn_right();
        break;
    case BACKWARD_LEFT:
        arm_turn_left();
        break;
    case BACKWARD_RIGHT:
        arm_turn_right();
        break;
    case Release:
        gripper_release();
        break;
    case Clamp:
        gripper_clamp();
        break;
    default:
        arm_stop();
        break;
    }
}



void setup()
{
    Serial.begin(115200);

    motor_init();
    ps2x.config_gamepad(PS2_CLK_PIN, PS2_CMD_PIN, PS2_ATT_PIN, PS2_DAT_PIN, true, true);
    ultrasonic_init();
    // servo_init();
}

void loop()
{
    // ultrasonic_update();
    // apply_motor_from_status(LEFT);
    // delay(2000);
    // apply_motor_from_status(BACKWARD_RIGHT);
    // delay(2000);

    unsigned long now = millis();
    void arm_turn_left();
    if (now - last_ps2_poll_ms >= PS2_POLL_INTERVAL_MS)
    {
        last_ps2_poll_ms = now;
        ps2x.read_gamepad();

        int x = 0;
        int y = 0;

        // 1. เช็คปุ่ม D-PAD ก่อนเป็นอันดับแรก (ให้ความสำคัญสูงสุด)
        if (ps2x.Button(PSB_PAD_UP)) { y = -1; }
        else if (ps2x.Button(PSB_PAD_DOWN)) { y = 1; }

        if (ps2x.Button(PSB_PAD_LEFT)) { x = -1; }
        else if (ps2x.Button(PSB_PAD_RIGHT)) { x = 1; }

        // 2. ถ้าไม่ได้กด D-PAD เลย ค่อยไปเช็คก้านโยก Analog
        if (x == 0 && y == 0) 
        {
            uint8_t analog_x = ps2x.Analog(PSS_LX);
            uint8_t analog_y = ps2x.Analog(PSS_LY);
            
            // ป้องกันค่า 255 มารบกวน (กรณีจอยไม่ได้เปิดไฟแดง Analog)
            if (analog_x != 255 && analog_y != 255) 
            {
                if (analog_x < (128 - PS2_DEADZONE)) x = -1;
                else if (analog_x > (128 + PS2_DEADZONE)) x = 1;

                if (analog_y < (128 - PS2_DEADZONE)) y = -1;
                else if (analog_y > (128 + PS2_DEADZONE)) y = 1;
            }
        }
        
        // if (ps2x.Analog(PSS_RX) == 255 && ps2x.Analog(PSS_RY) == 255 && ps2x.Analog(PSS_LX) == 255 && ps2x.Analog(PSS_LY) == 255)
        // {
        //     x = 0;
        //     y = 0;
        //     x_right = 0;
        //     y_right = 0;
        // }
        status_left = get_status_from_sticks(x, y, STOP);
        apply_motor_from_status(status_left);

        status_right = get_status_from_sticks(x, y, STOP);
        apply_arm_from_status(status_right);

        if (ps2x.Button(PSB_SQUARE))
        {
            gripper_status = Clamp;
        }
        else if (ps2x.Button(PSB_CIRCLE))
        {
            gripper_status = Release;
        }
        apply_arm_from_status(gripper_status);
        
    }

    if (now - last_debug_print_ms >= DEBUG_PRINT_INTERVAL_MS)
    {
        last_debug_print_ms = now;
        print_debug(status_left, status_right, gripper_status);
    }
    
}