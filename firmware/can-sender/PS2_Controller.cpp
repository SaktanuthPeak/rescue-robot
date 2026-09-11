#include "PS2_Controller.h"

// ประกาศอินสแตนซ์ของ PS2X
PS2X ps2x;

static PS2_Status motorStatus = STOP;
static PS2_Status armStatus = STOP;

bool PS2_Init()
{
    // กำหนดขาสำหรับจอย PS2: CLK, CMD, ATT, DAT
    byte error = ps2x.config_gamepad(PS2_CLK_PIN, PS2_CMD_PIN, PS2_ATT_PIN, PS2_DAT_PIN, true, true);

    if (error == 0)
    {
        Serial.println("PS2 Controller: Connected and configured successfully");
        byte type = ps2x.readType();
        switch (type)
        {
        case 0: Serial.println("  Controller Type: Unknown"); break;
        case 1: Serial.println("  Controller Type: DualShock"); break;
        case 2: Serial.println("  Controller Type: GuitarHero"); break;
        case 3: Serial.println("  Controller Type: Wireless DualShock"); break;
        default: break;
        }
        return true;
    }
    else
    {
        Serial.print("PS2 Controller: Connection FAILED! Error code: ");
        Serial.println(error);
        if (error == 1) Serial.println("  -> No controller found! Check wiring (DAT, CMD, ATT, CLK) and power.");
        if (error == 2) Serial.println("  -> Controller found but refusing commands. Turn on red ANALOG light.");
        if (error == 3) Serial.println("  -> Controller refusing Pressure mode.");
        return false;
    }
}

PS2_Status get_status_from_sticks(int x, int y, PS2_Status center_value)
{
    if (x == 0 && y == -1)       return FORWARD;
    else if (x == 0 && y == 1)   return BACKWARD;
    else if (x == -1 && y == 0)  return LEFT;
    else if (x == 1 && y == 0)   return RIGHT;
    else if (x == -1 && y == -1) return FORWARD_LEFT;
    else if (x == 1 && y == -1)  return FORWARD_RIGHT;
    else if (x == -1 && y == 1)  return BACKWARD_LEFT;
    else if (x == 1 && y == 1)   return BACKWARD_RIGHT;
    else                         return center_value;
}

void PS2_Update()
{
    ps2x.read_gamepad();

    // ==============================================================
    // 1. ตรวจสอบการควบคุมล้อ (MOTOR CONTROL)
    // ==============================================================
    int x_motor = 0;
    int y_motor = 0;

    // เช็คปุ่ม D-PAD ก่อน (ลำดับความสำคัญสูงสุดสำหรับการเดินหน้า/ถอย/สไลด์ตรง)
    if (ps2x.Button(PSB_PAD_UP))         y_motor = -1;
    else if (ps2x.Button(PSB_PAD_DOWN))  y_motor = 1;

    if (ps2x.Button(PSB_PAD_LEFT))       x_motor = -1;
    else if (ps2x.Button(PSB_PAD_RIGHT)) x_motor = 1;

    // ถ้าไม่มีการกด D-PAD ให้ตรวจสอบก้านโยกอนาล็อกซ้าย (Left Stick: LX, LY)
    if (x_motor == 0 && y_motor == 0)
    {
        uint8_t analog_lx = ps2x.Analog(PSS_LX);
        uint8_t analog_ly = ps2x.Analog(PSS_LY);

        // ป้องกันค่ารบกวน 255 เมื่อสัญญาณจอยหลุด
        if (analog_lx != 255 && analog_ly != 255)
        {
            if (analog_lx < (128 - PS2_DEADZONE))      x_motor = -1;
            else if (analog_lx > (128 + PS2_DEADZONE)) x_motor = 1;

            if (analog_ly < (128 - PS2_DEADZONE))      y_motor = -1;
            else if (analog_ly > (128 + PS2_DEADZONE)) y_motor = 1;
        }
    }

    // แปลงแกน x, y เป็นทิศทางการขับเคลื่อนล้อ Mecanum
    motorStatus = get_status_from_sticks(x_motor, y_motor, STOP);

    // ปุ่มหมุนตัวอยู่กับที่ (Spin In-Place): L1 = หมุนซ้าย, R1 = หมุนขวา
    if (ps2x.Button(PSB_L1))
    {
        motorStatus = SPIN_LEFT;
    }
    else if (ps2x.Button(PSB_R1))
    {
        motorStatus = SPIN_RIGHT;
    }

    // ==============================================================
    // 2. ตรวจสอบการควบคุมแขนกลและกริปเปอร์ (ARM & GRIPPER CONTROL)
    // ==============================================================
    int x_arm = 0;
    int y_arm = 0;

    uint8_t analog_rx = ps2x.Analog(PSS_RX);
    uint8_t analog_ry = ps2x.Analog(PSS_RY);

    if (analog_rx != 255 && analog_ry != 255)
    {
        if (analog_rx < (128 - PS2_DEADZONE))      x_arm = -1;
        else if (analog_rx > (128 + PS2_DEADZONE)) x_arm = 1;

        if (analog_ry < (128 - PS2_DEADZONE))      y_arm = -1;
        else if (analog_ry > (128 + PS2_DEADZONE)) y_arm = 1;
    }

    armStatus = get_status_from_sticks(x_arm, y_arm, STOP);

    // ปุ่มกดคุมกริปเปอร์และแขน:
    // Square = หนีบ, Circle = ปล่อย, Triangle = ยกขึ้น, Cross = วางลง
    if (ps2x.Button(PSB_SQUARE))
    {
        armStatus = Clamp;
    }
    else if (ps2x.Button(PSB_CIRCLE))
    {
        armStatus = Release;
    }
    else if (ps2x.Button(PSB_TRIANGLE))
    {
        armStatus = FORWARD;
    }
    else if (ps2x.Button(PSB_CROSS))
    {
        armStatus = BACKWARD;
    }
}

PS2_Status PS2_GetMotorStatus()
{
    return motorStatus;
}

PS2_Status PS2_GetArmStatus()
{
    return armStatus;
}

void print_debug(PS2_Status status_motor, PS2_Status status_arm)
{
    Serial.print("PS2 | LX:"); Serial.print(ps2x.Analog(PSS_LX));
    Serial.print(" LY:");     Serial.print(ps2x.Analog(PSS_LY));
    Serial.print(" RX:");     Serial.print(ps2x.Analog(PSS_RX));
    Serial.print(" RY:");     Serial.print(ps2x.Analog(PSS_RY));

    Serial.print(" | MOTOR: ");
    switch (status_motor)
    {
    case FORWARD:        Serial.print("FORWARD"); break;
    case BACKWARD:       Serial.print("BACKWARD"); break;
    case LEFT:           Serial.print("LEFT"); break;
    case RIGHT:          Serial.print("RIGHT"); break;
    case FORWARD_LEFT:   Serial.print("FORWARD_LEFT"); break;
    case FORWARD_RIGHT:  Serial.print("FORWARD_RIGHT"); break;
    case BACKWARD_LEFT:  Serial.print("BACKWARD_LEFT"); break;
    case BACKWARD_RIGHT: Serial.print("BACKWARD_RIGHT"); break;
    case SPIN_LEFT:      Serial.print("SPIN_LEFT"); break;
    case SPIN_RIGHT:     Serial.print("SPIN_RIGHT"); break;
    default:             Serial.print("STOP"); break;
    }

    Serial.print(" | ARM: ");
    switch (status_arm)
    {
    case FORWARD:  Serial.print("UP"); break;
    case BACKWARD: Serial.print("DOWN"); break;
    case LEFT:     Serial.print("TURN_LEFT"); break;
    case RIGHT:    Serial.print("TURN_RIGHT"); break;
    case Clamp:    Serial.print("CLAMP"); break;
    case Release:  Serial.print("RELEASE"); break;
    default:       Serial.print("STOP"); break;
    }

    Serial.println();
}
