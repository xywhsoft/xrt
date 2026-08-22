#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可独立裁剪 TLS 调度骨架。 */
int main(void)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
		defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		return 0;
	#else
		xtlstranscript Transcript;

		memset(&Transcript, 0xA5, sizeof(Transcript));
		if ( __xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA256) ||
			 __xrtTlsTranscriptInit(&Transcript, XCRYPTO_HASH_SHA256) ) {
			return 1;
		}
		__xrtTlsTranscriptClear(&Transcript);
		return 0;
	#endif
}
