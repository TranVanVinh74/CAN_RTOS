# Node2 · Hiển thị và điều khiển quạt

Node2 nhận số đo từ Node1 qua CAN, hiển thị lên màn hình TFT ST7735, điều khiển relay quạt và gửi ngưỡng đang áp dụng cho Node3.

## Node2 có mấy task?

**Node2 có ba task ứng dụng chạy lâu dài là `CONTROL`, `BUTTONS` và `UI`.** Ngoài ra, FreeRTOS tạo `IDLE`, còn `defaultTask` do CubeMX tạo sẽ tự xóa sau lần chạy đầu. Sau khi khởi động xong, có bốn task tồn tại, tính cả `IDLE`.

| Task | Ưu tiên FreeRTOS | Nhiệm vụ |
| --- | ---: | --- |
| `CONTROL` | 3 | Nhận số đo CAN, điều khiển relay, xử lý sự kiện nút, lưu ngưỡng, đồng bộ với Node3 và gửi trạng thái hiển thị. |
| `BUTTONS` | 2 | Quét bốn nút mỗi **20 ms**, chống dội và gửi các lần nhấn đã xác nhận. |
| `UI` | 1 | Nhận trạng thái mới nhất và vẽ TFT theo mốc **40 ms**. |
| `defaultTask` | 3 | Chạy `StartDefaultTask()` rồi tự xóa bằng `vTaskDelete(NULL)`. |
| `IDLE` | 0 | Chạy khi không có task ưu tiên cao hơn sẵn sàng và thu hồi bộ nhớ của task đã tự xóa. |

`main()` khởi tạo phần cứng, tạo queue và các task, rồi gọi `osKernelStart()`. `Core/Src/freertos.c` cung cấp bộ nhớ tĩnh cho `IDLE`. Ưu tiên, stack và các mốc xử lý của task ứng dụng được đặt trong [`app_cfg.h`](app/inc/app_cfg.h). FreeRTOS dùng heap **8 KiB** để cấp phát task và queue.

## Ba task trao đổi dữ liệu thế nào?

```text
Ngắt CAN ── xSensorQueue (5 mẫu) ──┐
                                  ├──> CONTROL ── xUiQueue (1 trạng thái) ──> UI
BUTTONS ── xButtonQueue (8 mục) ───┘       ├──> Relay quạt
                                         └──> CAN 0x200 tới Node3
```

Mỗi mục trong `xButtonQueue` là một mask chứa các lần nhấn đã xác nhận trong cùng một lượt quét. `CONTROL` đọc chúng theo thứ tự. Khi queue đầy, `BUTTONS` chờ có chỗ để gửi. `xUiQueue` luôn giữ bản sao `UiView_t` mới nhất để `UI` đọc ở lần vẽ kế tiếp.

`CONTROL` đọc ngưỡng từ flash khi khởi động, dùng ngưỡng mặc định nếu bản ghi lỗi rồi bật CAN. Ngắt CAN và `BUTTONS` báo cho `CONTROL` bằng task notification sau khi đưa dữ liệu vào queue. Khi chưa có sự kiện, `CONTROL` chờ tối đa **20 ms** để còn kiểm tra lỗi CAN, dữ liệu quá hạn và lịch gửi ngưỡng. Sau **3 giây** không xử lý mẫu mới, màn hình chính có thể hiện `OFFLINE` nếu CAN không báo lỗi.

## Chỉnh và lưu ngưỡng

`SELECT` vào chế độ chỉnh rồi chuyển giữa gas và nhiệt độ. `PLUS` / `MINUS` chỉ đổi bản nháp. `CONTROL` sở hữu cả bản nháp và ngưỡng đang áp dụng. Khi nhận `SAVE`, `CONTROL` chỉ đổi ngưỡng điều khiển sau khi lưu thành công. Nếu ngưỡng đổi và còn số đo mới, task tính lại trạng thái quạt ngay. Nếu lưu lỗi, bản nháp vẫn trên màn hình để chỉnh hoặc thử lưu lại. Trong lúc ghi flash đồng bộ, `CONTROL` có thể phải chờ thao tác ghi hoàn tất trước khi xử lý mẫu CAN tiếp theo.

Node2 phát ngưỡng đang áp dụng với ID `0x200` khi khởi động, khi lưu ngưỡng thay đổi và định kỳ khoảng **1 giây**. Nếu lần trước không nạp được frame vào mailbox, task chờ ít nhất **100 ms** trước khi thử lại.

## Phần nào chạy trong ngắt CAN?

Node1 phát số đo với ID **`0x100`** lên bus CAN. Cả Node2 và Node3 có thể nhận trực tiếp frame này. Trong Node2, luồng từ phần cứng tới task là:

```text
CAN RX FIFO0
  → USB_LP_CAN1_RX0_IRQHandler()
  → HAL_CAN_IRQHandler()
  → HAL_CAN_RxFifo0MsgPendingCallback()
  → on_can_frame()
  → xQueueSendFromISR(xSensorQueue, ...)
  → vTaskNotifyGiveFromISR() báo cho CONTROL
  → CONTROL lấy mẫu bằng xQueueReceive()
```

`on_can_frame()` kiểm tra frame dữ liệu chuẩn có ID `0x100` và 5 byte, giải mã số đo rồi kiểm tra các bit cờ lỗi trước khi đưa mẫu vào queue. Queue chứa tối đa **5 mẫu**. Nếu đầy, mẫu vừa nhận bị bỏ. Callback chạy trong ngắt. Điều khiển relay chạy trong `CONTROL`, còn vẽ màn hình chạy trong `UI`.

## Các file chính

| File | Vai trò |
| --- | --- |
| [`Core/Src/main.c`](Core/Src/main.c) | Khởi tạo phần cứng, tạo task và khởi động FreeRTOS. |
| [`app/src/task_actuator.c`](app/src/task_actuator.c) | Vòng lặp `CONTROL` và callback nhận CAN. |
| [`app/src/task_buttons.c`](app/src/task_buttons.c) | Quét GPIO theo chu kỳ và gửi các sự kiện nút. |
| [`app/src/task_ui.c`](app/src/task_ui.c) | Đọc trạng thái mới nhất và vẽ TFT. |
| [`app/src/app_ipc.c`](app/src/app_ipc.c) | Tạo ba queue cho mẫu, sự kiện nút và trạng thái hiển thị. |
| [`hal/src/hal_can.c`](hal/src/hal_can.c) | Đọc FIFO0 trong callback của HAL và nạp frame truyền vào mailbox. |
| [`hal/src/hal_buttons.c`](hal/src/hal_buttons.c) và [`app/src/app_ui.c`](app/src/app_ui.c) | Chống dội nút, chỉnh ngưỡng nháp và vẽ màn hình. |
| [`app/src/app_settings.c`](app/src/app_settings.c) | Đọc, kiểm tra và lưu ngưỡng trong flash. |
| [`app/src/app_threshold_sync.c`](app/src/app_threshold_sync.c) | Quyết định thời điểm gửi ngưỡng `0x200` cho Node3. |
