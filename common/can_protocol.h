/**
 * @file can_protocol.h
 * @brief Shared CAN identifiers and byte-level sensor and threshold codecs.
 */

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

/**
 * @brief Encode a five-byte sensor payload with the gas high byte first.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] humidity_pct Relative humidity in whole percent.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @param[out] data Output payload buffer with the protocol-defined DLC capacity.
 */
static inline void can_pack_sensor(uint8_t temperature_c, uint8_t humidity_pct, uint16_t gas_raw,
                                   uint8_t faults, uint8_t data[CAN_DLC_SENSOR]) {
    data[CAN_SENSOR_BYTE_TEMPERATURE] = temperature_c;
    data[CAN_SENSOR_BYTE_HUMIDITY] = humidity_pct;
    data[CAN_SENSOR_BYTE_GAS_HIGH] = (uint8_t)(gas_raw >> CAN_SENSOR_BYTE_BITS);
    data[CAN_SENSOR_BYTE_GAS_LOW] = (uint8_t)(gas_raw & CAN_SENSOR_BYTE_MASK);
    data[CAN_SENSOR_BYTE_FAULTS] = faults;
}

/**
 * @brief Decode a five-byte sensor payload; the caller validates frame metadata and fault bits.
 * @param[in] data Input payload buffer containing at least the protocol-defined DLC bytes.
 * @param[out] temperature_c Output pointer receiving the decoded temperature in degrees Celsius.
 * @param[out] humidity_pct Output pointer receiving the decoded relative humidity in percent.
 * @param[out] gas_raw Output pointer receiving the decoded raw gas ADC value.
 * @param[out] faults Output pointer receiving the decoded sensor fault flags.
 */
static inline void can_unpack_sensor(const uint8_t data[CAN_DLC_SENSOR], uint8_t *temperature_c,
                                     uint8_t *humidity_pct, uint16_t *gas_raw, uint8_t *faults) {
    *faults = data[CAN_SENSOR_BYTE_FAULTS];
    *temperature_c = data[CAN_SENSOR_BYTE_TEMPERATURE];
    *humidity_pct = data[CAN_SENSOR_BYTE_HUMIDITY];
    *gas_raw = (uint16_t)(((uint16_t)data[CAN_SENSOR_BYTE_GAS_HIGH] << CAN_SENSOR_BYTE_BITS) |
                          data[CAN_SENSOR_BYTE_GAS_LOW]);
}

/**
 * @brief Encode gas and temperature thresholds into a three-byte payload with the gas high byte first.
 * @param[in] gas_limit_raw Gas activation threshold in raw ADC units.
 * @param[in] temperature_limit_c Temperature activation threshold in degrees Celsius.
 * @param[out] data Output payload buffer with the protocol-defined DLC capacity.
 */
static inline void can_pack_thresholds(uint16_t gas_limit_raw, uint8_t temperature_limit_c,
                                       uint8_t data[CAN_DLC_THRESHOLDS]) {
    data[CAN_THRESHOLDS_BYTE_GAS_HIGH] = (uint8_t)(gas_limit_raw >> CAN_SENSOR_BYTE_BITS);
    data[CAN_THRESHOLDS_BYTE_GAS_LOW] = (uint8_t)(gas_limit_raw & CAN_SENSOR_BYTE_MASK);
    data[CAN_THRESHOLDS_BYTE_TEMPERATURE] = temperature_limit_c;
}

/**
 * @brief Decode a three-byte threshold payload; the caller validates the decoded ranges.
 * @param[in] data Input payload buffer containing at least the protocol-defined DLC bytes.
 * @param[out] gas_limit_raw Output pointer receiving the decoded gas activation threshold in ADC units.
 * @param[out] temperature_limit_c Output pointer receiving the decoded temperature activation threshold in degrees Celsius.
 */
static inline void can_unpack_thresholds(const uint8_t data[CAN_DLC_THRESHOLDS],
                                         uint16_t *gas_limit_raw, uint8_t *temperature_limit_c) {
    *gas_limit_raw = (uint16_t)(((uint16_t)data[CAN_THRESHOLDS_BYTE_GAS_HIGH] <<
                               CAN_SENSOR_BYTE_BITS) | data[CAN_THRESHOLDS_BYTE_GAS_LOW]);
    *temperature_limit_c = data[CAN_THRESHOLDS_BYTE_TEMPERATURE];
}

#endif
