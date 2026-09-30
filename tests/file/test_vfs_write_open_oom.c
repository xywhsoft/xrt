#include <xrt/memory_debug.h>
#include <xrt/vfs.h>
#include "../test.h"

/* OOM in native, rooted and VFS disk opens must not truncate caller data. */
int main(void)
{
    char directory[96];
    static const unsigned char original[]={'K',0,'E','E','P'};
    xfileoptions options;
    xroot parent,root;
    xvfs fs;
    xvfsdisk disk;
    xvfsmount mount;
    str path;
    testRequire(xrtMemDebugEnable(true),"cannot enable memory debug");
    snprintf(directory,sizeof(directory),".xrt-vfs-write-oom-%lld",(long long)xrtNow());
    parent=xrtRootOpen(".");
    testRequire(parent != NULL && xrtRootDirCreate(parent,directory,0700u),"cannot create fixture");
    root=xrtRootOpenIn(parent,directory);
    fs=xrtVfsCreate();
    disk=xrtVfsDiskCreate(directory,XVFS_DISK_READ|XVFS_DISK_WRITE);
    testRequire(root != NULL && fs != NULL && disk != NULL,"cannot create roots");
    mount=xrtVfsDiskMount(fs,"/",0,XVFS_CASE_SENSITIVE,disk,0u);
    path=xrtPathJoin(directory,"entry.bin");
    testRequire(mount != NULL && path != NULL,"cannot mount fixture");
    xrtFileOptionsInit(&options);
    options.Flags=XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE;
    for (int mode=0;mode<3;++mode) {
        bool completed=false;
        size_t points=0;
        for (uint64 failure=0;failure<128u;++failure) {
            xfile file=xrtOpen(path,XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE);
            bool triggered;
            xmemdebugsnapshot before,after;
            testRequire(file != NULL && xrtWriteFull(file,original,sizeof(original),NULL) &&
                xrtClose(file),"cannot initialize content");
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            testRequire(xrtMemDebugFailAfter(failure),"cannot inject allocation failure");
            file=mode==0 ? xrtFileOpen(path,&options) :
                 mode==1 ? xrtRootFileOpen(root,"entry.bin",&options) :
                           xrtVfsOpen(fs,"/entry.bin",&options);
            triggered=xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (file != NULL) {
                testRequire(!triggered && xrtClose(file),"open succeeded after OOM");
                completed=true;
            } else {
                unsigned char data[sizeof(original)];
                size_t count=0;
                testRequire(triggered,"open failed without injected failure");
                ++points;
                xrtClearError();
                file=xrtOpen(path,XFILE_READ);
                testRequire(file != NULL && xrtReadFull(file,data,sizeof(data),&count) &&
                    count==sizeof(data) && memcmp(data,original,sizeof(data))==0 &&
                    xrtClose(file),"OOM truncated existing bytes");
            }
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,
                "open leaked allocations");
            if (completed) break;
        }
        testRequire(completed && points != 0,"OOM enumeration did not complete");
    }
    xrtFree(path);
    xrtVfsMountDestroy(mount);
    xrtVfsDestroy(fs);
    xrtVfsDiskDestroy(disk);
    testRequire(xrtRootRemove(root,"entry.bin") && xrtRootClose(root) &&
        xrtRootRemove(parent,directory) && xrtRootClose(parent),"cannot clean fixture");
    xrtClearError();
    return 0;
}
