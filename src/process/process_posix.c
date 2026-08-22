#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS) && \
	!defined(_WIN32) && !defined(_WIN64)

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>



extern char** environ;

#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* 严格语言模式可能隐藏这些 POSIX.1 终端入口，声明保持标准 ABI。 */
extern int posix_openpt(int iFlags);
extern int grantpt(int iFd);
extern int unlockpt(int iFd);
extern char* ptsname(int iFd);

	#if defined(__linux__) || defined(__ANDROID__)
extern int ptsname_r(int iFd, char* sName, size_t iSize);
	#endif
#endif



/* 子进程只通过 close-on-exec 管道返回启动阶段和 errno。 */
typedef struct xprocesschilderror {
	xprocesserror Stage;
	int SystemCode;
} xprocesschilderror;



/* POSIX 启动计划全部在 fork 前构建，子进程不调用内存分配器。 */
typedef struct xprocessposixplan {
	char** Argv;
	char** Env;
	char* Program;
} xprocessposixplan;



/* 设置 fd 的 close-on-exec 标志。 */
static bool __xrtProcessFdCloseExec(int iFd)
{
	int iFlags = fcntl(iFd, F_GETFD);

	return (iFlags >= 0) &&
		(fcntl(iFd, F_SETFD, iFlags | FD_CLOEXEC) == 0);
}



/* 复制借用 fd，并让 XRT 持有的副本避开标准流编号。 */
static bool __xrtProcessFdDuplicate(int iSource, int* pTarget)
{
	int iCopy;

	#if defined(F_DUPFD_CLOEXEC)
		iCopy = fcntl(iSource, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
	#else
		iCopy = fcntl(iSource, F_DUPFD, STDERR_FILENO + 1);
		if ( (iCopy >= 0) && !__xrtProcessFdCloseExec(iCopy) ) {
			int iError = errno;

			(void)close(iCopy);
			errno = iError;
			return false;
		}
	#endif
	if ( iCopy < 0 ) {
		return false;
	}
	*pTarget = iCopy;
	return true;
}



/* 将 XRT 新建的 fd 提升到标准流之外，消除 dup2 重定向别名。 */
static bool __xrtProcessFdOwn(int* pFd)
{
	int iCopy;

	if ( *pFd < 0 ) {
		errno = EBADF;
		return false;
	}
	if ( *pFd > STDERR_FILENO ) {
		return __xrtProcessFdCloseExec(*pFd);
	}
	if ( !__xrtProcessFdDuplicate(*pFd, &iCopy) ) {
		return false;
	}
	(void)close(*pFd);
	*pFd = iCopy;
	return true;
}



/* 创建两端都带 close-on-exec 的普通管道。 */
static bool __xrtProcessPipe(int pPipe[2])
{
	pPipe[0] = -1;
	pPipe[1] = -1;
	if ( pipe(pPipe) != 0 ) {
		return false;
	}
	if ( !__xrtProcessFdOwn(&pPipe[0]) ||
		!__xrtProcessFdOwn(&pPipe[1]) ) {
		int iError = errno;

		(void)close(pPipe[0]);
		(void)close(pPipe[1]);
		pPipe[0] = -1;
		pPipe[1] = -1;
		errno = iError;
		return false;
	}
	return true;
}



/* stdin 使用 socketpair，使写端可以通过 MSG_NOSIGNAL 避免终止父进程。 */
static bool __xrtProcessInputPipe(int pPipe[2])
{
	pPipe[0] = -1;
	pPipe[1] = -1;
	if ( socketpair(AF_UNIX, SOCK_STREAM, 0, pPipe) != 0 ) {
		return false;
	}
	if ( !__xrtProcessFdOwn(&pPipe[0]) ||
		!__xrtProcessFdOwn(&pPipe[1]) ) {
		int iError = errno;

		(void)close(pPipe[0]);
		(void)close(pPipe[1]);
		pPipe[0] = -1;
		pPipe[1] = -1;
		errno = iError;
		return false;
	}
	#if defined(SO_NOSIGPIPE)
		{
			int iOne = 1;

			(void)setsockopt(
				pPipe[1],
				SOL_SOCKET,
				SO_NOSIGPIPE,
				&iOne,
				sizeof(iOne)
			);
		}
	#endif
	return true;
}



/* 释放深拷贝的 NULL 结尾字符串数组。 */
static void __xrtProcessStringsFree(char** pStrings)
{
	if ( pStrings == NULL ) {
		return;
	}
	for ( size_t i = 0u; pStrings[i] != NULL; i++ ) {
		xrtFree(pStrings[i]);
	}
	xrtFree(pStrings);
}



/* 复制一个零结尾文本。 */
static char* __xrtProcessTextCopy(cstr sText)
{
	size_t iSize = strlen(sText);
	char* sCopy;

	if ( iSize == SIZE_MAX ) {
		return NULL;
	}
	sCopy = (char*)xrtMalloc(iSize + 1u);
	if ( sCopy != NULL ) {
		memcpy(sCopy, sText, iSize + 1u);
	}
	return sCopy;
}



/* 返回环境项名称长度。 */
static size_t __xrtProcessEnvNameSize(cstr sEntry)
{
	const char* sEquals = strchr(sEntry, '=');

	return sEquals != NULL ? (size_t)(sEquals - sEntry) : strlen(sEntry);
}



/* POSIX 环境变量名称按字节区分大小写。 */
static bool __xrtProcessEnvNameEqual(cstr sEntry, cstr sName)
{
	size_t iEntry = __xrtProcessEnvNameSize(sEntry);
	size_t iName = strlen(sName);

	return (iEntry == iName) && (memcmp(sEntry, sName, iName) == 0);
}



/* 判断父环境项是否被配置覆盖或删除。 */
static bool __xrtProcessEnvOverridden(
	cstr sEntry,
	const xprocessenv* pEnv,
	size_t iCount
)
{
	for ( size_t i = 0u; i < iCount; i++ ) {
		if ( __xrtProcessEnvNameEqual(sEntry, pEnv[i].Name) ) {
			return true;
		}
	}
	return false;
}



/* 判断同名配置是否在后面再次出现。 */
static bool __xrtProcessEnvHasLater(
	const xprocessenv* pEnv,
	size_t iCount,
	size_t iIndex
)
{
	for ( size_t i = iIndex + 1u; i < iCount; i++ ) {
		if ( strcmp(pEnv[iIndex].Name, pEnv[i].Name) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 合成一个 Name=Value 环境项。 */
static char* __xrtProcessEnvPair(const xprocessenv* pPair)
{
	size_t iName = strlen(pPair->Name);
	size_t iValue = strlen(pPair->Value);
	char* sEntry;

	if ( iName > (SIZE_MAX - iValue - 2u) ) {
		return NULL;
	}
	sEntry = (char*)xrtMalloc(iName + iValue + 2u);
	if ( sEntry != NULL ) {
		memcpy(sEntry, pPair->Name, iName);
		sEntry[iName] = '=';
		memcpy(sEntry + iName + 1u, pPair->Value, iValue + 1u);
	}
	return sEntry;
}



/* 深拷贝并应用环境覆盖，使 fork 子进程不触碰 libc 环境状态。 */
static char** __xrtProcessEnvironmentBuild(const xprocessconfig* pConfig)
{
	size_t iParentCount = 0u;
	size_t iOutputCount = 0u;
	char** pEnv;

	if ( pConfig->InheritEnv ) {
		while ( environ[iParentCount] != NULL ) {
			iParentCount++;
		}
	}
	if ( iParentCount > (SIZE_MAX - pConfig->EnvCount - 1u) ) {
		return NULL;
	}
	pEnv = (char**)xrtCalloc(
		iParentCount + pConfig->EnvCount + 1u,
		sizeof(char*)
	);
	if ( pEnv == NULL ) {
		return NULL;
	}
	for ( size_t i = 0u; i < iParentCount; i++ ) {
		if ( __xrtProcessEnvOverridden(
			environ[i],
			pConfig->Env,
			pConfig->EnvCount
		) ) {
			continue;
		}
		pEnv[iOutputCount] = __xrtProcessTextCopy(environ[i]);
		if ( pEnv[iOutputCount] == NULL ) {
			__xrtProcessStringsFree(pEnv);
			return NULL;
		}
		iOutputCount++;
	}
	for ( size_t i = 0u; i < pConfig->EnvCount; i++ ) {
		if ( (pConfig->Env[i].Value == NULL) ||
			__xrtProcessEnvHasLater(pConfig->Env, pConfig->EnvCount, i) ) {
			continue;
		}
		pEnv[iOutputCount] = __xrtProcessEnvPair(&pConfig->Env[i]);
		if ( pEnv[iOutputCount] == NULL ) {
			__xrtProcessStringsFree(pEnv);
			return NULL;
		}
		iOutputCount++;
	}
	return pEnv;
}



/* 从预构建环境中读取变量值。 */
static cstr __xrtProcessEnvironmentGet(char* const* pEnv, cstr sName)
{
	for ( size_t i = 0u; pEnv[i] != NULL; i++ ) {
		if ( __xrtProcessEnvNameEqual(pEnv[i], sName) ) {
			return pEnv[i] + strlen(sName) + 1u;
		}
	}
	return NULL;
}



/* 在父进程解析 PATH，避免 fork 后调用可能分配内存的 execvp。 */
static char* __xrtProcessProgramResolve(cstr sProgram, char* const* pEnv)
{
	cstr sPath;
	const char* pStart;

	if ( strchr(sProgram, '/') != NULL ) {
		return __xrtProcessTextCopy(sProgram);
	}
	sPath = __xrtProcessEnvironmentGet(pEnv, "PATH");
	if ( sPath == NULL ) {
		sPath = "/bin:/usr/bin";
	}
	pStart = sPath;
	while ( true ) {
		const char* pEnd = strchr(pStart, ':');
		size_t iDirectory = pEnd != NULL ?
			(size_t)(pEnd - pStart) : strlen(pStart);
		size_t iProgram = strlen(sProgram);
		size_t iSize;
		char* sCandidate;

		if ( iDirectory > (SIZE_MAX - iProgram - 2u) ) {
			return NULL;
		}
		iSize = iDirectory + iProgram + 2u;
		sCandidate = (char*)xrtMalloc(iSize);
		if ( sCandidate == NULL ) {
			return NULL;
		}
		if ( iDirectory != 0u ) {
			memcpy(sCandidate, pStart, iDirectory);
		} else {
			sCandidate[0] = '.';
			iDirectory = 1u;
		}
		sCandidate[iDirectory] = '/';
		memcpy(
			sCandidate + iDirectory + 1u,
			sProgram,
			iProgram + 1u
		);
		if ( access(sCandidate, X_OK) == 0 ) {
			return sCandidate;
		}
		xrtFree(sCandidate);
		if ( pEnd == NULL ) {
			break;
		}
		pStart = pEnd + 1;
	}
	errno = ENOENT;
	return NULL;
}



/* 构建 argv、envp 和已解析程序路径。 */
static bool __xrtProcessPlanBuild(
	const xprocessconfig* pConfig,
	xprocessposixplan* pPlan
)
{
	size_t iArgCount = pConfig->Target == XPROCESS_SHELL ?
		3u : (pConfig->ArgCount + 1u);
	cstr sProgram = pConfig->Target == XPROCESS_SHELL ?
		"/bin/sh" : pConfig->Program;

	memset(pPlan, 0, sizeof(xprocessposixplan));
	if ( iArgCount == SIZE_MAX ) {
		return false;
	}
	pPlan->Argv = (char**)xrtCalloc(iArgCount + 1u, sizeof(char*));
	if ( pPlan->Argv == NULL ) {
		return false;
	}
	if ( pConfig->Target == XPROCESS_SHELL ) {
		pPlan->Argv[0] = __xrtProcessTextCopy("/bin/sh");
		pPlan->Argv[1] = __xrtProcessTextCopy("-c");
		pPlan->Argv[2] = __xrtProcessTextCopy(pConfig->Command);
	} else {
		pPlan->Argv[0] = __xrtProcessTextCopy(
			pConfig->Arg0 != NULL ? pConfig->Arg0 : pConfig->Program
		);
		for ( size_t i = 0u; i < pConfig->ArgCount; i++ ) {
			pPlan->Argv[i + 1u] = __xrtProcessTextCopy(pConfig->Args[i]);
		}
	}
	for ( size_t i = 0u; i < iArgCount; i++ ) {
		if ( pPlan->Argv[i] == NULL ) {
			return false;
		}
	}
	pPlan->Env = __xrtProcessEnvironmentBuild(pConfig);
	if ( pPlan->Env == NULL ) {
		return false;
	}
	pPlan->Program = __xrtProcessProgramResolve(sProgram, pPlan->Env);
	return pPlan->Program != NULL;
}



/* 隔离调用前错误，确保计划失败只发布本次启动原因。 */
static bool __xrtProcessPlanPrepare(
	const xprocessconfig* pConfig,
	xprocessposixplan* pPlan
)
{
	xerror* pPrevious = xrtTakeError();
	xerror* pFailure;
	int iError;

	errno = 0;
	if ( __xrtProcessPlanBuild(pConfig, pPlan) ) {
		pFailure = xrtTakeError();
		xrtErrorFree(pFailure);
		if ( pPrevious != NULL ) {
			__xrtErrorSetOwned(pPrevious);
		}
		return true;
	}
	iError = errno;
	pFailure = xrtTakeError();
	xrtErrorFree(pPrevious);
	if ( pFailure != NULL ) {
		__xrtErrorSetOwned(pFailure);
		return false;
	}
	__xrtProcessErrorSet(
		iError == ENOENT ? XERR_NOT_FOUND : XERR_RANGE,
		XPROCESS_ERROR_COMMAND,
		"spawn.command",
		iError == ENOENT ?
			"process program was not found" :
			"process launch plan could not be represented",
		iError
	);
	return false;
}



/* 释放父进程构建的 POSIX 启动计划。 */
static void __xrtProcessPlanUnit(xprocessposixplan* pPlan)
{
	__xrtProcessStringsFree(pPlan->Argv);
	__xrtProcessStringsFree(pPlan->Env);
	xrtFree(pPlan->Program);
	memset(pPlan, 0, sizeof(xprocessposixplan));
}



/* 子进程以单次小写入报告启动错误。 */
static void __xrtProcessChildErrorWrite(
	int iFd,
	xprocesserror Stage,
	int iSystemCode
)
{
	xprocesschilderror Error;
	ssize_t iResult;

	Error.Stage = Stage;
	Error.SystemCode = iSystemCode;
	do {
		iResult = write(iFd, &Error, sizeof(Error));
	} while ( (iResult < 0) && (errno == EINTR) );
}



/* 子进程重定向一个配置流，失败时直接报告并退出。 */
static void __xrtProcessChildRedirect(
	int iSource,
	int iTarget,
	int iErrorFd,
	xprocesserror Stage
)
{
	if ( (iSource >= 0) && (iSource != iTarget) &&
		(dup2(iSource, iTarget) < 0) ) {
		__xrtProcessChildErrorWrite(iErrorFd, Stage, errno);
		_exit(126);
	}
}



/* 关闭一个有效 fd。 */
static void __xrtProcessFdClose(int* pFd)
{
	if ( *pFd >= 0 ) {
		(void)close(*pFd);
		*pFd = -1;
	}
}



/* 根据模式准备子端 fd 和可选父端 fd。 */
static bool __xrtProcessStdioPrepare(
	xprocessstream Stream,
	xprocessio Io,
	int* pChild,
	int* pParent
)
{
	int pPipe[2] = { -1, -1 };

	*pChild = -1;
	*pParent = -1;
	if ( Io.Mode == XPROCESS_IO_PIPE ) {
		if ( Stream == XPROCESS_STDIN ) {
			if ( !__xrtProcessInputPipe(pPipe) ) {
				return false;
			}
			*pChild = pPipe[0];
			*pParent = pPipe[1];
		} else {
			if ( !__xrtProcessPipe(pPipe) ) {
				return false;
			}
			*pParent = pPipe[0];
			*pChild = pPipe[1];
		}
		return true;
	}
	if ( Io.Mode == XPROCESS_IO_NULL ) {
		*pChild = open(
			"/dev/null",
			Stream == XPROCESS_STDIN ? O_RDONLY : O_WRONLY
		);
		if ( (*pChild < 0) || !__xrtProcessFdOwn(pChild) ) {
			__xrtProcessFdClose(pChild);
			return false;
		}
		return true;
	}
	if ( Io.Mode == XPROCESS_IO_HANDLE ) {
		if ( (Io.Handle < 0) || (Io.Handle > INT_MAX) ) {
			errno = EBADF;
			return false;
		}
		return __xrtProcessFdDuplicate((int)Io.Handle, pChild);
	}
	return true;
}



#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* 读取 PTY 从端路径；支持的平台优先使用可重入入口。 */
static char* __xrtProcessTerminalSlaveName(int iMaster)
{
	#if defined(__linux__) || defined(__ANDROID__)
		size_t iCapacity = 128u;

		for ( ;; ) {
			char* sName = (char*)xrtMalloc(iCapacity);
			int iResult;
			int iError;

			if ( sName == NULL ) {
				errno = ENOMEM;
				return NULL;
			}
			errno = 0;
			iResult = ptsname_r(iMaster, sName, iCapacity);
			if ( iResult == 0 ) {
				return sName;
			}
			iError = iResult == -1 ? errno : iResult;
			xrtFree(sName);
			if ( iError != ERANGE ) {
				errno = iError;
				return NULL;
			}
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				errno = EOVERFLOW;
				return NULL;
			}
			iCapacity *= 2u;
		}
	#else
		char* sName = ptsname(iMaster);

		return sName != NULL ? __xrtProcessTextCopy(sName) : NULL;
	#endif
}



/* 关闭启动中的 PTY 子进程，并确保不会留下僵尸进程。 */
static void __xrtProcessTerminalChildStop(pid_t* pPid)
{
	if ( *pPid <= 0 ) {
		return;
	}
	(void)kill(*pPid, SIGKILL);
	while ( (waitpid(*pPid, NULL, 0) < 0) && (errno == EINTR) ) {
	}
	*pPid = -1;
}



/* 判断当前 POSIX 系统能否建立并授权伪终端。 */
bool __xrtProcessTerminalSupportedPlatform(void)
{
	int iMaster = posix_openpt(O_RDWR | O_NOCTTY);
	bool bSupported;

	if ( iMaster < 0 ) {
		return false;
	}
	bSupported = (grantpt(iMaster) == 0) && (unlockpt(iMaster) == 0);
	(void)close(iMaster);
	return bSupported;
}



/* 调整 PTY 尺寸，内核负责向前台进程组发送 SIGWINCH。 */
bool __xrtProcessTerminalResizePlatform(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
)
{
	struct winsize Size;
	int iFd;
	int iResult;

	memset(&Size, 0, sizeof(Size));
	Size.ws_col = (unsigned short)iColumns;
	Size.ws_row = (unsigned short)iRows;
	(void)xrtMutexLock(&pProcess->Lock);
	iFd = pProcess->Stdout;
	if ( iFd < 0 ) {
		(void)xrtMutexUnlock(&pProcess->Lock);
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"process terminal is closed",
			0
		);
		return false;
	}
	iResult = ioctl(iFd, TIOCSWINSZ, &Size);
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( iResult == 0 ) {
		return true;
	}
	{
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"process terminal could not be resized",
			iError
		);
	}
	return false;
}



/* 使用单个 PTY 承载子进程的 stdin、stdout 与 stderr。 */
static bool __xrtProcessTerminalSpawnPosix(
	xprocess* pProcess,
	const xprocessconfig* pConfig
)
{
	xprocessposixplan Plan;
	struct winsize Size;
	int pError[2] = { -1, -1 };
	int iMaster = -1;
	int iInput = -1;
	char* sSlave = NULL;
	pid_t iPid = -1;
	xprocesschilderror ChildError;
	ssize_t iRead;
	bool bOk = false;
	int iError;

	memset(&Plan, 0, sizeof(Plan));
	memset(&Size, 0, sizeof(Size));
	if ( !__xrtProcessPlanPrepare(pConfig, &Plan) ) {
		goto cleanup;
	}
	if ( !__xrtProcessPipe(pError) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.terminal.error_pipe",
			"terminal error pipe could not be created",
			iError
		);
		goto cleanup;
	}
	iMaster = posix_openpt(O_RDWR | O_NOCTTY);
	if ( (iMaster < 0) || !__xrtProcessFdOwn(&iMaster) ||
		(grantpt(iMaster) != 0) || (unlockpt(iMaster) != 0) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal",
			"POSIX pseudo-terminal could not be prepared",
			iError
		);
		goto cleanup;
	}
	sSlave = __xrtProcessTerminalSlaveName(iMaster);
	if ( sSlave == NULL ) {
		iError = errno;
		__xrtProcessErrorSet(
			iError != 0 ? __xrtSystemErrorKind(iError) : XERR_MEMORY,
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal.name",
			"pseudo-terminal slave name could not be copied",
			iError
		);
		goto cleanup;
	}
	if ( !__xrtProcessFdDuplicate(iMaster, &iInput) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.terminal.duplicate",
			"pseudo-terminal input descriptor could not be duplicated",
			iError
		);
		goto cleanup;
	}
	Size.ws_col = (unsigned short)pConfig->Columns;
	Size.ws_row = (unsigned short)pConfig->Rows;
	if ( ioctl(iMaster, TIOCSWINSZ, &Size) != 0 ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal.resize",
			"pseudo-terminal initial size could not be set",
			iError
		);
		goto cleanup;
	}
	iPid = fork();
	if ( iPid < 0 ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.terminal.fork",
			"terminal process could not be forked",
			iError
		);
		goto cleanup;
	}
	if ( iPid == 0 ) {
		int iSlave;

		(void)close(pError[0]);
		if ( setsid() < 0 ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_TERMINAL,
				errno
			);
			_exit(126);
		}
		iSlave = open(sSlave, O_RDWR | O_NOCTTY);
		if ( iSlave < 0 ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_TERMINAL,
				errno
			);
			_exit(126);
		}
		#if defined(TIOCSCTTY)
			if ( ioctl(iSlave, TIOCSCTTY, 0) != 0 ) {
				__xrtProcessChildErrorWrite(
					pError[1],
					XPROCESS_ERROR_TERMINAL,
					errno
				);
				_exit(126);
			}
		#endif
		if ( ioctl(iSlave, TIOCSWINSZ, &Size) != 0 ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_TERMINAL,
				errno
			);
			_exit(126);
		}
		__xrtProcessChildRedirect(
			iSlave,
			STDIN_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		__xrtProcessChildRedirect(
			iSlave,
			STDOUT_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		__xrtProcessChildRedirect(
			iSlave,
			STDERR_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		if ( iSlave > STDERR_FILENO ) {
			(void)close(iSlave);
		}
		(void)close(iMaster);
		(void)close(iInput);
		if ( (pConfig->WorkDir != NULL) &&
			(chdir(pConfig->WorkDir) != 0) ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_CONFIG,
				errno
			);
			_exit(126);
		}
		execve(Plan.Program, Plan.Argv, Plan.Env);
		__xrtProcessChildErrorWrite(
			pError[1],
			XPROCESS_ERROR_SPAWN,
			errno
		);
		_exit(127);
	}
	__xrtProcessFdClose(&pError[1]);
	memset(&ChildError, 0, sizeof(ChildError));
	do {
		iRead = read(pError[0], &ChildError, sizeof(ChildError));
	} while ( (iRead < 0) && (errno == EINTR) );
	__xrtProcessFdClose(&pError[0]);
	if ( iRead > 0 ) {
		(void)waitpid(iPid, NULL, 0);
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(ChildError.SystemCode),
			ChildError.Stage,
			ChildError.Stage == XPROCESS_ERROR_CONFIG ?
				"spawn.workdir" : "spawn.terminal.child",
			"terminal child setup failed",
			ChildError.SystemCode
		);
		iPid = -1;
		goto cleanup;
	}
	if ( iRead < 0 ) {
		iError = errno;
		__xrtProcessTerminalChildStop(&iPid);
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.terminal.error_pipe",
			"terminal child setup result could not be read",
			iError
		);
		goto cleanup;
	}
	pProcess->Id = iPid;
	pProcess->Stdin = iInput;
	pProcess->Stdout = iMaster;
	pProcess->Stderr = -1;
	iPid = -1;
	iInput = -1;
	iMaster = -1;
	bOk = true;

cleanup:
	if ( !bOk ) {
		__xrtProcessTerminalChildStop(&iPid);
	}
	__xrtProcessFdClose(&pError[0]);
	__xrtProcessFdClose(&pError[1]);
	__xrtProcessFdClose(&iInput);
	__xrtProcessFdClose(&iMaster);
	xrtFree(sSlave);
	__xrtProcessPlanUnit(&Plan);
	return bOk;
}
#endif



/* 普通 POSIX 进程使用安全的最小 fork 子路径启动。 */
bool __xrtProcessPlatformSpawn(
	xprocess* pProcess,
	const xprocessconfig* pConfig
)
{
	xprocessposixplan Plan;
	int pError[2] = { -1, -1 };
	int iChildIn = -1;
	int iChildOut = -1;
	int iChildErr = -1;
	int iParentIn = -1;
	int iParentOut = -1;
	int iParentErr = -1;
	pid_t iPid = -1;
	xprocesschilderror ChildError;
	ssize_t iRead;
	bool bOk = false;
	int iError;

	#if defined(XRT_FEATURE_PROCESS_TERMINAL)
		if ( pConfig->Terminal ) {
			return __xrtProcessTerminalSpawnPosix(pProcess, pConfig);
		}
	#endif
	memset(&Plan, 0, sizeof(Plan));
	if ( !__xrtProcessPlanPrepare(pConfig, &Plan) ) {
		goto cleanup;
	}
	if ( !__xrtProcessPipe(pError) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.error_pipe",
			"process error pipe could not be created",
			iError
		);
		goto cleanup;
	}
	if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDIN,
		pConfig->Stdin,
		&iChildIn,
		&iParentIn
	) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stdin",
			"process stdin could not be prepared",
			iError
		);
		goto cleanup;
	}
	if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDOUT,
		pConfig->Stdout,
		&iChildOut,
		&iParentOut
	) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stdout",
			"process stdout could not be prepared",
			iError
		);
		goto cleanup;
	}
	if ( pConfig->Stderr.Mode == XPROCESS_IO_MERGE ) {
		iChildErr = STDOUT_FILENO;
	} else if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDERR,
		pConfig->Stderr,
		&iChildErr,
		&iParentErr
	) ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stderr",
			"process stderr could not be prepared",
			iError
		);
		goto cleanup;
	}
	iPid = fork();
	if ( iPid < 0 ) {
		iError = errno;
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.fork",
			"process could not be forked",
			iError
		);
		goto cleanup;
	}
	if ( iPid == 0 ) {
		(void)close(pError[0]);
		if ( pConfig->NewGroup && (setpgid(0, 0) != 0) ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_SPAWN,
				errno
			);
			_exit(126);
		}
		__xrtProcessChildRedirect(
			iChildIn,
			STDIN_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		__xrtProcessChildRedirect(
			iChildOut,
			STDOUT_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		__xrtProcessChildRedirect(
			iChildErr,
			STDERR_FILENO,
			pError[1],
			XPROCESS_ERROR_PIPE
		);
		if ( iChildIn > STDERR_FILENO ) {
			(void)close(iChildIn);
		}
		if ( (iChildOut > STDERR_FILENO) && (iChildOut != iChildIn) ) {
			(void)close(iChildOut);
		}
		if ( (iChildErr > STDERR_FILENO) &&
			(iChildErr != iChildIn) && (iChildErr != iChildOut) ) {
			(void)close(iChildErr);
		}
		if ( iParentIn >= 0 ) {
			(void)close(iParentIn);
		}
		if ( iParentOut >= 0 ) {
			(void)close(iParentOut);
		}
		if ( iParentErr >= 0 ) {
			(void)close(iParentErr);
		}
		if ( (pConfig->WorkDir != NULL) &&
			(chdir(pConfig->WorkDir) != 0) ) {
			__xrtProcessChildErrorWrite(
				pError[1],
				XPROCESS_ERROR_CONFIG,
				errno
			);
			_exit(126);
		}
		execve(Plan.Program, Plan.Argv, Plan.Env);
		__xrtProcessChildErrorWrite(
			pError[1],
			XPROCESS_ERROR_SPAWN,
			errno
		);
		_exit(127);
	}
	__xrtProcessFdClose(&pError[1]);
	__xrtProcessFdClose(&iChildIn);
	__xrtProcessFdClose(&iChildOut);
	if ( pConfig->Stderr.Mode != XPROCESS_IO_MERGE ) {
		__xrtProcessFdClose(&iChildErr);
	}
	memset(&ChildError, 0, sizeof(ChildError));
	do {
		iRead = read(pError[0], &ChildError, sizeof(ChildError));
	} while ( (iRead < 0) && (errno == EINTR) );
	__xrtProcessFdClose(&pError[0]);
	if ( iRead > 0 ) {
		(void)waitpid(iPid, NULL, 0);
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(ChildError.SystemCode),
			ChildError.Stage,
			ChildError.Stage == XPROCESS_ERROR_CONFIG ?
				"spawn.workdir" : "spawn.exec",
			"child process setup failed",
			ChildError.SystemCode
		);
		iPid = -1;
		goto cleanup;
	}
	if ( iRead < 0 ) {
		iError = errno;
		(void)kill(iPid, SIGKILL);
		(void)waitpid(iPid, NULL, 0);
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.error_pipe",
			"child setup result could not be read",
			iError
		);
		iPid = -1;
		goto cleanup;
	}
	pProcess->Id = iPid;
	pProcess->Stdin = iParentIn;
	pProcess->Stdout = iParentOut;
	pProcess->Stderr = iParentErr;
	iParentIn = -1;
	iParentOut = -1;
	iParentErr = -1;
	bOk = true;

cleanup:
	__xrtProcessFdClose(&pError[0]);
	__xrtProcessFdClose(&pError[1]);
	__xrtProcessFdClose(&iChildIn);
	__xrtProcessFdClose(&iChildOut);
	if ( pConfig->Stderr.Mode != XPROCESS_IO_MERGE ) {
		__xrtProcessFdClose(&iChildErr);
	}
	__xrtProcessFdClose(&iParentIn);
	__xrtProcessFdClose(&iParentOut);
	__xrtProcessFdClose(&iParentErr);
	__xrtProcessPlanUnit(&Plan);
	return bOk;
}



/* 等待指定 pid，绝不消费其他库创建的子进程。 */
bool __xrtProcessPlatformWait(
	xprocess* pProcess,
	xprocessstatus* pStatus
)
{
	int iStatus;
	pid_t iResult;

	do {
		iResult = waitpid(pProcess->Id, &iStatus, 0);
	} while ( (iResult < 0) && (errno == EINTR) );
	if ( iResult < 0 ) {
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_WAIT,
			"wait",
			"process wait failed",
			iError
		);
		return false;
	}
	if ( WIFEXITED(iStatus) ) {
		pStatus->Kind = XPROCESS_EXIT_CODE;
		pStatus->Code = (int32)WEXITSTATUS(iStatus);
		return true;
	}
	if ( WIFSIGNALED(iStatus) ) {
		pStatus->Kind = XPROCESS_EXIT_SIGNAL;
		pStatus->Code = -1;
		pStatus->Signal = (int32)WTERMSIG(iStatus);
		#if defined(WCOREDUMP)
			pStatus->CoreDumped = WCOREDUMP(iStatus) != 0;
		#endif
		return true;
	}
	__xrtProcessErrorSet(
		XERR_INTERNAL,
		XPROCESS_ERROR_WAIT,
		"wait.status",
		"process returned an unsupported wait status",
		0
	);
	return false;
}



/* 读取父端输出 fd。 */
int64 __xrtProcessPlatformRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
)
{
	int iFd;
	ssize_t iRead;
	size_t iChunk = iSize > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : iSize;

	(void)xrtMutexLock(&pProcess->Lock);
	iFd = Stream == XPROCESS_STDOUT ? pProcess->Stdout : pProcess->Stderr;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( iFd < 0 ) {
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_READ,
			"read",
			"process output pipe is closed",
			0
		);
		return -1;
	}
	do {
		iRead = read(iFd, pData, iChunk);
	} while ( (iRead < 0) && (errno == EINTR) );
	if ( iRead >= 0 ) {
		return (int64)iRead;
	}
	#if defined(XRT_FEATURE_PROCESS_TERMINAL)
		if ( pProcess->Terminal && (errno == EIO) ) {
			return 0;
		}
	#endif
	{
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_READ,
			"read",
			"process output read failed",
			iError
		);
	}
	return -1;
}



/* 向 socketpair stdin 写入并抑制 SIGPIPE。 */
int64 __xrtProcessPlatformWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
{
	int iFd;
	ssize_t iWritten;
	size_t iChunk = iSize > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : iSize;

	(void)xrtMutexLock(&pProcess->Lock);
	iFd = pProcess->Stdin;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( iFd < 0 ) {
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_WRITE,
			"write",
			"process input pipe is closed",
			0
		);
		return -1;
	}
	do {
		#if defined(XRT_FEATURE_PROCESS_TERMINAL)
			if ( pProcess->Terminal ) {
				iWritten = write(iFd, pData, iChunk);
			} else
		#endif
		{
			#if defined(MSG_NOSIGNAL)
				iWritten = send(iFd, pData, iChunk, MSG_NOSIGNAL);
			#else
				iWritten = send(iFd, pData, iChunk, 0);
			#endif
		}
	} while ( (iWritten < 0) && (errno == EINTR) );
	if ( iWritten >= 0 ) {
		return (int64)iWritten;
	}
	{
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_WRITE,
			"write",
			"process input write failed",
			iError
		);
	}
	return -1;
}



/* 在锁下取走 fd，再在锁外关闭。 */
bool __xrtProcessPlatformClose(
	xprocess* pProcess,
	xprocessstream Stream
)
{
	int* pSlot;
	int iFd;

	if ( Stream == XPROCESS_STDIN ) {
		pSlot = &pProcess->Stdin;
	} else if ( Stream == XPROCESS_STDOUT ) {
		pSlot = &pProcess->Stdout;
	} else {
		pSlot = &pProcess->Stderr;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	iFd = *pSlot;
	*pSlot = -1;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( iFd < 0 ) {
		return true;
	}
	if ( close(iFd) == 0 ) {
		return true;
	}
	{
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_CLOSE,
			"close",
			"process pipe could not be closed",
			iError
		);
	}
	return false;
}



/* 释放全部 POSIX 父端 fd。 */
void __xrtProcessPlatformUnit(xprocess* pProcess)
{
	__xrtProcessFdClose(&pProcess->Stdin);
	__xrtProcessFdClose(&pProcess->Stdout);
	__xrtProcessFdClose(&pProcess->Stderr);
}



/* 返回 pid。 */
uint64 __xrtProcessPlatformId(const xprocess* pProcess)
{
	return pProcess->Id > 0 ? (uint64)pProcess->Id : 0u;
}



/* POSIX 原生进程句柄就是 pid。 */
intptr_t __xrtProcessPlatformNative(const xprocess* pProcess)
{
	return (intptr_t)pProcess->Id;
}



/* 返回借用的父端 fd。 */
intptr_t __xrtProcessPlatformStream(
	const xprocess* pProcess,
	xprocessstream Stream
)
{
	int iFd;

	(void)xrtMutexLock((xmutex*)&pProcess->Lock);
	if ( Stream == XPROCESS_STDIN ) {
		iFd = pProcess->Stdin;
	} else if ( Stream == XPROCESS_STDOUT ) {
		iFd = pProcess->Stdout;
	} else if ( Stream == XPROCESS_STDERR ) {
		iFd = pProcess->Stderr;
	} else {
		iFd = -1;
	}
	(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
	return (intptr_t)iFd;
}



/* 向根进程或新进程组发送 POSIX 信号。 */
static bool __xrtProcessSignal(
	xprocess* pProcess,
	int iSignal,
	bool bTree,
	cstr sOperation
)
{
	pid_t iTarget = bTree && pProcess->NewGroup ?
		-pProcess->Id : pProcess->Id;

	if ( kill(iTarget, iSignal) == 0 ) {
		return true;
	}
	if ( errno == ESRCH ) {
		return true;
	}
	{
		int iError = errno;

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SIGNAL,
			sOperation,
			"process signal failed",
			iError
		);
	}
	return false;
}



/* SIGINT 默认作用于完整新进程组。 */
bool __xrtProcessPlatformInterrupt(xprocess* pProcess)
{
	return __xrtProcessSignal(
		pProcess,
		SIGINT,
		pProcess->NewGroup,
		"interrupt"
	);
}



/* SIGTERM 默认作用于完整新进程组，避免后代继续持有管道。 */
bool __xrtProcessPlatformTerminate(xprocess* pProcess)
{
	return __xrtProcessSignal(
		pProcess,
		SIGTERM,
		pProcess->NewGroup,
		"terminate"
	);
}



/* SIGKILL 只结束根进程。 */
bool __xrtProcessPlatformKill(xprocess* pProcess)
{
	return __xrtProcessSignal(pProcess, SIGKILL, false, "kill");
}



/* SIGKILL 结束新进程组，没有进程组时明确拒绝。 */
bool __xrtProcessPlatformKillTree(xprocess* pProcess)
{
	if ( !pProcess->NewGroup ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_SIGNAL,
			"kill.tree",
			"process was not created with a process group",
			0
		);
		return false;
	}
	return __xrtProcessSignal(pProcess, SIGKILL, true, "kill.tree");
}

#endif
