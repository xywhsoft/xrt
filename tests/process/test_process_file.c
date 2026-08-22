#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#endif



/* 子进程模式原样复制 stdin 到 stdout。 */
static int testProcessFileChild(int argc, char** argv)
{
	unsigned char pData[256];
	size_t iRead;

	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdin), _O_BINARY);
		(void)_setmode(_fileno(stdout), _O_BINARY);
	#endif
	if ( (argc < 2) || (strcmp(argv[1], "--xrt-process-file-child") != 0) ) {
		return -1;
	}
	while ( (iRead = fread(pData, 1u, sizeof(pData), stdin)) != 0u ) {
		if ( fwrite(pData, 1u, iRead, stdout) != iRead ) {
			return 91;
		}
	}
	fflush(stdout);
	return ferror(stdin) ? 92 : 0;
}



/* 读取 Process 的小型 stdout 管道到零结尾缓冲。 */
static size_t testProcessFileRead(
	xprocess* pProcess,
	char* pData,
	size_t iCapacity
)
{
	size_t iSize = 0u;

	while ( iSize < (iCapacity - 1u) ) {
		int64 iRead = xrtProcessRead(
			pProcess,
			XPROCESS_STDOUT,
			pData + iSize,
			iCapacity - iSize - 1u
		);

		testRequire(iRead >= 0, "process file stdout read failed");
		if ( iRead == 0 ) {
			break;
		}
		iSize += (size_t)iRead;
	}
	pData[iSize] = 0;
	return iSize;
}



/* 验证文件句柄在 Spawn 后可由调用方立即关闭。 */
int main(int argc, char** argv)
{
	static const char sInput[] = "file redirected input\n";
	static const cstr sInputPath = "xrt-process-file-input.tmp";
	static const cstr sOutputPath = "xrt-process-file-output.tmp";
	const cstr pArgs[] = { "--xrt-process-file-child" };
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	xprocessio Io;
	xfile File;
	char pOutput[128];
	size_t iRead;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-file-child") == 0) ) {
		return testProcessFileChild(argc, argv);
	}

	File = xrtOpen(
		sInputPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);
	testRequire(File != NULL, "process input file create failed");
	testRequire(
		xrtWriteFull(File, sInput, sizeof(sInput) - 1u, NULL),
		"process input file write failed"
	);
	testRequire(xrtClose(File), "process input file close failed");

	File = xrtOpen(sInputPath, XFILE_READ);
	testRequire(File != NULL, "process input file open failed");
	testRequire(xrtProcessConfigInit(&Config), "process file config init failed");
	Config.Program = argv[0];
	Config.Args = pArgs;
	Config.ArgCount = 1u;
	Config.Stdin = xrtProcessFile(File);
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	testRequire(pProcess != NULL, "process input redirect spawn failed");
	testRequire(xrtClose(File), "borrowed input file close failed");
	iRead = testProcessFileRead(pProcess, pOutput, sizeof(pOutput));
	testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "input redirect wait failed");
	testRequire(
		(iRead == (sizeof(sInput) - 1u)) &&
		(memcmp(pOutput, sInput, sizeof(sInput) - 1u) == 0),
		"input redirect result mismatch"
	);
	xrtProcessDestroy(pProcess);

	File = xrtOpen(
		sOutputPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);
	testRequire(File != NULL, "process output file create failed");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(
			xrtProcessShellConfigInit(&Config, "echo file-output"),
			"process output shell config failed"
		);
	#else
		testRequire(
			xrtProcessShellConfigInit(&Config, "printf file-output"),
			"process output shell config failed"
		);
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout = xrtProcessFile(File);
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	testRequire(pProcess != NULL, "process output redirect spawn failed");
	testRequire(xrtClose(File), "borrowed output file close failed");
	testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "output redirect wait failed");
	testRequire(xrtProcessStatus(pProcess, &Status), "output redirect status failed");
	testRequire(
		(Status.Kind == XPROCESS_EXIT_CODE) && (Status.Code == 0),
		"output redirect exit mismatch"
	);
	xrtProcessDestroy(pProcess);

	File = xrtOpen(sOutputPath, XFILE_READ);
	testRequire(File != NULL, "process output file open failed");
	testRequire(
		xrtRead(File, pOutput, sizeof(pOutput) - 1u, &iRead),
		"process output file read failed"
	);
	pOutput[iRead] = 0;
	testRequire(
		(iRead >= 11u) && (memcmp(pOutput, "file-output", 11u) == 0),
		"output redirect content mismatch"
	);
	testRequire(xrtClose(File), "process output file close failed");

	Io = xrtProcessFile(NULL);
	testRequire(
		(Io.Mode == XPROCESS_IO_HANDLE) && (Io.Handle == -1),
		"invalid process file mapping mismatch"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_ARGUMENT,
		"invalid process file error mismatch"
	);
	testRequire(xrtFileDelete(sInputPath), "process input file delete failed");
	testRequire(xrtFileDelete(sOutputPath), "process output file delete failed");
	return 0;
}
