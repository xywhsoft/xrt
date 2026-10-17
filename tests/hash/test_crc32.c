#include "../test.h"

static uint32 reference(uint32 crc, const uint8* data, size_t size)
{
	crc ^= UINT32_MAX;
	for (size_t i=0; i<size; ++i) {
		crc ^= data[i];
		for (unsigned bit=0; bit<8; ++bit)
			crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & (0u-(crc&1u)));
	}
	return crc ^ UINT32_MAX;
}

int main(void)
{
	uint8 data[258];
	for (size_t i=0; i<sizeof(data); ++i) data[i]=(uint8)i;
	testRequire(xrtCrc32(NULL,0)==0,"CRC empty");
	testRequire(xrtCrc32("123456789",9)==UINT32_C(0xCBF43926),"CRC check vector");
	for (size_t offset=0; offset<3; ++offset) {
		for (size_t size=0; size<=256; ++size) {
			uint32 expected=reference(0,data+offset,size);
			testRequire(xrtCrc32(data+offset,size)==expected,"CRC reference/alignment");
			for (size_t split=0; split<=size; ++split) {
				uint32 crc=xrtCrc32(data+offset,split);
				testRequire(xrtCrc32Update(crc,data+offset+split,size-split)==expected,"CRC split");
			}
		}
	}
	testRequire(xrtCrc32Update(UINT32_C(0x12345678),NULL,0)==UINT32_C(0x12345678),"CRC empty update");
	xrtSetErrorKind(XERR_STATE);const xerror* original=xrtGetError();
	testRequire(xrtCrc32("123456789",9)==UINT32_C(0xCBF43926) && xrtGetError()==original,"CRC preserves ambient error");
	xrtClearError();
	(void)xrtCrc32(NULL,1);testRequire(xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"CRC null refusal");xrtClearError();
	(void)xrtCrc32((void*)(uintptr_t)(UINTPTR_MAX-1),4);
	testRequire(xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"CRC range wrap refusal");xrtClearError();
	printf("[PASS] crc32 vectors, all splits, alignment, range and ambient error\n");
	return 0;
}
