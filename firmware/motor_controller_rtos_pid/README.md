# Mecanum Motor Controller with TM1638 PID Tuning

Firmware ชุดนี้เป็น RTOS sketch แยกจาก `firmware/motor_controller/` สำหรับ
Arduino Mega 2560 โดยใช้ TM1638 แบบ bit-bang ผ่าน `shiftOut()`/`shiftIn()`
และไม่ต้องใช้ TM1638 library ภายนอก

## การต่อสาย TM1638

| TM1638 | Arduino Mega 2560 |
|---|---:|
| VCC | 5V |
| GND | GND |
| STB / STROBE | D42 |
| CLK / CLOCK | D43 |
| DIO / DATA | D44 |

ขา D42-D44 ถูกเลือกเพราะไม่ชนกับ Encoder, PWM มอเตอร์ หรือ SPI ของ MCP2515

## ปุ่มตั้งค่า PID

| ปุ่ม | การทำงาน |
|---|---|
| S1 | เลือก Kp |
| S2 | เลือก Ki |
| S3 | เลือก Kd |
| S4 | ลดค่า |
| S5 | เพิ่มค่า |
| S6 | เปลี่ยน step: 0.001 / 0.01 / 0.1 |
| S7 | คืนค่า default |
| S8 | บันทึกลง EEPROM |

กดค้าง S4 หรือ S5 เพื่อปรับค่าต่อเนื่อง ค่าใหม่จะถูกส่งเข้า Motor Control
ผ่าน FreeRTOS queue และการบันทึก EEPROM จะเกิดเฉพาะเมื่อกด S8

ก่อนจูน PID ให้ยกล้อพ้นพื้นหรือหยุดหุ่นยนต์เพื่อความปลอดภัย
