#include "../test.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
	#include <sys/select.h>
	#include <unistd.h>
#endif



#if !defined(_WIN32) && !defined(_WIN64)

/* 关闭为抬高描述符编号而暂时占用的全部文件。 */
static void testSelectCloseFiles(int* pFiles, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		(void)close(pFiles[i]);
	}
}



/* 内部唤醒描述符超出 fd_set 时必须在创建阶段拒绝，不能留到 Wait 越界。 */
int main(void)
{
	int Files[FD_SETSIZE + 8];
	size_t iCount = 0;
	xnetportconfig Config;
	xnetport* pPort;

	while ( iCount < (sizeof(Files) / sizeof(Files[0])) ) {
		int iFile = open("/dev/null", O_RDONLY);

		if ( iFile < 0 ) {
			break;
		}
		Files[iCount++] = iFile;
		if ( iFile >= (FD_SETSIZE - 1) ) {
			break;
		}
	}
	if ( (iCount == 0) || (Files[iCount - 1] < (FD_SETSIZE - 1)) ) {
		testSelectCloseFiles(Files, iCount);
		return 0;
	}

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pPort = xrtNetPortCreate(&Config);
	if ( pPort != NULL ) {
		(void)xrtNetPortDestroy(pPort);
	}
	testRequire((pPort == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CREATE),
		"select accepted an unrepresentable wake descriptor");
	xrtClearError();
	testSelectCloseFiles(Files, iCount);
	return 0;
}

#else

/* Windows fd_set 按 Socket 数量计数，不存在 POSIX 整数描述符越界。 */
int main(void)
{
	testRequire(true, "Windows select fd_set contract");
	return 0;
}

#endif
