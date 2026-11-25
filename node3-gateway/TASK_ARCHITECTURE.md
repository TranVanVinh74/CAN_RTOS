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

Logic cảnh báo và hysteresis giữ như trước khi tách task.

### CALL

`CALL` sở hữu state machine và chính sách gọi, không truy cập trực tiếp USART2.

- Đọc trạng thái mới nhất bằng `xQueuePeek(xAlarmQueue)`.
- Gửi lệnh `AT`, `ATD...` và `ATH` vào `xAtCmdQueue`.
- Nhận kết quả modem từ `xModemEventQueue`.
- Chờ modem boot 3 giây trước lần kiểm tra AT đầu tiên.
- Kiểm tra lại AT mỗi 2 giây khi modem chưa sẵn sàng.
- Chỉ gọi khi cảnh báo còn hiệu lực, mẫu chưa quá hạn 3 giây, modem sẵn sàng
  và đã hết khoảng cách tối thiểu 60 giây giữa hai lần gọi.
- Ngắt cuộc gọi khi cảnh báo hết hiệu lực hoặc sau tối đa 30 giây, tính từ lúc gửi ATD.
- ATD không nhận được phản hồi trong 5 giây vẫn được coi là có thể đang gọi: giới hạn
  30 giây và ATH vẫn áp dụng. ATD trả `ERROR` hoặc gửi UART lỗi thì gửi ATH ngay.
- Modem báo `ERROR` không kèm lệnh nào trong lúc đang gọi thì gửi ATH.
- Modem reset (`RDY`, `*ATREADY`, `+CPIN: READY`) thì quay về chờ boot 3 giây.
- Nếu không nhận được event kết thúc của lệnh sau timeout của lệnh cộng thêm 1 giây,
  `CALL` coi lệnh đó là timeout, không chờ mãi.

### MODEM_AT

`MODEM_AT` sở hữu USART2 và A7680C HAL.

- Nhận `AtCommand_t` từ `xAtCmdQueue`.
- Gửi từng lệnh AT theo cơ chế một lệnh đang chờ tại một thời điểm.
- Đọc byte từ ring buffer do USART2 ISR cấp dữ liệu.
- Parse `OK`, `ERROR`, `NO CARRIER`, `BUSY`, `NO ANSWER` và tín hiệu modem reset.
- Gửi `ModemEvent_t` lên `xModemEventQueue`. Phản hồi nhận được khi không có lệnh nào
  đang chờ được gắn `AT_CMD_NONE`, để `CALL` không ghép nhầm vào lệnh khác.
- Phân loại hết byte đã nhận trước khi gửi lệnh mới; khi gửi lệnh mới chỉ bỏ các dòng
  đã trọn vẹn, nên một `OK` cũ không khớp nhầm cho lệnh mới.
- Trong 3 giây sau khi thấy modem reset, bỏ mọi lệnh trong `xAtCmdQueue`: các lệnh này
  được gửi trước khi `CALL` biết modem reset, nếu chạy sẽ không được theo dõi.
- Được đánh thức bởi notification từ USART2 ISR hoặc timeout polling ngắn
  khoảng 20 ms để kiểm tra lệnh mới và timeout lệnh.

## 4. Các queue mới

### `xAtCmdQueue`

Hướng truyền: `CALL -> MODEM_AT`.

Mỗi phần tử gồm loại lệnh, timeout và chuỗi lệnh AT. Các loại hiện tại là:
`AT_CMD_CHECK`, `AT_CMD_DIAL`, `AT_CMD_HANGUP`. `AT_CMD_NONE` chỉ xuất hiện trong
event, đánh dấu phản hồi không thuộc lệnh nào.

### `xModemEventQueue`

Hướng truyền: `MODEM_AT -> CALL`.

Mỗi phần tử gồm loại sự kiện và loại lệnh liên quan. Các sự kiện hiện tại gồm:
`OK`, kết thúc cuộc gọi, lỗi, modem reset, timeout và lỗi truyền UART.

## 5. Tệp chính

- `app/src/task_alarm.c`: task cảnh báo.
- `app/src/task_call.c`: state machine và chính sách gọi.
- `app/src/task_modem_at.c`: thực thi lệnh AT và parse USART2.
- `app/src/app_ipc.c`: tạo các queue ứng dụng.
- `hal/src/hal_a7680c.c`: ring buffer USART2 và notification cho `MODEM_AT`.
- `Core/Src/main.c`: khởi tạo ba task.

Tổng số task ứng dụng sau thay đổi là 3: `ALARM`, `CALL`, `MODEM_AT`. FreeRTOS
vẫn tự tạo thêm Idle task.
