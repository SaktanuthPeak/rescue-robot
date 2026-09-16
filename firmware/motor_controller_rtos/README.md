# Mecanum Motor Controller RTOS (Direct PWM)

Firmware ชุดนี้เป็น RTOS sketch แยกจาก `firmware/motor_controller/` สำหรับ
Arduino Mega 2560 โดยไม่มี Closed-Loop PID และไม่มี TM1638

## หลักการทำงาน

รับคำสั่งการเคลื่อนที่จาก CAN แล้วส่ง PWM คงที่ตามรูปแบบ Mecanum
ไปยังมอเตอร์ทั้ง 4 ล้อทันที โดยใช้ `MOTOR_BASE_PWM` เป็นค่าหลัก
Encoder ยังถูกอ่านเพื่อส่ง Telemetry เท่านั้น ไม่ได้นำกลับมาปรับ PWM

## การตั้งค่า PWM

แก้ค่าได้ที่ `robot_config.h`:

```cpp
constexpr uint8_t MOTOR_BASE_PWM = 180;
```

## Task ที่ใช้งาน

- `CAN_RX` priority 2: รับคำสั่ง CAN
- `MOTOR_CTL` priority 3: อัปเดต Encoder และตรวจ watchdog
- `TELEMETRY` priority 1: ส่งข้อมูล Encoder ทุก 100 ms

ถ้าไม่มีคำสั่ง CAN เกิน `CAN_TIMEOUT` มอเตอร์จะหยุดเพื่อความปลอดภัย
