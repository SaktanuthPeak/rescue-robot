# Standalone CAN bus debug

สเก็ตช์นี้แยกจาก `firmware/arm_controller/` และไม่มีโค้ดควบคุม servo, relay หรือ motor ใช้สำหรับแยกหาสาเหตุว่า CAN เสียที่ชั้นใด:

1. `MCP2515` และ SPI ใช้งานได้หรือไม่
2. MCP2515 รับ frame จาก bus ได้หรือไม่
3. MCP2515 ส่ง frame ได้และมี node อื่น ACK หรือไม่
4. ขา INT มีปัญหาหรือไม่ — สเก็ตช์นี้ใช้ polling จึงไม่พึ่ง D2

## Configuration ที่ตรงกับโปรเจกต์

| รายการ | ค่า |
| --- | --- |
| Board | Arduino UNO/Mega + MCP2515 |
| CS | D10 |
| INT | D2 (แสดงสถานะเท่านั้น) |
| bitrate | 500 kbit/s |
| MCP2515 crystal | 8 MHz |
| Arm command ID | `0x101` |
| Debug ID | `0x7A0` |

ถ้าโมดูล MCP2515 พิมพ์ว่า `16 MHz` ให้แก้ `CAN_CLOCK` ในไฟล์ `.ino` เป็น `MCP_16MHZ` และแก้ `CAN_CLOCK_NAME` เป็น `"16 MHz"` ก่อน upload

## วิธีทดสอบ

1. ถอด/หยุด firmware arm เดิมก่อน แล้ว flash โฟลเดอร์นี้ลงบอร์ด arm controller
2. เปิด Serial Monitor ที่ `115200 baud`
3. ตรวจข้อความ `CAN READY`
4. ส่งคำสั่ง `l` ก่อน — ต้องได้ `LOOPBACK PASS` แม้ยังไม่ต่อ CANH/CANL
5. ต่อ CANH ไป CANH, CANL ไป CANL, GND ร่วม และมี termination 120 Ω ที่ปลาย bus สองด้าน
6. เปิด CAN sender ที่ส่ง `0x100`/`0x101` แล้วดูบรรทัด `RX`
7. ใช้ `x` ส่ง frame ทดสอบ ID `0x7A0` หากต้องการทดสอบ TX ไปยัง node อื่น
8. ใช้ `a` ส่ง `0x101, DATA=00` ซึ่งเป็นคำสั่ง ARM STOP แบบปลอดภัย

## Serial commands

```text
h  help
r  register report: CANSTAT/CNF/TEC/REC/EFLG
l  MCP2515 internal loopback test
x  send harmless frame ID 0x7A0
a  send ARM STOP frame ID 0x101 DATA=00
n  force NORMAL mode
p  one-line status
```

## แปลผลที่พบบ่อย

- `CAN0.begin() -> CAN_FAILINIT`: ปัญหา SPI, CS, ไฟเลี้ยง, MCP2515 เสีย หรือเลือก crystal ผิด
- `LOOPBACK PASS` แต่ไม่มี `RX` ใน normal mode: ปัญหาที่ CANH/CANL, GND, termination, bitrate หรือไม่มี node อื่นส่ง
- `RX` มี frame `0x101` แต่ arm จริงไม่ขยับ: ปัญหาอยู่หลัง CAN เช่น firmware arm, PCA9685, relay/servo หรือ logic ของ application
- `TEC` เพิ่ม, `TX-PASSIVE` หรือ `BUS-OFF`: frame ส่งออกแต่ไม่มี ACK หรือ bitrate/สายมีปัญหา
- `REC` เพิ่ม: รับ frame แต่ตรวจพบ bit/CRC/form error ซึ่งมักเกี่ยวกับ bitrate, crystal, สาย หรือสัญญาณรบกวน
- `INT pin D2=HIGH` ตลอด แต่ยังมี `RX`: CAN bus ใช้ได้ และปัญหาเดิมน่าจะอยู่ที่สาย INT/interrupt logic

## หมายเหตุความปลอดภัย

โหมด debug ไม่ขับ actuator ใด ๆ แต่คำสั่ง `a` จะส่ง `ARM STOP` บน bus ดังนั้นควรยก/ถอดโหลดแขนกลไว้ระหว่างทดสอบ
