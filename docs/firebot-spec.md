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

### ข้อมูลที่ถ่ายทำจากระบบ (Phase 1 Telemetry)

| ข้อมูล | แหล่งที่มา | ความถี่ |
|-------|-----------|--------|
| 🔥 Flame Detection Status | IR Sensor | Every 100ms |
| 📏 Distance Measurement | LiDAR / Ultrasonic | Every 200ms |
| ⚡ Motor Status | Motor Driver | Real-time |
| 😷 Mask Dispenser Count | Counter Module | On demand |
| 🔋 System Status | Board Diagnostics | Every 500ms |

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

### 1. Arduino → Backend (Serial Input)
```
Arduino sends: "FLAME:1,DIST:45,STATUS:RUNNING\n"
                ↓
pyserial reads and parses
                ↓
Backend stores in memory/state
```

### 2. Backend → Frontend (WebSocket Broadcast)
```
FastAPI WebSocket endpoint broadcasts every 100ms:
{
  "timestamp": 1692547200000,
  "flame_detected": true,
  "distance_cm": 45,
  "motor_status": "running",
  "mask_count": 12,
  "system_status": "operational"
}
                ↓
Frontend receives and updates UI in real-time
```

### 3. Frontend → Backend → Arduino (Control Command - Future)
```
User clicks "Dispense Mask" button on dashboard
                ↓
Frontend sends via WebSocket
                ↓
Backend translates to command string
                ↓
pyserial sends to Arduino: "DISPENSE:1\n"
```

---

## ✨ Phase 1 Feature Checklist

- [ ] **Arduino Prototype** - Emulate sensor data with realistic patterns
- [ ] **Serial Communication** - pyserial setup and reliable data parsing
- [ ] **FastAPI Backend** - REST endpoints + WebSocket server
- [ ] **Svelte Dashboard** - Real-time telemetry display
  - [ ] Flame detection indicator
  - [ ] Distance measurement display
  - [ ] Motor status panel
  - [ ] Mask dispenser counter
- [ ] **WebSocket Integration** - Bi-directional real-time connection

---

## 📈 Future Phases (For Reference)

**Phase 2:** Thermal Camera integration + AI object detection  
**Phase 3:** Autonomous navigation + pathfinding  
**Phase 4:** Field deployment + safety validation

---

## 🔗 Related Documentation

- [Hardware Specifications](./hardware-spec.md)
- [Software Architecture](./software-spec.md)
- [Backend Setup Guide](../backend/README.md)
- [Frontend Setup Guide](../frontend/README.md)
