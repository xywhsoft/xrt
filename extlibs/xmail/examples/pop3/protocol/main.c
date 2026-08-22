#include <stdio.h>
#include <xmail.h>



/* 解析 STAT 并构建 RETR 命令。 */
int main(void)
{
	xpop3stat Stat;
	char arrCommand[32];
	size_t iSize;

	if ( !xrtPop3StatParse(XRT_STR_LITERAL("+OK 12 4096"), &Stat) ||
		 !xrtPop3CommandWrite(
			XRT_STR_LITERAL("RETR"),
			XRT_STR_LITERAL("1"),
			arrCommand,
			sizeof(arrCommand),
			&iSize
		) ) {
		return 1;
	}
	printf("messages=%llu command=%.*s",
		(unsigned long long)Stat.Messages, (int)iSize, arrCommand);
	return 0;
}
