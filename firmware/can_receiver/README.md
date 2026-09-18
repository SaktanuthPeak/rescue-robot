# CAN Receiver / FastAPI Protocol Bridge

เฟิร์มแวร์นี้ใช้กับ Arduino UNO ทำหน้าที่รับคำสั่งจาก remote ผ่าน CAN, ควบคุม
PCA9685/ปั๊ม และเปิด USB serial protocol ให้ FastAPI ควบคุมหรืออ่านสถานะได้

```text
remote CAN sender ─┐
                   ├─ CAN receiver UNO ── USB Serial ── Raspberry Pi / FastAPI
FastAPI command ───┘          ├─ PCA9685 arm
                              ├─ relay/pump
                              └─ arm/pump telemetry
```

ไม่มีโค้ดอ่าน battery module, IR รอบคัน, DHT11 หรือ OLED ใน sketch นี้แล้ว
การอ่าน battery module และ IR อยู่ใน `firmware/motor_controller_mega/` และส่งเข้า
FastAPI ผ่าน protocol `MC1`

## Protocol

ตั้ง USB serial ที่ `115200 baud` และส่งคำสั่งจบด้วย newline:

| คำสั่ง | ความหมาย |
| --- | --- |
| `CMD:MOTOR:0..8` | ส่งสถานะมอเตอร์ต่อเข้า CAN ID `0x100` |
| `CMD:ARM:0..8,11..14` | ควบคุมแขน/ปั๊มผ่าน PCA9685 และ relay |
| `CMD:ALL:0` หรือ `STOP` | หยุดมอเตอร์ แขน และปั๊ม |
| `PING` | ตรวจลิงก์ ได้คำตอบ `PONG` |

คำสั่งจาก FastAPI จะ override คำสั่ง CAN ชั่วคราวและหมดอายุภายใน `1000 ms`
หากไม่มีคำสั่งซ้ำ เพื่อให้มอเตอร์/แขนหยุดเมื่อ USB link หาย

ทุก `100 ms` บอร์ดส่ง telemetry รูปแบบ `RB4` พร้อม XOR checksum:

```text
RB4,motor_code,motor_alive,arm_code,arm_alive,battery_mV,battery_adc,axis1_pwm,axis2_pwm,axis3_pwm,pump_on,seq*CK
```

บอร์ดนี้ยังส่ง field `battery_mV` และ `battery_adc` เป็น `0` เพื่อคงรูปแบบ `RB4`
สำหรับระบบเก่าเท่านั้น ค่า battery จริงมาจาก `MC1` ของ Mega

ตัวอย่าง:

```text
RB4,-1,0,13,1,0,0,335,303,305,0,42*CK
```

รูปแบบ `RB4` ตรงกับ parser ใน `backend/apiapp/infrastructure/receiver_canbus.py`
และรองรับโดย `docker-compose.robot-serial.yml`

## Pin map: Arduino UNO

| UNO pin | อุปกรณ์ | หมายเหตุ |
| --- | --- | --- |
| `A0..A3` | ไม่ได้ใช้งาน | battery/IR ย้ายไป Arduino Mega |
| `A4/SDA` | PCA9685 `SDA` | สงวนไว้ ห้ามต่อ battery module |
| `A5/SCL` | PCA9685 `SCL` | สงวนไว้ |
| `D4` | relay/pump control | active LOW |
| `D10` | MCP2515 `CS` |  |
| `D11` | MCP2515 `MOSI/SI` | SPI |
| `D12` | MCP2515 `MISO/SO` | SPI |
| `D13` | MCP2515 `SCK` | SPI |
| `D0/D1` | USB serial ไป Raspberry Pi | ห้ามต่อ sensor อื่น |

MCP2515 ใช้ crystal `8 MHz`, CAN bitrate `500 kbps` และ firmware ใช้ polling จึงไม่ต้องต่อ
ขา `INT`

## PCA9685 arm channels

| Channel | หน้าที่ | ค่าเริ่มต้น |
| --- | --- | ---: |
| `CH0` | ฐานซ้าย/ขวา | `335` |
| `CH1` | แขนเดินหน้า/ถอยหลัง | `305` |
| `CH2` | หัวขึ้น/ลง | `305` |

ต้องติดตั้งไลบรารี Arduino `mcp_can` และวาง `PCA9685_Control.cpp/.h` ไว้ในโฟลเดอร์
เดียวกับ sketch ก่อน compile
