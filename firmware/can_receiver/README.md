# CAN Receiver

โฟลเดอร์นี้ใช้เป็น CAN receiver สำหรับอ่านสถานะจาก CAN sender และอ่าน analog IR
sensor 4 ช่อง พร้อมส่งข้อมูลออก Raspberry Pi ผ่าน USB Serial port เดิม

ต้องติดตั้งไลบรารี Arduino `DHT sensor library` และ `U8g2` ก่อน compile

- รับ `CAN_ID_MOTOR` (`0x100`) และ `CAN_ID_ARM` (`0x101`)
- ตรวจสอบ status และ timeout 300 ms
- อ่านแรงดันแบตเตอรี่จาก `A0`
- อ่าน IR จาก `A1` ถึง `A4`
- อ่านความชื้นจาก DHT11 ที่ `D8`
- พิมพ์ค่า humidity และ temperature จาก DHT11 ทาง Serial ทุก 2 วินาที
- ส่งข้อมูลไป Raspberry Pi ผ่าน USB Serial ในรูปแบบ `RB3`
- OLED แสดงเฉพาะไอคอนแบตเตอรี่และแรงดัน
- ไม่มีการสั่ง motor, servo, PCA9685 หรือ relay/pump

## การต่อสาย

| Arduino | ต่อกับ |
| --- | --- |
| `A0` | voltage divider output |
| `A1` | IR หน้า (front) ขา `AO` |
| `A2` | IR ขวา (right) ขา `AO` |
| `A3` | IR หลัง (rear) ขา `AO` |
| `A4` | IR ซ้าย (left) ขา `AO` |
| `D8` | DHT11 ขา `DATA` |
| `5V` | VCC ของ IR sensor ทุกตัว |
| `5V` | VCC ของ DHT11 |
| `GND` | GND ของ IR sensor ทุกตัว, DHT11 และ voltage divider |
| `D6` | OLED SDA |
| `D7` | OLED SCL |

IR ต้องต่อขา `AO` ไม่ใช่ `DO` เพราะ firmware อ่านค่า analog `0..1023`
และใช้ D0/D1 เป็น USB Serial ห้ามนำไปต่อ OLED หรือ sensor

รูปแบบ Serial:

```text
RB3,motor_code,motor_alive,arm_code,arm_alive,battery_mV,battery_adc,ir_front,ir_right,ir_rear,ir_left,humidity_percent,seq*CK
```

ถ้าใช้ DHT11 แบบตัวเซนเซอร์เปล่า ให้ใส่ตัวต้านทาน pull-up ประมาณ `10kΩ`
ระหว่าง `DATA` กับ `5V` (โมดูล DHT11 ส่วนใหญ่มีตัวนี้มาให้แล้ว)

ตัวอย่าง log:

```text
DHT11 | Humidity: 62% | Temperature: 29.0 C
```

ไฟล์ PCA9685 และ configuration เดิมของ Arm Controller Test ถูกถอดออกจากโฟลเดอร์นี้แล้ว โดย implementation แขนกลจริงยังอยู่ใน `firmware/arm_controller/`.
