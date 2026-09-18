# Mega 2560 Motor Controller / Raspberry Pi Gateway

สเก็ตช์ชุดใหม่สำหรับ Arduino Mega 2560 แยกจาก `firmware/motor_controller/`
เดิม โดย Mega เป็นเจ้าของมอเตอร์, encoder, battery module และ IR distance sensors
แล้วคุยกับ Raspberry Pi ผ่าน USB serial

```text
Raspberry Pi / FastAPI ── USB Serial ── Mega 2560
                                      ├─ 4-wheel motor + PID
                                      ├─ 4 encoders
                                      ├─ battery module
                                      ├─ IR distance sensors
                                      └─ CAN bus ไป remote / can_receiver
```

## Upload

เปิดโฟลเดอร์นี้เป็น Arduino sketch และเลือกบอร์ด `Arduino Mega or Mega 2560`.
ต้องติดตั้งไลบรารี `mcp_can` ก่อน compile

ไฟล์ทั้งหมดในโฟลเดอร์ต้องอยู่ด้วยกัน:

```text
motor_controller_mega.ino
robot_config.h
motor.h / motor.cpp
encoder.h / encoder.cpp
battery_sensor.h / battery_sensor.cpp
ir_sensors.h / ir_sensors.cpp
```

## Pin map

| อุปกรณ์ | Mega 2560 |
| --- | ---: |
| Battery module `AO` | `A0` |
| IR หน้า `AO` | `A1` |
| IR ขวา `AO` | `A2` |
| IR หลัง `AO` | `A3` |
| IR ซ้าย `AO` | `A4` |
| MCP2515 `CS` | `D10` |
| MCP2515 `MISO/MOSI/SCK` | `D50/D51/D52` |
| Mega hardware `SS` | `D53` ต้องตั้งเป็น OUTPUT/HIGH |
| TF/serial sensor | ยังไม่ใช้ |

พินมอเตอร์และ encoder ใช้ชุดเดียวกับ `robot_config.h` ในโฟลเดอร์นี้
และไม่ชนกับ `A0..A4`

ถ้า IR module มีเฉพาะ `DO` แบบ digital ให้เปลี่ยน mapping เป็นพินดิจิทัลที่ว่าง
เช่น `D34..D37` และแก้ `ir_sensors.cpp`

## USB serial protocol

รับคำสั่งจาก FastAPI ที่ `115200 baud` โดยจบคำสั่งด้วย newline:

```text
CMD:MOTOR:0..10
CMD:ARM:0..8,11..14
CMD:ALL:0
STOP
PING
```

`CMD:MOTOR` ควบคุมมอเตอร์บน Mega โดยตรง ส่วน `CMD:ARM` จะถูกส่งต่อผ่าน CAN ID
`0x101` ไปยัง arm receiver

Mega ส่ง telemetry ทุก `100 ms`:

```text
MC1,motor_code,motor_alive,arm_code,arm_alive,battery_mV,battery_adc,ir_front,ir_right,ir_rear,ir_left,enc_fl,enc_fr,enc_bl,enc_br,speed_fl,speed_fr,speed_bl,speed_br,seq*CK
```

ทุก frame มี XOR checksum และคำสั่ง serial จะหมดอายุภายใน `1000 ms` หากไม่มีคำสั่งใหม่

## CAN IDs

| ID | ความหมาย |
| --- | --- |
| `0x100` | motor command จาก remote |
| `0x101` | arm command ไปยัง arm receiver |
| `0x102` | motor/encoder telemetry จาก Mega |

เซนเซอร์ที่ต่ออยู่บน Mega ไม่จำเป็นต้องส่ง raw data ผ่าน CAN; Mega รวมค่าไว้ใน `MC1`
ส่งให้ FastAPI โดยตรง ส่วนข้อมูลจากบอร์ดอื่นให้ส่งเข้ามาทาง CAN แล้ว Mega จึงรวมใน telemetry
ตามต้องการ
