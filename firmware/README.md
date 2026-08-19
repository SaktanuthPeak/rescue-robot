# FireBot Firmware

โค้ด C++ (Arduino) ที่ต้อง **upload ลงบอร์ดก่อน** จึงจะมีข้อมูลส่งเข้า backend

Backend เป็นฝ่าย *อ่าน* อย่างเดียว — มันเปิด serial port แล้วรอรับบรรทัดที่บอร์ดพิมพ์ออกมา
ถ้าไม่ upload sketch นี้ บอร์ดจะไม่ส่งอะไรเลย และ `link.state` จะค้างที่ `disconnected`

> ยังไม่มีบอร์ด / ยังไม่ได้ต่อสาย? ไม่ต้อง upload อะไรเลย — ตั้ง `TELEMETRY_SOURCE=mock`
> ใน `backend/.env` (ค่า default อยู่แล้ว) แล้ว backend จะสร้างข้อมูลจำลองให้ dashboard
> ทำงานได้เต็มรูปแบบ

---

## `flame_telemetry/` — 4-channel analog IR flame telemetry

### การต่อสาย (Wiring)

| Arduino | ต่อกับ |
| --- | --- |
| `A0` | sensor **หน้า** (front) ขา AO |
| `A1` | sensor **ขวา** (right) ขา AO |
| `A2` | sensor **หลัง** (rear) ขา AO |
| `A3` | sensor **ซ้าย** (left) ขา AO |
| `5V` / `GND` | VCC / GND ของ sensor ทุกตัว |

ใช้ขา **AO (analog out)** ไม่ใช่ `DO` — เราต้องการค่าต่อเนื่องเพื่อบอกความแรงและทิศทางของไฟ
ถ้าใช้ `DO` จะได้แค่ เจอ/ไม่เจอ ซึ่งเสียประโยชน์ของ 4 ตัวไปทั้งหมด

**ห้ามใช้ `D0` และ `D1`** — สองขานี้คือ UART ที่ telemetry วิ่งอยู่

### Upload

Arduino IDE — เปิดโฟลเดอร์ `flame_telemetry/` เลือกบอร์ดกับ port แล้วกด Upload
(ชื่อไฟล์ `.ino` ต้องตรงกับชื่อโฟลเดอร์ นี่คือข้อบังคับของ Arduino IDE)

หรือใช้ `arduino-cli`:

```bash
cd firmware/flame_telemetry
arduino-cli compile --fqbn arduino:avr:uno .
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 .
```

### ทดสอบว่าบอร์ดส่งข้อมูลจริง (ไม่ต้องเปิด backend)

```bash
cd backend
poetry run python -m serial.tools.miniterm /dev/ttyACM0 115200
```

ควรเห็นบรรทัดแบบนี้ไหลออกมาที่ 20 Hz:

```
FB1,812,118,1010,990,OK,4137*7B
FB1,809,121,1010,988,OK,4138*7E
```

ถ้าเห็นแล้ว ให้ตั้ง `TELEMETRY_SOURCE=serial` ใน `backend/.env` แล้ว restart backend

### ปัญหาที่เจอบ่อย

**`PermissionError: [Errno 13] /dev/ttyACM0`** — user ไม่ได้อยู่ในกลุ่ม `dialout`
(ตอนนี้เครื่องนี้ยังไม่อยู่):

```bash
sudo usermod -aG dialout $USER
# ต้อง log out / log in ใหม่ หรือใช้ `newgrp dialout` สำหรับ shell นี้
```

**หา port ไม่เจอ** — Arduino Uno R3 (ชิป ATmega16U2) จะขึ้นเป็น `/dev/ttyACM*`
ส่วนบอร์ด clone ที่ใช้ชิป CH340 จะขึ้นเป็น `/dev/ttyUSB*` ตรวจด้วย:

```bash
ls /dev/ttyACM* /dev/ttyUSB*
```

**ค่ากลับทาง** — sensor ส่วนใหญ่ (YG1006) เป็น *active low* คือยิ่งเจอไฟ ค่ายิ่ง **ต่ำ**
sketch นี้ส่งค่า ADC ดิบเสมอ ไม่แปลงอะไร ถ้าบอร์ดคุณกลับทาง ให้แก้
`FLAME_ACTIVE_LOW=False` ใน `backend/.env` — **ไม่ต้องแก้ sketch และไม่ต้อง flash ใหม่**

**Serial Monitor ของ Arduino IDE แย่งพอร์ต** — serial port เปิดได้ทีละโปรแกรมเท่านั้น
ปิด Serial Monitor ก่อนสั่ง backend อ่าน

### ทำไม protocol เป็นแบบนี้

รูปแบบบรรทัด (นิยามเต็มอยู่ใน `docs/firebot-spec.md`):

```
FB1,<front>,<right>,<rear>,<left>,<status>,<seq>*<CK>
```

- **`FB1`** — magic prefix บรรทัดที่ไม่เริ่มด้วยสิ่งนี้จะถูกทิ้งทั้งหมด นี่คือวิธีกรอง
  ขยะที่ bootloader พิมพ์ออกมาตอน reset (sketch จึงตั้งใจไม่พิมพ์ข้อความต้อนรับใดๆ)
- **CSV ไม่ใช่ JSON** — บน ATmega328P การสร้าง JSON ต้องพึ่ง ArduinoJson หรือ `sprintf`
  ที่เปราะบาง แบบ CSV ใช้ `snprintf` ครั้งเดียวจบ และสั้นกว่า (34 ไบต์ vs ~110)
- **`*CK` checksum** — เหตุผลสำคัญที่สุด ถ้าสัญญาณรบกวนทำให้เลขหลักเดียวเปลี่ยน
  JSON ที่ยังถูกไวยากรณ์จะกลายเป็น *ค่าที่ดูสมเหตุสมผล* สำหรับหุ่นยนต์ตรวจจับไฟ
  "ข้อมูลเสียถูกอ่านเป็นไม่มีไฟ" คือความผิดพลาดที่คุ้มค่ากับ checksum 5 ไบต์
- **`seq`** — ตัวนับเฟรม ทำให้ backend รู้ว่ามีบรรทัดหายไปกี่บรรทัด (`link.dropped_frames`)
- **ส่งค่าดิบเท่านั้น** — polarity และ threshold อยู่ใน backend config เพื่อให้ปรับ
  calibration ได้โดยไม่ต้อง flash บอร์ดใหม่ทุกครั้ง
