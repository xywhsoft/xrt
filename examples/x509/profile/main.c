#include <stdio.h>

#include <xrt.h>



/* 演示零分配遍历独立 GeneralNames DER。 */
int main(void)
{
	static const uint8 GeneralNames[] = {
		0x30, 0x18,
		0x82, 0x10, 'a', 'p', 'i', '.', 'e', 'x', 'a', 'm',
		'p', 'l', 'e', '.', 't', 'e', 's', 't',
		0x87, 0x04, 127, 0, 0, 1
	};
	xx509gencursor Cursor;
	xx509genname Name;
	xx509result Result;

	if ( !xrtX509GeneralNameInit(
		(xbytesview) { GeneralNames, sizeof(GeneralNames) }, &Cursor
	) ) {
		return 1;
	}
	while ( (Result = xrtX509GeneralNameRead(
		&Cursor, &Name
	)) == X509_VALUE ) {
		printf("type=%d size=%zu\n", (int)Name.Type, Name.Value.Size);
	}
	return (Result == X509_DONE) ? 0 : 1;
}
