#include <xrt.h>
#include <stdio.h>

int main(void)
{
    uint32 crc = xrtCrc32("123456789", 9);
    if (crc != UINT32_C(0xCBF43926)) return 1;
    uint32 incremental = xrtCrc32Update(0, "1234", 4);
    incremental = xrtCrc32Update(incremental, "56789", 5);
    if (incremental != crc) return 2;
    puts("CRC32 one-shot and incremental check vectors passed");
    return 0;
}
