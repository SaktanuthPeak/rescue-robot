# 🤖 FireBot: Search, Rescue & Mask Dispenser Robot
หุ่นยนต์กู้ภัยขนาด 1.5 เมตร สำหรับตรวจจับจุดเกิดเหตุเพลิงไหม้ ค้นหาผู้ประสบภัย และแจกจ่ายหน้ากากกันแก๊สอัตโนมัติ

---

## 📋 ภาพรวมโปรเจกต์ (Project Overview)

FireBot เป็นหุ่นยนต์กู้ภัยแบบล้อสายพาน (Tracked Robot) ที่ออกแบบมาเพื่อเข้าไปในพื้นที่เพลิงไหม้ มีระบบเซนเซอร์สำหรับ:
- 🔥 **ตรวจจับเปลวไฟ** - IR/Flame Sensor
- 👤 **ค้นหาผู้ประสบภัย** - Thermal Camera + AI (แผนพัฒนา)
- 😷 **แจกหน้ากากกันแก๊ส** - กลไกจ่ายอัตโนมัติเมื่อพบเป้าหมาย

ปัจจุบันอยู่ในระยะ **Prototype Phase** ซึ่งเน้นการสื่อสารระหว่างฮาร์ดแวร์ (Arduino) และ Web Dashboard สำหรับมอนิเตอร์สถานะแบบ Real-time

---

## 🎯 Phase 1: Web Dashboard & Backend Communication (เฟสปัจจุบัน)

### เป้าหมายหลัก
ระยะแรกของโปรเจกต์เน้นการสร้างช่องทางสื่อสาร **Arduino ↔ Backend ↔ Frontend Dashboard** เพื่อ:

1. **Backend API** - รับข้อมูลเซนเซอร์จาก Arduino ผ่านสาย Serial (USB)
2. **Web Frontend** - แสดงผลสถานะแบบ Real-time บน Dashboard
3. **WebSocket Communication** - ส่งข้อมูล Telemetry สดแบบ 2 ทิศทาง

### สถาปัตยกรรมระบบ (System Architecture)

```
┌─────────────────┐
│   Arduino Board │ (Emulating Robot Sensors & Motors)
│  (Prototype)    │
└────────┬────────┘
         │
         │ USB Serial (pyserial)
         │ [Flame, Distance, Status Data]
         │
┌────────▼────────────────┐
│   FastAPI Backend       │
│ • Reads Serial Data     │
│ • Parses Telemetry      │
│ • Broadcasts via WS     │
└────────┬────────────────┘
         │
         │ WebSocket
         │ [Real-time Sensor Values]
         │
┌────────▼──────────────────┐
│  Svelte Frontend          │
│ • Live Dashboard Display  │
│ • Monitor Robot Status    │
│ • Control Signals (Tx)    │
└───────────────────────────┘
```

### ข้อมูลที่ส่งในระบบ (Phase 1 Telemetry)

Phase 1 อ่าน **IR flame sensor แบบ analog 4 ตัว** เท่านั้น (ยังไม่มี distance / motor / mask
dispenser) เพราะเป้าหมายคือทำให้เส้นทาง Arduino → serial → WebSocket → dashboard ทำงานครบก่อน

| ข้อมูล | แหล่งที่มา | ความถี่ |
|-------|-----------|--------|
| 🔥 Flame intensity × 4 ทิศ (front/right/rear/left) | IR flame sensor แบบ analog บน `A0`–`A3` | 20 Hz จากบอร์ด |
| 🧭 ทิศที่แรงที่สุด + bearing ประมาณการ | คำนวณจาก 4 ค่าข้างต้น | ทุกเฟรม |
| 🔌 สถานะ serial link (source/state/dropped/parse errors) | Backend | ทุกเฟรม + heartbeat 1 Hz |
| 🩺 Device status (OK/WARN/FAULT) | บอร์ดตรวจว่ามีช่องค้างที่ rail | ทุกเฟรม |

**ใช้ analog ไม่ใช่ digital:** ต้องต่อขา `AO` ของ sensor ไม่ใช่ `DO` ถ้าใช้ `DO` จะได้แค่
เจอ/ไม่เจอ ซึ่งบอกความแรงและทิศทางไม่ได้ และหมายเหตุว่า **ขา `D8`–`D11` ของ Uno อ่าน analog
ไม่ได้** — ADC ต่อกับ `A0`–`A5` เท่านั้น

---

## 🛠️ Tech Stack (Phase 1)

### Frontend
- **Svelte + Vite** - Fast, lightweight framework for real-time dashboard
- **Tailwind CSS** - Responsive design for monitoring interface
- **WebSocket Client** - Real-time telemetry reception

### Backend
- **Python FastAPI** - REST API server + WebSocket endpoint
- **uvicorn** - ASGI server
- **pyserial** - USB/Serial communication with Arduino
- **asyncio** - Async task management for real-time data streaming

### Hardware (Prototype)
- **Arduino Board** - Simulates robot sensors & telemetry data
- **USB Serial Connection** - Data transmission to backend

---

## 📊 Data Flow (Phase 1)

ส่วนนี้เป็น **ข้อตกลงที่ผูกพันทั้งสองฝั่ง** (normative) — backend และ frontend ต้องตรงกันเป๊ะ

### 1. Arduino → Backend (FB1 line protocol ผ่าน serial)

```
FB1,<front>,<right>,<rear>,<left>,<status>,<seq>*<CK>\n
```

ตัวอย่าง: `FB1,812,118,1010,990,OK,4137*7B` — baud **115200**

- `FB1` — magic prefix **บรรทัดที่ไม่เริ่มด้วยสิ่งนี้จะถูกทิ้งทั้งหมด** นี่คือกลไกกรองขยะที่
  bootloader พิมพ์ตอน reset (sketch จึงตั้งใจไม่พิมพ์ข้อความต้อนรับ)
- ค่า ADC ดิบ 4 ตัว `0..1023` เรียง **front, right, rear, left** = `A0..A3`
- `status` — `OK` / `WARN` / `FAULT` สุขภาพการต่อสาย **ไม่ใช่** การตรวจจับไฟ
- `seq` — uint16 วนที่ 65536 ทำให้นับบรรทัดที่หายได้
- `*CK` — XOR ของทุกไบต์ก่อน `*` เป็น hex 2 หลัก **บรรทัดที่ไม่มี checksum ยอมรับ**
  (เพื่อให้พิมพ์มือทดสอบได้) แต่ถ้ามีแล้วไม่ตรงจะถูกทิ้ง

เหตุผลที่ใช้ CSV + checksum ไม่ใช่ JSON: บน ATmega328P รูปแบบนี้ใช้ `snprintf` ครั้งเดียว
สั้นกว่า (34 vs ~110 ไบต์) และสำคัญที่สุด — ถ้าสัญญาณรบกวนทำให้เลขหลักเดียวเปลี่ยน JSON ที่ยัง
ถูกไวยากรณ์จะกลายเป็น *ค่าที่ดูสมเหตุสมผล* สำหรับหุ่นตรวจจับไฟ "ข้อมูลเสียถูกอ่านเป็นไม่มีไฟ"
คือความผิดพลาดที่คุ้มค่ากับ checksum 5 ไบต์

### 2. Backend → Frontend (WebSocket)

`ws://<host>/v1/telemetry/ws` — **message type เดียว** โดยสถานะ link เป็น *field* ไม่ใช่ message
แยก เพื่อให้ frontend ไม่ต้องรวม 2 stream และเพื่อให้ทั้ง payload อธิบายได้ใน OpenAPI

```json
{
  "type": "telemetry", "v": 1, "ts": 1755600000123, "seq": 4137,
  "status": "OK", "adc_max": 1023,
  "link": { "source": "mock", "state": "streaming",
            "last_frame_age_ms": 12, "dropped_frames": 3, "parse_errors": 0 },
  "flame": {
    "front": { "raw": 812,  "intensity": 0.206, "detected": false },
    "right": { "raw": 118,  "intensity": 0.885, "detected": true  },
    "rear":  { "raw": 1010, "intensity": 0.013, "detected": false },
    "left":  { "raw": 990,  "intensity": 0.032, "detected": false }
  },
  "flame_detected": true, "strongest_direction": "right"
}
```

| field | ความหมาย |
|---|---|
| `v` | protocol version ถ้าไม่ตรง dashboard จะขึ้น banner และ **ไม่แสดงค่า** แทนที่จะอ่านผิด |
| `ts` | epoch **milliseconds** เวลาที่ backend รับบรรทัด ไม่ใช่เวลาของบอร์ด |
| `link.source` | `mock` = ข้อมูลจำลอง **UI ต้องขึ้น ribbon SIMULATED DATA** |
| `link.state` | ไม่ใช่ `streaming` = ค่าไม่สด ต้องทำให้กราฟิกจางลง |
| `link.last_frame_age_ms` | วัดฝั่ง server ด้วย monotonic clock จึงไม่เพี้ยนตามนาฬิกา browser |
| `flame.*.raw` | ADC ดิบ ใช้ในหน้า debug เท่านั้น |
| `flame.*.intensity` | `0.0..1.0` **แปลง polarity แล้ว 1.0 = ไฟแรงสุด — ใช้ค่านี้วาดทุกอย่าง** |
| `strongest_direction` | ทิศที่แรงสุดในกลุ่มที่ `detected` ถ้าไม่มีเป็น `null` |

**polarity อยู่ที่ backend config ไม่ใช่บนสาย:** sensor YG1006 ส่วนใหญ่เป็น active-low (เจอไฟ
มาก = ค่าต่ำ) แต่การต่อสายต่างกันได้ Arduino จึงส่งค่าดิบ แล้ว backend ใช้ `FLAME_ACTIVE_LOW` +
`FLAME_THRESHOLD` คำนวณ `intensity`/`detected` — ปรับ calibration ได้โดยไม่ต้อง flash ใหม่
และตัดปัญหา "0 คือไฟหรือไม่มีไฟ" ออกจาก frontend ทั้งหมด

**สิ่งที่ frontend เชื่อถือได้:** `flame` ไม่เคยเป็น null (hub seed ค่า idle ตอน start) และ
REST snapshot คืน 200 เสมอ — ไม่มีบอร์ดคือ *สถานะ* ไม่ใช่ error

REST endpoints เพิ่มเติม: `GET /v1/telemetry` (snapshot รูปร่างเดียวกันเป๊ะ) และ
`GET /v1/telemetry/config` (threshold/polarity/adc_max สำหรับวาดเส้น threshold)

### 3. Frontend → Backend → Arduino (Phase 2)

ยังไม่มีใน Phase 1 — stream เป็น send-only `type` เป็น discriminated field ไว้ตั้งแต่ต้นเพื่อรองรับ
คำสั่ง `DISPENSE:1` ในอนาคต

---

## ✨ Phase 1 Feature Checklist

- [x] **FB1 line protocol** — magic prefix + XOR checksum + seq gap accounting
- [x] **Arduino sketch** — [firmware/flame_telemetry](../firmware/flame_telemetry/) อ่าน `A0`–`A3` ที่ 20 Hz
- [x] **Serial transport** — pyserial บน dedicated thread + reconnect loop
- [x] **Mock source** — flame จำลองโคจรรอบหุ่น ใช้ dev ได้โดยไม่ต้องมีบอร์ด
- [x] **FastAPI backend** — REST snapshot + `/config` + WebSocket fan-out (latest-wins)
- [x] **Svelte dashboard** — กราฟิกรถมองจากบน + 4 sensor wedges
  - [x] heat ramp ตามความแรง + radial gauge
  - [x] bearing needle ประมาณทิศไฟ
  - [x] staleness detection (กราฟิกจาง + badge)
  - [x] reconnect แบบ exponential backoff + jitter
  - [x] hysteresis กันเตือนซ้ำ
- [x] **Tests** — 65 tests (protocol, hub fan-out, REST contract, WebSocket)
- [ ] **ทดสอบกับบอร์ดจริง** — ต้อง upload sketch + ต่อ sensor + `TELEMETRY_SOURCE=serial`

---

## 📈 Future Phases (For Reference)

**Phase 2:** Thermal Camera integration + AI object detection  
**Phase 3:** Autonomous navigation + pathfinding  
**Phase 4:** Field deployment + safety validation

สถาปัตยกรรมเป้าหมายแบบเต็มระบบ (Raspberry Pi AP, CAN bus multi-board, RC fail-over, FPV, AI
pipeline, fail-safe protocols) อยู่ใน [`system-architecture.md`](./system-architecture.md) — เอกสาร
นั้นคือ target spec ข้าม phase ส่วนไฟล์นี้คือสิ่งที่ implement แล้วจริงใน Phase 1

---

## 🔗 Related Documentation

- [System Architecture (target spec, ทุกเฟส)](./system-architecture.md)
- [Firmware / การต่อสาย + upload](../firmware/README.md)
- [Backend Setup Guide](../backend/README.md)
- [Frontend Setup Guide](../frontend/README.md)
