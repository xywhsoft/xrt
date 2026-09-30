#include "../internal/xrt_io.h"
#include <xrt/atomic.h>

#include <stdio.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#else
	#include <unistd.h>
#endif

#if defined(XRT_FEATURE_IO_STANDARD)

typedef struct __xrt_standard_io {
	int Stream;
} __xrt_standard_io;

static xatomicptr __xrtStandardInputOwner = XRT_ATOMICPTR_INIT(NULL);
bool __xrtStandardInputAcquire(ptr Owner)
{
    ptr Expected = NULL;
    if (xrtAtomicPtrCompareExchange(&__xrtStandardInputOwner, &Expected, Owner,
        XMEMORY_ACQUIRE, XMEMORY_RELAXED)) return true;
    __xrtIoError(XERR_STATE, XIO_ERROR_READ, "acquire-stdin", "standard input is already in use");
    return false;
}
void __xrtStandardInputRelease(ptr Owner)
{
    (void)xrtAtomicPtrCompareExchange(&__xrtStandardInputOwner, &Owner, NULL,
        XMEMORY_RELEASE, XMEMORY_RELAXED);
}

static FILE* __xrtStandardFile(int Stream)
{
	return Stream == 0 ? stdin : Stream == 1 ? stdout : stderr;
}

#if defined(_WIN32) || defined(_WIN64)
static HANDLE __xrtStandardHandle(int Stream)
{
	HANDLE Handle = GetStdHandle(Stream == 0 ? STD_INPUT_HANDLE :
		Stream == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
	if (Handle == NULL || Handle == INVALID_HANDLE_VALUE) {
		intptr_t Value = _get_osfhandle(_fileno(__xrtStandardFile(Stream)));
		Handle = Value == -1 ? INVALID_HANDLE_VALUE : (HANDLE)Value;
	}
	return Handle;
}
#endif

static bool __xrtStandardReadRaw(ptr Context, ptr Buffer, size_t Request, size_t* Read)
{
	(void)Context;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD Done = 0;
		DWORD Count = Request > MAXDWORD ? MAXDWORD : (DWORD)Request;
		if (!ReadFile(__xrtStandardHandle(0), Buffer, Count, &Done, NULL)) {
			DWORD Code = GetLastError();
			if (Code == ERROR_BROKEN_PIPE || Code == ERROR_HANDLE_EOF) {
				*Read = 0u;
				return true;
			}
			__xrtErrorSetSystem("xrt.io", XIO_ERROR_READ, "read-stdin",
				(int)Code, "standard input read failed");
			return false;
		}
		*Read = (size_t)Done;
	#else
		ssize_t Done;
		size_t Count = Request > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : Request;
		do { Done = read(STDIN_FILENO, Buffer, Count); } while (Done < 0 && errno == EINTR);
		if (Done < 0) {
			__xrtErrorSetSystem("xrt.io", XIO_ERROR_READ, "read-stdin",
				errno, "standard input read failed");
			return false;
		}
		*Read = (size_t)Done;
	#endif
	return true;
}

static bool __xrtStandardInputClose(ptr Context)
{
    __xrtStandardInputRelease(Context);
    return true;
}

static bool __xrtStandardWrite(ptr Context, const void* Buffer, size_t Request, size_t* Written)
{
	int Stream = ((__xrt_standard_io*)Context)->Stream;
	/* Drain stdio text before a raw write so serial console/IO calls stay ordered. */
	if (fflush(__xrtStandardFile(Stream)) != 0) {
		__xrtErrorSetSystem("xrt.io", XIO_ERROR_FLUSH, "write-standard",
			errno, "standard output flush failed");
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		DWORD Done = 0;
		DWORD Count = Request > MAXDWORD ? MAXDWORD : (DWORD)Request;
		if (!WriteFile(__xrtStandardHandle(Stream), Buffer, Count, &Done, NULL)) {
			__xrtErrorSetSystem("xrt.io", XIO_ERROR_WRITE, "write-standard",
				(int)GetLastError(), "standard output write failed");
			return false;
		}
		*Written = (size_t)Done;
	#else
		ssize_t Done;
		size_t Count = Request > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : Request;
		do { Done = write(Stream == 1 ? STDOUT_FILENO : STDERR_FILENO, Buffer, Count); }
		while (Done < 0 && errno == EINTR);
		if (Done < 0) {
			__xrtErrorSetSystem("xrt.io", XIO_ERROR_WRITE, "write-standard",
				errno, "standard output write failed");
			return false;
		}
		*Written = (size_t)Done;
	#endif
	return true;
}

static bool __xrtStandardFlush(ptr Context)
{
	if (fflush(__xrtStandardFile(((__xrt_standard_io*)Context)->Stream)) == 0)
		return true;
	__xrtErrorSetSystem("xrt.io", XIO_ERROR_FLUSH, "flush-standard",
		errno, "standard output flush failed");
	return false;
}

XRT_API xreader* xrtReaderStdin(void)
{
	xreaderops Ops = {0};
    ptr Context;
    xreader* Reader;
    Ops.Read = __xrtStandardReadRaw;
    Ops.Close = __xrtStandardInputClose;
    Reader = __xrtReaderCreateInline(&Ops, sizeof(__xrt_standard_io), &Context);
    if (Reader == NULL) return NULL;
    if (!__xrtStandardInputAcquire(Context)) { (void)xrtReaderDestroy(Reader); return NULL; }
    ((__xrt_standard_io*)Context)->Stream = 0;
    return Reader;
}

static xwriter* __xrtStandardWriter(int Stream)
{
	xwriterops Ops = {0};
	ptr Context;
	xwriter* Writer;
	Ops.Write = __xrtStandardWrite;
	Ops.Flush = __xrtStandardFlush;
	Writer = __xrtWriterCreateInline(&Ops, sizeof(__xrt_standard_io), &Context);
	if (Writer != NULL) ((__xrt_standard_io*)Context)->Stream = Stream;
	return Writer;
}

XRT_API xwriter* xrtWriterStdout(void) { return __xrtStandardWriter(1); }
XRT_API xwriter* xrtWriterStderr(void) { return __xrtStandardWriter(2); }

#endif
