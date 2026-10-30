# Chưa có tiêu đề

# Distributed Industrial Safety & Environmental Monitoring System via CAN Bus & 4G LTE Gateway

A real-time, distributed multi-node embedded system designed for industrial environmental monitoring, autonomous hazard mitigation, and cellular telemetry. The system utilizes deterministic **CAN 2.0A bus (500 kbps)** communication across a homogeneous network of **STM32F103C8T6 (ARM Cortex-M3)** microcontrollers scheduled under **FreeRTOS**.

## System Architecture

```
                  ┌──────────────────────────────────────────────┐
                  │          CAN Bus Backbone (500 kbps)         │
                  │  (Differential CANH / CANL + 120Ω Split RT) │
                  └──────┬──────────────────────┬───────────┬────┘
                         │                      │           │
                         ▼                      ▼           ▼
                  ┌──────────────┐      ┌──────────────┐  ┌──────────────┐
                  │    Node 1    │      │    Node 2    │  │    Node 3    │
                  │ (Sensor ECU) │      │(Actuator ECU)│  │  (Gateway)   │
                  │ STM32F103C8T6│      │ STM32F103C8T6│  │ STM32F103C8T6│
                  └──────────────┘      └──────────────┘  └──────┬───────┘
                                                                 │ USART2 (AT)
                                                                 ▼
                                                          ┌──────────────┐
                                                          │ SIMCom A7680C│
                                                          │   (4G LTE)   │
                                                          └──────────────┘
```

The system partitions data acquisition, localized human-machine interface (HMI)/actuation, and telemetry across three dedicated STM32 nodes:

### 1. Node 1: Sensor Acquisition ECU (STM32F103C8T6)

- **Core:** ARM Cortex-M3 @ 72 MHz.
- **Sensing:**
    - **DHT11:** 1-Wire protocol with critical-section microsecond edge capture and FreeRTOS non-blocking wake signals.
    - **MQ-5:** Analog gas sensor sampled via 12-bit ADC with an 8-sample moving average filter.
- **Firmware:** FreeRTOS dual-task architecture (`Task_Sensor` sampling every 500 ms $\rightarrow$ thread-safe `xQueue` $\rightarrow$ `Task_CAN_TX`).
- **CAN Transceiver:** SN65HVD230 / VP230 (3.3V).
- **CAN Frame Output:** `StdId = 0x100`, DLC = 4 (`[Temp, Hum, Gas_Raw_MSB, Gas_Raw_LSB]`).

### 2. Node 2: Local Actuator & Display ECU (STM32F103C8T6)

- **Core:** ARM Cortex-M3 @ 72 MHz.
- **Actuation:** Relay channel driving a 5V exhaust ventilation fan via push-pull GPIO (`PB0`).
- **HMI / Display:** 1.8" ST7735 TFT Display ($128 \times 160$ RGB565) driven by high-speed SPI1 (18 MHz) using bare-metal atomic register writes (`BSRR`) and a custom $7 \times 10$ ASCII font engine.
- **Dual-Tier Control Logic:**
    - **Command Mode:** Listens to Gateway actuation frame `0x200` to toggle relay and refresh sensor telemetry.
    - **Autonomous Edge Fail-Safe:** Directly monitors `0x100` broadcast frames from Node 1. If gas raw ADC exceeds `1500` and Gateway response is missing, the node independently activates the exhaust fan.

### 3. Node 3: Telemetry & LTE Gateway ECU (STM32F103C8T6)

- **Core:** ARM Cortex-M3 @ 72 MHz.
- **CAN Interface:** Native on-chip bxCAN controller with hardware acceptance filtering @ 500 kbps, interfaced via VP230 / SN65HVD230 transceiver.
- **Firmware & RTOS Task Architecture:**
    - **High-Priority Ingestion (`Task_CAN_RX` / CAN RX0 ISR):** Captures incoming `0x100` broadcast frames into an RTOS message queue (`xGatewayQueue`) without frame drops or bus jitter.
    - **Processing & Telemetry Pipeline (`Task_Gateway_Process`):** Consumes queue packets, performs hazard threshold checking (`Gas_Raw > 1500`), dispatches CAN control frames (`0x200`) to Node 2, and handles cellular modem interactions.
- **Cellular Interface:** SIMCom A7680C 4G LTE module interfaced via `USART2` (`PA2`/`PA3` @ 115200 baud) and GPIO for power sequencing (`PWRKEY`). Executes non-blocking AT command parsing for SMS emergency dispatch with rate-limiting / anti-spam logic (60-second cooldown interval).
