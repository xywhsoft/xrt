#include "test.h"
#include <xrt/file.h>
static const uint8 TzdbFileUTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
#include "tzdb_file_fault_cases.h"
int main(void)
{
    char Directory[96];int N=snprintf(Directory,sizeof(Directory),".xcron-tzdb-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));testRequire(N>0 && (size_t)N<sizeof(Directory),"format isolated root name");
    xroot Parent=xrtRootOpen(".");testRequire(Parent && xrtRootDirCreate(Parent,Directory,0700),"create explicit timezone release root");xroot Root=xrtRootOpenIn(Parent,Directory);testRequire(Root!=NULL,"anchor release directory");
    xfileoptions O={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};xfile F=xrtRootFileOpen(Root,"UTC",&O);
    testRequire(F && xrtWriteFull(F,TzdbFileUTC,sizeof(TzdbFileUTC),NULL) && xrtClose(F),"write pinned TZif fixture");
    xcrontzdbconfig C={8,1,1024*1024};xcrontzdb* DB=NULL;testRequire(xcronTzdbCreate(&C,&DB,NULL),"file loader database");
    xstrview Names[]={XRT_STR_LITERAL("UTC")};uint64 R=0;
    testRequire(xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("offline-a"),Names,1,0,&R,NULL) && R==1,"root-backed named publication");
    xcrontzdbsnapshot* S=NULL;xcronzone* Z=NULL;xcronzonestate State;
    testRequire(xcronTzdbAcquire(DB,&S,NULL) && xcronTzdbFind(S,Names[0],&Z,NULL)==XCRON_TZDB_FOUND && xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&State,NULL) && !State.Offset,"real root-loaded lookup");
    xcronZoneRelease(Z);xcronTzdbSnapshotRelease(S);
#if defined(XCRON_TZDB_FILE_FAULTS)
    testTzdbFileFaults(DB,Root);
#endif
    testTzdbFileRejections(DB,Root,Parent,Directory);testRequire(xcronTzdbClose(DB,NULL) && xcronTzdbDestroy(DB,NULL),"root-backed manager cleanup");
    testRequire(xrtRootRemove(Root,"UTC") && xrtRootClose(Root) && xrtRootRemove(Parent,Directory) && xrtRootClose(Parent),"release file/root/directory duties");
    puts("[PASS] named TZif explicit anchored-root loading");return 0;
}
