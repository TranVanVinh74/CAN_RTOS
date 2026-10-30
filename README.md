# Chưa có tiêu đề

# Distributed Industrial Safety & Environmental Monitoring System via CAN Bus & 4G LTE Gateway

A real-time, distributed multi-node embedded system designed for industrial environmental monitoring, autonomous hazard mitigation, and cellular telemetry. The system utilizes deterministic **CAN 2.0A bus (500 kbps)** communication across STM32 microcontrollers and an ESP32 dual-core gateway with **FreeRTOS** task scheduling.

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
                  │ STM32F103C8T6│      │ STM32F103C8T6│  │ ESP32 DevKit │
                  └──────────────┘      └──────────────┘  └──────┬───────┘
                                                                 │ UART (AT)
                                                                 ▼
                                                          ┌──────────────┐
                                                          │ SIMCom A7680C│
                                                          │   (4G LTE)   │
                                                          └──────────────┘
```

The system partitions data acquisition, localized human-machine interface (HMI)/actuation, and telemetry across three dedicated nodes:

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

### 3. Node 3: Telemetry & LTE Gateway (ESP32 DevKit)

- **Core:** Dual-Core Xtensa 32-bit LX6 @ 240 MHz.
- **Dual-Core FreeRTOS Partitioning:**
    - **Core 0 (`Task_CAN_RX`):** Dedicated to the native TWAI/CAN peripheral @ 500 kbps, parsing incoming `0x100` telemetry frames without frame dropping.
    - **Core 1 (`Task_Gateway_Process`):** Evaluates hazard conditions, synchronizes states, dispatches control frames (`0x200`), and interfaces with the cellular modem.
- **Cellular Interface:** SIMCom A7680C 4G LTE module over UART2 (`115200 baud`). Executes SMS emergency dispatch with rate-limiting / anti-spam logic (60-second cooldown interval).
