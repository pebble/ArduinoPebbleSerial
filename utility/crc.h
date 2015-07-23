#pragma once

#include <stdint.h>

void crc8_calculate_bytes_streaming(const uint8_t *data, unsigned int data_len, uint8_t *crc);
