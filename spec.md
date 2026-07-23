# 🤖 Educational Robotics Platform - Feature & Tech Stack Specifications

เอกสารสรุปคุณสมบัติทางเทคนิค (Specifications) ฟีเจอร์การทำงาน และเทคโนโลยีที่เลือกใช้ในโปรเจคหุ่นยนต์เพื่อการศึกษา

---

## 🌟 1. System Features (คุณสมบัติของระบบ)

### 🧩 A. บล็อกคำสั่งการเรียนรู้ (Blockly Features)
การลากวางบล็อกโค้ดบนหน้าเว็บ (พัฒนาด้วย Google Blockly) จะแบ่งเป็น 4 หมวดหมู่หลัก:
*   **Movement (การเคลื่อนที่):** 
    *   `เดินหน้า / ถอยหลัง` (ระบุระยะทางเป็น เซนติเมตร)
    *   `เลี้ยวซ้าย / เลี้ยวขวา` (ระบุทิศทางเป็น องศา เช่น 45°, 90°)
    *   `ตั้งความเร็ว` (ปรับกำลังมอเตอร์ 0-100%)
*   **Sensors (การรับรู้):**
    *   `อ่านระยะห่างด้านหน้า` (ดึงข้อมูล Real-time จาก TF-Luna LiDAR)
    *   `หมุนมุมเซนเซอร์` (ควบคุม Servo Motor ที่ติดตั้ง LiDAR เพื่อกวาดสแกนมุม 0-180°)
*   **Indicators (การแสดงผลบนหุ่น):**
    *   `เปลี่ยนสีไฟ` (ควบคุมไฟ RGB LED / NeoPixel)
    *   `ส่งเสียงเตือน` (ควบคุม Buzzer ตามตัวโน้ตดนตรี)
*   **Control Logic (ตรรกะและการควบคุม):**
    *   `Loop / Repeat` (การทำซ้ำตามรอบที่กำหนด หรือทำซ้ำตลอดเวลา)
    *   `Condition (If-Else)` (เงื่อนไขการตัดสินใจ เช่น ถ้าเจอสิ่งกีดขวางให้เลี้ยว)
    *   `Delay` (คำสั่งหน่วงเวลาเป็นวินาที)

### 💻 B. ฟีเจอร์หน้าเว็บควบคุม (Web Dashboard Features)
*   **Local UI Hosting:** ตัวเว็บรันอยู่บน Raspberry Pi โดยตรง หุ่นยนต์จะปล่อย WiFi Hotspot ออกมา (Access Point Mode) ผู้ใช้งานนำ iPad/Notebook มาเชื่อมต่อแล้วเปิดเบราว์เซอร์ใช้งานได้ทันทีโดยไม่ต้องพึ่งพาอินเทอร์เน็ต
*   **Live Telemetry Dashboard:** หน้าจอแสดงผลสถานะของหุ่นยนต์แบบ Real-time (ค่าระยะทางจาก LiDAR, ความเร็วปัจจุบัน, ปริมาณแบตเตอรี่)
*   **Real-time Transpiler:** หน้าต่าง Code Viewer ด้านข้าง แสดงโค้ดภาษา C++ (Arduino) ที่ถูกแปลงมาจาก Block Code แบบ Real-time เพื่อให้เด็กระดับสูงได้เรียนรู้ภาษา C++ ควบคู่ไปด้วย
*   **Remote Web Control (Hybrid Mode):** มีโหมด Manual บังคับหุ่นยนต์ผ่านจอยสติ๊กบนหน้าเว็บ (Virtual Joystick) เผื่อสลับจากการเขียนโค้ดอัตโนมัติ

---

## 🛠️ 2. Tech Stack (เทคโนโลยีที่เลือกใช้)

โครงสร้างระบบแบบ Hybrid Architecture แยกส่วน High-level (ประมวลผลเว็บ/ข้อมูล) และ Low-level (ควบคุมฮาร์ดแวร์แบบ Real-time)

### 💾 Software Stack
*   **Frontend (User Interface):**
    *   **Svelte Framework:** เลือกใช้เนื่องจากมีขนาดเล็ก โหลดเร็วมาก (Compiled to pure JS) เหมาะกับเครื่องที่มีสเปกจำกัดอย่าง Raspberry Pi
    *   **Tailwind CSS:** สำหรับออกแบบ Dashboard ให้สวยงาม ทันสมัย และรองรับการแสดงผลบน Tablet/Mobile (Responsive Design)
    *   **Google Blockly:** คลังสมองกลหลักสำหรับการทำระบบลากวางบล็อกคำสั่งและการทำ Custom Block (Block to String/Code Generator)
*   **Backend (Server & Logic Processing):**
    *   **Python (FastAPI):** ใช้สร้าง Web Service และ REST API สำหรับส่ง Static Files (Svelte build) และประมวลผลคำสั่ง
    *   **WebSockets:** ใช้สำหรับสร้างช่องทางสื่อสารข้อมูลสดแบบสองทิศทาง (Bi-directional Real-time Telemetry) ระหว่างหน้าเว็บและตัวหุ่นยนต์
    *   **PySerial:** ไลบรารี Python สำหรับจัดการเปิด Communication Port สื่อสารกับบอร์ดไมโครคอนโทรลเลอร์ผ่านสาย USB
*   **Firmware (On-board Robot Control):**
    *   **C++ (Arduino Framework):** เขียนโค้ดโครงสร้าง Command Parser เพื่อรับชุดคำสั่งที่เป็น String (เช่น `MOVE:30\n`) จาก Python แล้วตีความไปสั่งงานฮาร์ดแวร์

### 🔌 Hardware Stack (Encoder + IR Sensors Version)
*   **Main Computing (สมองส่วนบน):**
    *   **Raspberry Pi 4 / 5:** ทำหน้าที่ปล่อย WiFi AP, โฮสต์เว็บเซอร์วิส (Svelte + FastAPI) และประมวลผลคำสั่งเชิงตรรกะระดับสูงจาก Block Code
*   **Real-time Controller (สมองควบคุมการเคลื่อนที่):**
    *   **ESP32 NodeMCU:** รับหน้าที่อ่านสัญญาณ Interrupt จาก Wheel Encoder, คุมสัญญาณ PWM ขับมอเตอร์ และอ่านค่า Digital/Analog จาก IR Sensors ทั้งหมดแบบ Real-time
*   **Perception & Safety Sensors (ระบบรับรู้สิ่งกีดขวางรอบคัน):**
    *   **IR Obstacle Avoidance Sensors (4 - 6 ตัว):** ติดตั้งรอบคัน (หน้าซ้าย, หน้าขวา, ข้างซ้าย, ข้างขวา, ด้านหลัง) ทำหน้าที่เป็นเรดาร์ระยะประชิด (0 - 30 ซม.) สำหรับตรวจจับสิ่งกีดขวางรอบตัวและอุดจุดอับสายตาทั้งหมด
*   **Dead Reckoning & Drive System (ระบบนำทางและการขับเคลื่อน):**
    *   **DC Geared Motor with Wheel Encoders:** มอเตอร์ขับเคลื่อนล้อที่มีเซนเซอร์วัดรอบในตัว ทำหน้าที่คำนวณระยะทางและพิกัดจำลอง (Wheel Odometry) เพื่อให้หุ่นยนต์เดินหน้า/ถอยหลังตามหน่วยเซนติเมตร และเลี้ยวตามองศาได้อย่างแม่นยำ
    *   **TB6612FNG / L298N Motor Driver:** ไอซีขับกระแสไฟฟ้าสำหรับควบคุมความเร็วและทิศทางของมอเตอร์
*   **Indicators & Sound (ระบบโต้ตอบ):**
    *   **NeoPixel RGB LED Strip & Active Buzzer:** สำหรับแสดงสถานะและส่งเสียงเตือนตามบล็อกคำสั่งของเด็กๆ
*   **Power Management (ระบบจัดการพลังงาน):**
    *   **Li-ion / Li-Po Battery (7.4V - 12V) + 5V Buck Converter (≥ 3A):** แหล่งจ่ายไฟแยกส่วนระหว่างบอร์ดประมวลผลและมอเตอร์เพื่อความเสถียรสูงสุด