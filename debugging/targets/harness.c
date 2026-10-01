#include <stdint.h>
#include <stdlib.h>

int lab_parse(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    lab_parse(data, size);
    return 0;
}
