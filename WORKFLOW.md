# Quy trinh lam viec

- develop: khung 3 node, nhan cac PR da review va test.
- setup/project-skeleton: ban khung khoi tao.
- feature/hal-timer-adc: DHT11/MQ5 Node1.
- feature/hal-i2c-spi: TFT/FONT Node2.
- feature/hal-gpio-uart: A7680C Node3.
- backup/full-code: code day du lay tu lich su Git trong ZIP nguon.

Moi nhanh chua ca 3 node. Nhanh feature chi them driver tuong ung; main.c van la khung, chua goi cam bien/task ung dung. Them code va test tren nhanh feature, commit/push, tao PR vao develop. Test tich hop truoc khi PR develop vao main.

Da kiem tra duong dan va tien xu ly C; chua build/link bang Keil ARM Compiler 5 va chua test board.
