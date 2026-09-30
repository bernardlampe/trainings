// binary_re/targets/checks.c --- reversing lab challenge binary
//
// A deliberately small "license key" checker with layered validation,
// for a live static+dynamic reversing session:
//   [0..3]  magic "CHK!"
//   [4]     version nibble must be 2, flags nibble: bit0 results in an
//           anti-debug tripwire path (prints deceiver string)
//   [5]     length byte of the key field
//   [6..]   key bytes, each transformed by an iterated arithmetic decoder
//
// success = all checked, then prints "ACCESS GRANTED".

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static unsigned char decode(unsigned char c, int round)
{
    /* invertible per-byte transform; key bytes flow through here */
    c = (unsigned char)(c - 0x21);
    c = (unsigned char)((c << (round % 5)) | (c >> (8 - (round % 5))));
    c ^= (unsigned char)(0x5a + round);
    return c;
}

int check(const unsigned char *blob, size_t blob_len)
{
    if (blob_len < 7 || blob_len > 256) return -1;
    if (memcmp(blob, "CHK!", 4) != 0)   return -2;

    unsigned char ver  = blob[4] >> 4;
    unsigned char flag = blob[4] & 0xF;

    if (ver != 2) return -3;

    /* flag bit 0: emulate an anti-debug tripwire */
    if (flag & 1) {
        printf("debugger detected, exiting\n");
        return -4;
    }

    unsigned char klen = blob[5];
    if (klen < 2 || (size_t)klen + 6 > blob_len) return -5;

    /* decoded key must read "VR!" first, then sum to a target */
    unsigned char dec[256];
    unsigned sum = 0;
    for (int i = 0; i < klen; i++) {
        dec[i] = decode(blob[6 + i], i);
        sum += dec[i];
    }
    if (memcmp(dec, "VR!", 3) != 0) return -6;
    if (sum != 0x1E1)               return -7;

    printf("ACCESS GRANTED\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: checks <blobfile>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 2; }
    unsigned char blob[512];
    size_t n = fread(blob, 1, sizeof(blob), f);
    fclose(f);
    return (check(blob, n) == 0) ? 0 : 1;
}
