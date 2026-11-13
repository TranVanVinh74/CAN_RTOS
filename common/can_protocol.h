#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

#define CAN_ID_SENSOR 0x100U
#define CAN_DLC_SENSOR 5U
#define CAN_ID_THRESHOLDS 0x200U
#define CAN_DLC_THRESHOLDS 3U
#define CAN_SENSOR_BYTE_BITS 8U
#define CAN_SENSOR_BYTE_MASK 0xFFU

#define CAN_SENSOR_BYTE_TEMPERATURE 0U
#define CAN_SENSOR_BYTE_HUMIDITY 1U
#define CAN_SENSOR_BYTE_GAS_HIGH 2U
#define CAN_SENSOR_BYTE_GAS_LOW 3U
#define CAN_SENSOR_BYTE_FAULTS 4U
#define CAN_THRESHOLDS_BYTE_GAS_HIGH 0U
#define CAN_THRESHOLDS_BYTE_GAS_LOW 1U
#define CAN_THRESHOLDS_BYTE_TEMPERATURE 2U

static inline void can_pack_sensor(uint8_t temperature_c, uint8_t humidity_pct, uint16_t gas_raw,
                                   uint8_t faults, uint8_t data[CAN_DLC_SENSOR]) {
    data[CAN_SENSOR_BYTE_TEMPERATURE] = temperature_c;
    data[CAN_SENSOR_BYTE_HUMIDITY] = humidity_pct;
    data[CAN_SENSOR_BYTE_GAS_HIGH] = (uint8_t)(gas_raw >> CAN_SENSOR_BYTE_BITS);
    data[CAN_SENSOR_BYTE_GAS_LOW] = (uint8_t)(gas_raw & CAN_SENSOR_BYTE_MASK);
    data[CAN_SENSOR_BYTE_FAULTS] = faults;
}

static inline void can_unpack_sensor(const uint8_t data[CAN_DLC_SENSOR], uint8_t *temperature_c,
                                     uint8_t *humidity_pct, uint16_t *gas_raw, uint8_t *faults) {
    *faults = data[CAN_SENSOR_BYTE_FAULTS];
    *temperature_c = data[CAN_SENSOR_BYTE_TEMPERATURE];
    *humidity_pct = data[CAN_SENSOR_BYTE_HUMIDITY];
    *gas_raw = (uint16_t)(((uint16_t)data[CAN_SENSOR_BYTE_GAS_HIGH] << CAN_SENSOR_BYTE_BITS) |
                          data[CAN_SENSOR_BYTE_GAS_LOW]);
}

static inline void can_pack_thresholds(uint16_t gas_limit_raw, uint8_t temperature_limit_c,
                                       uint8_t data[CAN_DLC_THRESHOLDS]) {
    data[CAN_THRESHOLDS_BYTE_GAS_HIGH] = (uint8_t)(gas_limit_raw >> CAN_SENSOR_BYTE_BITS);
    data[CAN_THRESHOLDS_BYTE_GAS_LOW] = (uint8_t)(gas_limit_raw & CAN_SENSOR_BYTE_MASK);
    data[CAN_THRESHOLDS_BYTE_TEMPERATURE] = temperature_limit_c;
}

static inline void can_unpack_thresholds(const uint8_t data[CAN_DLC_THRESHOLDS],
                                         uint16_t *gas_limit_raw, uint8_t *temperature_limit_c) {
    *gas_limit_raw = (uint16_t)(((uint16_t)data[CAN_THRESHOLDS_BYTE_GAS_HIGH] <<
                               CAN_SENSOR_BYTE_BITS) | data[CAN_THRESHOLDS_BYTE_GAS_LOW]);
    *temperature_limit_c = data[CAN_THRESHOLDS_BYTE_TEMPERATURE];
}

#endif
