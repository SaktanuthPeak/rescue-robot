# CAN Receiver

โฟลเดอร์นี้ปัจจุบันใช้เป็น CAN receiver สำหรับอ่านสถานะจาก CAN sender เท่านั้น

- รับ `CAN_ID_MOTOR` (`0x100`) และ `CAN_ID_ARM` (`0x101`)
- ตรวจสอบ status และ timeout 300 ms
- ส่งสถานะไป Raspberry Pi ผ่าน USB Serial ในรูปแบบ `RB2`
- ไม่มีการสั่ง motor, servo, PCA9685 หรือ relay/pump

รูปแบบ Serial:

```text
RB2,motor_code,motor_alive,arm_code,arm_alive,0,0,seq*CK
```

ไฟล์ PCA9685 และ configuration เดิมของ Arm Controller Test ถูกถอดออกจากโฟลเดอร์นี้แล้ว โดย implementation แขนกลจริงยังอยู่ใน `firmware/arm_controller/`.
