#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件保留 HTTP Client 会话恢复的默认策略和公开统计结构。 */
int main(void)
{
	xhttpclientconfig Client;
	xhttpresumeconfig Resume;
	xhttpresumestats Stats;

	xrtHttpResumeConfigInit(&Resume);
	xrtHttpClientConfigInit(&Client);
	memset(&Stats, 0, sizeof(Stats));
	if ( (Resume.MaxEntries != XHTTP_RESUME_ENTRIES_DEFAULT) ||
		(Resume.MaxEntriesPerOrigin != XHTTP_RESUME_ORIGIN_DEFAULT) ||
		(Client.Resume.MaxEntries != XHTTP_RESUME_ENTRIES_DEFAULT) ||
		(Client.Resume.MaxEntriesPerOrigin != XHTTP_RESUME_ORIGIN_DEFAULT) ) {
		return 1;
	}
	return 0;
}
