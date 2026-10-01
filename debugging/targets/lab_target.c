// Deliberately vulnerable checksum-gated parser used by the fuzzing lab.
//
// Wire format (16..4095 bytes):
//   [0..3]   magic " CKS"        (each byte OR 0x80 checked)
//   [4..5]   payload_len  big-endian
//   [6..7]   checksum     big-endian = sum of payload bytes mod 0x10000
//   [8..]    payload
//
// Training value: the payload_len + checksum + magic gates form exactly
// the "staircase of friction" from the Mutation Fuzzing lecture. Dumb
// mutation rarely penetrates; this is the lab's target rung.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LAB_MAGIC "\x43\x4b\x53"   /* "CKS" */

// returns 0 on success, negative on validation failure
int lab_parse(const uint8_t *data, size_t size)
{
    if (size < 8 || size > 4096)
        return -1;

    /* gate 1: magic */
    if (memcmp(data, LAB_MAGIC, 3) != 0)
        return -2;

    /* gate 2: length field */
    size_t payload_len = (data[4] << 8) | data[5];
    if (payload_len < 1 || payload_len > size - 8)
        return -3;

    /* gate 3: checksum = sum(payload) mod 0x10000 */
    uint32_t sum = 0;
    for (size_t i = 0; i < payload_len; i++)
        sum += data[8 + i];
    uint16_t want = (uint16_t)(sum & 0xFFFF);
    uint16_t got  = (uint16_t)((data[6] << 8) | data[7]);
    if (want != got)
        return -4;

    /* BUG 1: payload copied into a fixed 64-byte stack buffer.
       Correct above only when payload_len <= 64. */
    uint8_t buf[64];
    memcpy(buf, data + 8, payload_len);

    /* BUG 2 (subtle): 'type' is payload[0], compared signed vs char
       -- negative byte 0x80 passes. Then used as a shift. */
    char type = (char)buf[0];
    if (type > 8) {
        return 1;
    }
    volatile uint32_t x = 1u << (type * 4);

    return 0;
}
