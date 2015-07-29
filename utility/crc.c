#include "crc.h"

void crc8_calculate_byte_streaming(const uint8_t data, uint8_t *crc) {
  // Optimal polynomial chosen based on
  // http://users.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
  // Note that this is different than the standard CRC-8 polynomial, because the
  // standard CRC-8 polynomial is not particularly good.

  // nibble lookup table for (x^8 + x^5 + x^3 + x^2 + x + 1)
  static const uint8_t lookup_table[] =
      { 0, 47, 94, 113, 188, 147, 226, 205, 87, 120, 9, 38, 235, 196,
        181, 154 };

  int i;
  for (i = 2; i > 0; i--) {
    uint8_t nibble = data;
    if (i % 2 == 0) {
      nibble >>= 4;
    }
    int index = nibble ^ (*crc >> 4);
    *crc = lookup_table[index & 0xf] ^ (*crc << 4);
  }
}
