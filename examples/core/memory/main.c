#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示拥有式分配、调整、复制和敏感内存清理。 */
int main(void)
{
	static const unsigned char Source[] = { 1, 2, 3, 4 };
	unsigned char* pValues;
	unsigned char* pCopy;

	pValues = (unsigned char*)xrtCalloc(4, sizeof(unsigned char));
	if ( pValues == NULL ) {
		return 1;
	}
	pValues[0] = 42;
	pValues = (unsigned char*)xrtRealloc(pValues, 16);
	if ( pValues == NULL ) {
		return 2;
	}

	pCopy = (unsigned char*)xrtMemDup(Source, sizeof(Source));
	if ( pCopy == NULL ) {
		xrtFree(pValues);
		return 3;
	}
	printf(
		"value=%u copied=%s\n",
		(unsigned int)pValues[0],
		memcmp(pCopy, Source, sizeof(Source)) == 0 ? "yes" : "no"
	);

	xrtSecureZero(pValues, 16);
	xrtSecureZero(pCopy, sizeof(Source));
	xrtFree(pCopy);
	xrtFree(pValues);
	return 0;
}
