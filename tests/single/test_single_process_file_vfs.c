/* process_file must preserve the real File backend failure, not replace
 * Unsupported/Memory with a process argument error. Keep the selected closure
 * independent from process_run/Future and exercise actual memory VFS. */
#define XRT_MODULE_PROCESS_FILE
#define XRT_MODULE_VFS_MEMORY
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#include <stdio.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
int main(void)
{
#if !defined(XRT_FEATURE_PROCESS_FILE) || !defined(XRT_FEATURE_VFS_MEMORY) || defined(XRT_FEATURE_PROCESS_RUN) || defined(XRT_FEATURE_FUTURE)
#error "Unexpected process_file/VFS selected dependency closure"
#endif
    assert(xrtMemDebugEnable(true));
    xvfs vfs=xrtVfsCreate();xvfsmemory memory=xrtVfsMemoryCreate();assert(vfs && memory);
    assert(xrtVfsMemoryPutCopy(memory,"item","a\0b",3) && xrtVfsMemorySeal(memory));
    xvfsmount mount=xrtVfsMemoryMount(vfs,"/m",0,(xvfscase)0,memory,0);assert(mount);
    xfileoptions options;xrtFileOptionsInit(&options);options.Flags=XFILE_READ;
    xfile file=xrtVfsOpen(vfs,"/m/item",&options);assert(file);
    assert(xrtFileNative(file)==-1);const xerror* native=xrtGetError();
    assert(native && xrtErrorKind(native)==XERR_UNSUPPORTED);
    int32 code=xrtErrorCode(native);xrtClearError();
    unsigned failures=0;bool complete=false;
    for(size_t point=0;point<32;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        xprocessio io=xrtProcessFile(file);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert(io.Mode==XPROCESS_IO_HANDLE && io.Handle==-1 && xrtGetError());
        if(hit){assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;}
        else{assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED && xrtErrorCode(xrtGetError())==code);complete=true;}
        xrtClearError();balanced(&before);if(complete)break;
    }
    assert(complete);
    xprocessio invalid=xrtProcessFile(NULL);
    assert(invalid.Handle==-1 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    assert(xrtErrorCode(xrtGetError())==XPROCESS_ERROR_ARGUMENT);xrtClearError();
    unsigned char data[3];size_t done=0;
    assert(xrtReadFull(file,data,3,&done) && done==3 && !memcmp(data,"a\0b",3));
    assert(xrtClose(file));xrtVfsMountDestroy(mount);xrtVfsMemoryDestroy(memory);xrtVfsDestroy(vfs);
    assert(xrtMemDebugReset());assert(xrtMemDebugEnable(false));
    printf("process_file actual VFS error: %u allocation failures + 1 first no-hit, Unsupported/Memory and NULL code preserved, balanced\n",failures);
    return 0;
}
