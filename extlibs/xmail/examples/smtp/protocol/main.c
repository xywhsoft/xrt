#include <stdio.h>
#include <xmail.h>



/* 解析 EHLO 能力并构建一条安全命令。 */
int main(void)
{
	xsmtpcapabilityview Capability;
	uint64 iCapabilities = 0;
	uint64 iSizeLimit = 0;
	char arrCommand[64];
	size_t iSize;

	if ( !xrtSmtpCapabilityParse(
		XRT_STR_LITERAL("SIZE 10485760"),
		&Capability
	) || !xrtSmtpCapabilityAdd(
		&Capability,
		&iCapabilities,
		&iSizeLimit
	) || !xrtSmtpCommandWrite(
		XRT_STR_LITERAL("EHLO"),
		XRT_STR_LITERAL("client.example"),
		arrCommand,
		sizeof(arrCommand),
		&iSize
	) ) {
		return 1;
	}
	printf("SIZE=%llu command=%.*s", (unsigned long long)iSizeLimit,
		(int)iSize, arrCommand);
	return 0;
}
