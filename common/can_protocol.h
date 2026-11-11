#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

#define CAN_ID_SENSOR 0x100U
#define CAN_DLC_SENSOR 4U

#define CAN_SENSOR_BYTE_TEMPERATURE 0U
#define CAN_SENSOR_BYTE_HUMIDITY 1U
#define CAN_SENSOR_BYTE_GAS_HIGH 2U
#define CAN_SENSOR_BYTE_GAS_LOW 3U

static inline void can_pack_sensor(uint8_t temperature_c, uint8_t humidity_pct, uint16_t gas_raw,
                                   uint8_t data[CAN_DLC_SENSOR]) {
    data[CAN_SENSOR_BYTE_TEMPERATURE] = temperature_c;
    data[CAN_SENSOR_BYTE_HUMIDITY] = humidity_pct;
    data[CAN_SENSOR_BYTE_GAS_HIGH] = (uint8_t)(gas_raw >> 8);
    data[CAN_SENSOR_BYTE_GAS_LOW] = (uint8_t)(gas_raw & 0xFFU);
}

static inline void can_unpack_sensor(const uint8_t data[CAN_DLC_SENSOR], uint8_t *temperature_c,
                                     uint8_t *humidity_pct, uint16_t *gas_raw) {
    *temperature_c = data[CAN_SENSOR_BYTE_TEMPERATURE];
    *humidity_pct = data[CAN_SENSOR_BYTE_HUMIDITY];
    *gas_raw = (uint16_t)(((uint16_t)data[CAN_SENSOR_BYTE_GAS_HIGH] << 8) |
                          data[CAN_SENSOR_BYTE_GAS_LOW]);
}

#endif
