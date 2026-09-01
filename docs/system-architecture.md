# 🏗️ System Architecture (Target Spec)

status: draft for review · created: 2026-09-01 · updated: 2026-09-01

เอกสารนี้บันทึก **สถาปัตยกรรมเป้าหมาย (target architecture)** ของ FireBot ทั้งระบบ — ฮาร์ดแวร์,
เครือข่าย, ซอฟต์แวร์, AI pipeline และ fail-safe protocol — ครอบคลุมทุกเฟสของโปรเจกต์ ไม่ใช่แค่สิ่งที่
implement แล้ว งานที่ทำเสร็จจริงในแต่ละเฟสอยู่ใน [`firebot-spec.md`](./firebot-spec.md) (Phase 1
ปัจจุบันใช้ Arduino เดี่ยวต่อ USB serial เข้ากับ backend โดยตรง ยังไม่มี Raspberry Pi / CAN bus /
RC fail-over ตามที่อธิบายด้านล่าง — ดูสถานะ implement จริงที่ feature checklist ของแต่ละเฟส)

---

## 1. ภาพรวมระบบ (System Overview)

ระบบหุ่นยนต์กู้ภัยกึ่งอัตโนมัติ (Semi-Autonomous Rescue Robot) ออกแบบมาเพื่อค้นหาผู้ประสบภัยและ
ตรวจจับอัคคีภัย เน้นความเสถียรในการทำงานแบบออฟไลน์เมื่อสูญเสียสัญญาณเครือข่าย พร้อมระบบควบคุมคู่
(Dual Control System) เพื่อความปลอดภัยสูงสุดในสถานการณ์ฉุกเฉิน

## 2. Hardware Architecture

| หมวดหมู่ | อุปกรณ์ที่เลือกใช้ | หน้าที่การทำงาน |
| :--- | :--- | :--- |
| **หน่วยประมวลผลหลัก** | Raspberry Pi 3 Model B (RAM 1GB) | รัน Web Server, รับภาพจากกล้อง USB, เป็นจุดปล่อย Wi-Fi (Access Point) |
| **ไมโครคอนโทรลเลอร์** | Arduino Mega (1 บอร์ด), Arduino Uno (2 บอร์ด) | ควบคุมมอเตอร์ล้อ, อ่านค่าเซนเซอร์, สั่งงานลิฟต์หน้ากาก, สลับโหมด Fail-Safe |
| **ระบบสื่อสาร (CAN Bus)** | MCP2515 CAN Module (4 ตัว) | เชื่อมต่อบอร์ดทั้งหมดเข้าด้วยกันผ่าน SPI (Pi 1 ตัว, Mega 1 ตัว, Uno 2 ตัว) |
| **เซนเซอร์ตรวจจับ** | MPU6050, Flame Sensor / MQ-2 | วัดความเอียงป้องกันรถคว่ำ, ตรวจจับความร้อนและควันไฟ |
| **กลไกช่วยเหลือ** | NEMA 17 Stepper Motor | ขับเคลื่อน Scissor Lift สำหรับยกลิฟต์แจกหน้ากากกันควัน |
| **ระบบสื่อสารฉุกเฉิน** | RC Receiver (2.4GHz / 900MHz) | ตัวรับสัญญาณวิทยุสำหรับบังคับมือเมื่อ Wi-Fi หลุด |
| **ระบบภาพฉุกเฉิน** | 5.8GHz FPV Camera & VTX | ส่งภาพวิดีโออนาล็อกไร้ความหน่วงไปยังจอมอนิเตอร์ของคนขับโดยไม่ผ่าน Pi |

## 3. Software & Network Architecture

- **Backend API:** FastAPI ทำหน้าที่สื่อสารกับ Arduino ผ่านบัส CAN และส่งภาพสตรีมมิ่งแบบ MJPEG
- **Frontend Dashboard:** Svelte / SvelteKit สำหรับแสดงภาพสด, สถานะแบตเตอรี่, การแจ้งเตือนจาก AI และรับคำสั่งบังคับทิศทาง (ผ่าน WebSocket)
- **Database (Offline-First):** PouchDB / CouchDB หรือ SQLite สำหรับเก็บ Log ข้อมูลชั่วคราวบนตัวหุ่น และรอซิงค์กลับเมื่อมีสัญญาณ
- **Deployment:** สร้าง Container ด้วย Docker และ Docker Compose (จัดการ Package Python ด้วย Poetry) — ดู [`docker-deployment.md`](./docker-deployment.md) สำหรับสถานะที่ implement แล้ว
- **Network Strategy:** ตั้งค่า Raspberry Pi ให้เป็น Wireless Access Point (AP) โดยมี Static IP เพื่อให้คอมพิวเตอร์คนขับสามารถเชื่อมต่อตรงได้โดยไม่ต้องพึ่งพาเร้าเตอร์ภายนอก

## 4. AI & Computer Vision Pipeline

- **Processing Unit:** รันโมเดลบนคอมพิวเตอร์/โน้ตบุ๊กของคนขับ (Offloaded Inference) เพื่อลดภาระ RAM และ CPU ของ Raspberry Pi 3
- **Model Selection:** YOLO-Nano หรือ YOLO11n (2 Classes: `person` และ `fire/smoke`)
- **Hardware Trigger:** หาก AI ตรวจพบผู้ประสบภัย (Person detected) และหุ่นยนต์อยู่ในระยะที่เหมาะสม ระบบจะส่งคำสั่งไปยังบอร์ด Arduino ให้รัน NEMA 17 เพื่อยกลิฟต์หน้ากากขึ้นอัตโนมัติ

## 5. Fail-Safe & Emergency Protocols

| สถานการณ์ | การตอบสนองของระบบ |
| :--- | :--- |
| **หุ่นยนต์จะพลิกคว่ำ** | MPU6050 ตรวจพบองศาอันตราย -> Arduino ตัดคำสั่งเดินหน้าและเบรกล้ออัตโนมัติ |
| **สัญญาณ Wi-Fi หลุด** | ภาพบนเว็บค้าง -> คนขับสลับไปดูจอ 5.8GHz FPV |
| **ขาดการเชื่อมต่อจาก Pi** | Arduino ตรวจไม่พบคำสั่งนาน 2 วินาที -> ตัดการเชื่อมต่อ Pi -> สลับไปรับคำสั่งจาก RC Receiver |
| **ข้อมูลส่งกลับไม่ได้** | สคริปต์บน Pi เก็บ Log เซนเซอร์ลง PouchDB ภายในเครื่อง -> รอสัญญาณกลับมาเพื่อ Sync |

---

## 🔗 Related Documentation

- [FireBot Phase 1 Spec (implemented)](./firebot-spec.md)
- [Docker Deployment](./docker-deployment.md)
- [Firmware / การต่อสาย + upload](../firmware/README.md)
- [Backend Setup Guide](../backend/README.md)
- [Frontend Setup Guide](../frontend/README.md)
