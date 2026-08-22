#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_WIRE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* SSH wire 单头只能闭合 XRT core。 */
int main(void)
{
	unsigned char arrWire[32];
	xsshwriter Writer;
	xsshreader Reader;
	uint32 iValue;

	#if !defined(XSSH_FEATURE_WIRE)
		#error "XSSH_MODULE_SSH_WIRE dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_STRING) || defined(XRT_FEATURE_NETWORK) || \
		defined(XRT_FEATURE_CRYPTO)
		#error "ssh wire unexpectedly enabled unrelated features"
	#endif

	if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshWriteU32(&Writer, UINT32_C(0x12345678)) != XSSH_OK) ||
		!xrtSshReaderInit(&Reader, (xbytesview){ arrWire, Writer.Size }) ||
		(xrtSshReadU32(&Reader, &iValue) != XSSH_OK) ||
		(iValue != UINT32_C(0x12345678)) ) {
		return 1;
	}
	Writer.Size = 0u;
	return (xrtSshBannerWrite(
		&Writer,
		XRT_STR_LITERAL("SSH-2.0-xssh")
	) == XSSH_OK) &&
		(Writer.Size == sizeof("SSH-2.0-xssh\r\n") - 1u) ? 0 : 2;
}
