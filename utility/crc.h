#pragma once

#include <stdint.h>

void crc8_calculate_byte_streaming(const uint8_t data, uint8_t *crc);
