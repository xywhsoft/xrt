#include <stdio.h>
#include <xmail.h>



/* 输出当前 UTC 邮件日期并立即回读。 */
int main(void)
{
	char arrDate[64];
	xtime iParsed;
	int iOffset;
	size_t iSize;
	xstrview Text;

	if ( !xrtMailDateWrite(
		xrtNow(),
		0,
		arrDate,
		sizeof(arrDate),
		&iSize
	) ) {
		return 1;
	}
	Text.Data = arrDate;
	Text.Size = iSize;
	if ( !xrtMailDateParse(
		Text,
		&iParsed,
		&iOffset
	) ) {
		return 2;
	}
	printf("%s\n", arrDate);
	return 0;
}
