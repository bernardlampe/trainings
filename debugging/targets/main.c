#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int lab_parse(const uint8_t *data, size_t size);

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    static uint8_t buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    lab_parse(buf, n); return 0;
}
