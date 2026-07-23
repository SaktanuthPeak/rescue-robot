# 🔌 Hardware Specification - Educational Robotics Platform

เอกสารสรุปสเปกฮาร์ดแวร์ที่เลือกใช้ในโปรเจคหุ่นยนต์เพื่อการศึกษา (Encoder + IR Sensors Version)

---

## Main Computing (สมองส่วนบน)
*   **Raspberry Pi 4 / 5:** ทำหน้าที่ปล่อย WiFi AP, โฮสต์เว็บเซอร์วิส (Svelte + FastAPI) และประมวลผลคำสั่งเชิงตรรกะระดับสูงจาก Block Code

## Real-time Controller (สมองควบคุมการเคลื่อนที่)
*   **ESP32 NodeMCU:** รับหน้าที่อ่านสัญญาณ Interrupt จาก Wheel Encoder, คุมสัญญาณ PWM ขับมอเตอร์ และอ่านค่า Digital/Analog จาก IR Sensors ทั้งหมดแบบ Real-time

## Perception & Safety Sensors (ระบบรับรู้สิ่งกีดขวางรอบคัน)
*   **IR Obstacle Avoidance Sensors (4 - 6 ตัว):** ติดตั้งรอบคัน (หน้าซ้าย, หน้าขวา, ข้างซ้าย, ข้างขวา, ด้านหลัง) ทำหน้าที่เป็นเรดาร์ระยะประชิด (0 - 30 ซม.) สำหรับตรวจจับสิ่งกีดขวางรอบตัวและอุดจุดอับสายตาทั้งหมด

## Dead Reckoning & Drive System (ระบบนำทางและการขับเคลื่อน)
*   **DC Geared Motor with Wheel Encoders:** มอเตอร์ขับเคลื่อนล้อที่มีเซนเซอร์วัดรอบในตัว ทำหน้าที่คำนวณระยะทางและพิกัดจำลอง (Wheel Odometry) เพื่อให้หุ่นยนต์เดินหน้า/ถอยหลังตามหน่วยเซนติเมตร และเลี้ยวตามองศาได้อย่างแม่นยำ
*   **TB6612FNG / L298N Motor Driver:** ไอซีขับกระแสไฟฟ้าสำหรับควบคุมความเร็วและทิศทางของมอเตอร์

## Indicators & Sound (ระบบโต้ตอบ)
*   **NeoPixel RGB LED Strip:** แสดงสถานะของหุ่นยนต์ตามบล็อกคำสั่งของเด็กๆ
*   **Active Buzzer:** ส่งเสียงเตือนตามตัวโน้ตดนตรีที่กำหนดในบล็อกคำสั่ง

## Power Management (ระบบจัดการพลังงาน)
*   **Li-ion / Li-Po Battery (7.4V - 12V):** แหล่งจ่ายไฟหลัก
*   **5V Buck Converter (≥ 3A):** แหล่งจ่ายไฟแยกส่วนระหว่างบอร์ดประมวลผล (Raspberry Pi / ESP32) และมอเตอร์ เพื่อความเสถียรสูงสุด
