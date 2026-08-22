#include <xrt.h>

#include <stdio.h>



/* 生成请求 ID，并展示无分配解析和时间提取路径。 */
int main(void)
{
	xid Value;
	xid Parsed;
	char arrText[XID_TEXT_CAPACITY];
	xtime iTime;

	if ( !xrtXidMake(&Value) ||
		 !xrtXidWrite(&Value, arrText, sizeof(arrText)) ||
		 !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Parsed) ||
		 !xrtXidTime(&Parsed, &iTime) ) {
		return 1;
	}
	printf("XID: %s\n", arrText);
	printf("Unix microseconds: %lld\n", (long long)iTime);
	return 0;
}
