# Node 3 - phân chia task và chức năng

## 1. Mục tiêu

Node 3 được tách thành ba task ứng dụng độc lập: `ALARM`, `CALL` và `MODEM_AT`.
Việc tách chỉ thay đổi cách tổ chức thực thi; chức năng cảnh báo, kiểm tra modem,
gọi điện và tự động ngắt cuộc gọi vẫn giữ nguyên.

## 2. Sơ đồ giao tiếp

```text
CAN ISR -> ALARM -> xAlarmQueue -> CALL
                              CALL -> xAtCmdQueue -> MODEM_AT -> USART2/A7680C
                              CALL <- xModemEventQueue <- MODEM_AT
```

`MODEM_AT` là thành phần duy nhất được quyền gửi và nhận dữ liệu trên USART2.
USART2 ISR chỉ đưa byte vào ring buffer và notification cho `MODEM_AT`; ISR không
parse chuỗi AT.

## 3. Trách nhiệm từng task

### ALARM

- Nhận frame cảm biến và frame ngưỡng qua CAN.
- Tính toán trạng thái cảnh báo và hysteresis.
- Ghi trạng thái mới nhất vào `xAlarmQueue`.
- Hoạt động theo chu kỳ 10 ms hoặc được đánh thức từ CAN ISR.

Task này giữ nguyên logic và giao diện IPC trước đây.

### CALL

`CALL` sở hữu state machine và chính sách gọi, không truy cập trực tiếp USART2.

- Đọc trạng thái mới nhất bằng `xQueuePeek(xAlarmQueue)`.
- Gửi lệnh `AT`, `ATD...` và `ATH` vào `xAtCmdQueue`.
- Nhận kết quả modem từ `xModemEventQueue`.
- Chờ modem boot 3 giây trước lần kiểm tra AT đầu tiên.
- Kiểm tra lại AT mỗi 2 giây khi modem chưa sẵn sàng.
- Chỉ gọi khi cảnh báo còn hiệu lực, mẫu chưa quá hạn 3 giây, modem sẵn sàng
  và đã hết khoảng cách tối thiểu 60 giây giữa hai lần gọi.
- Ngắt cuộc gọi khi cảnh báo hết hiệu lực hoặc sau tối đa 30 giây.

### MODEM_AT

`MODEM_AT` sở hữu USART2 và A7680C HAL.

- Nhận `AtCommand_t` từ `xAtCmdQueue`.
- Gửi từng lệnh AT theo cơ chế một lệnh đang chờ tại một thời điểm.
- Đọc byte từ ring buffer do USART2 ISR cấp dữ liệu.
- Parse `OK`, `ERROR`, `NO CARRIER`, `BUSY`, `NO ANSWER` và tín hiệu modem reset.
- Gửi `ModemEvent_t` lên `xModemEventQueue`.
- Được đánh thức bởi notification từ USART2 ISR hoặc timeout polling ngắn
  khoảng 20 ms để kiểm tra lệnh mới và timeout lệnh.

## 4. Các queue mới

### `xAtCmdQueue`

Hướng truyền: `CALL -> MODEM_AT`.

Mỗi phần tử gồm loại lệnh, timeout và chuỗi lệnh AT. Các loại hiện tại là:
`AT_CMD_CHECK`, `AT_CMD_DIAL`, `AT_CMD_HANGUP`.

### `xModemEventQueue`

Hướng truyền: `MODEM_AT -> CALL`.

Mỗi phần tử gồm loại sự kiện và loại lệnh liên quan. Các sự kiện hiện tại gồm:
`OK`, kết thúc cuộc gọi, lỗi, modem reset, timeout và lỗi truyền UART.

## 5. Tệp chính

- `app/src/task_alarm.c`: task cảnh báo, giữ nguyên.
- `app/src/task_call.c`: state machine và chính sách gọi.
- `app/src/task_modem_at.c`: thực thi lệnh AT và parse USART2.
- `app/src/app_ipc.c`: tạo các queue ứng dụng.
- `hal/src/hal_a7680c.c`: ring buffer USART2 và notification cho `MODEM_AT`.
- `Core/Src/main.c`: khởi tạo ba task.

Tổng số task ứng dụng sau thay đổi là 3: `ALARM`, `CALL`, `MODEM_AT`. FreeRTOS
vẫn tự tạo thêm Idle task.
