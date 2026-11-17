#include "test.h"
#include "../src/tzdb/tzdb_internal.h"
#include <xrt/file.h>
#include <xrt/thread.h>
static const uint8 TeUTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
typedef struct teobserver {xcrontzdb* DB;const xerror* Ambient;uint64 Caller;unsigned Old,Fresh;} teobserver;
static void teNotice(const xerror* Error,ptr Raw)
{
    teobserver* O=Raw;if(xrtThreadCurrentId()!=O->Caller)return;if(Error==O->Ambient&&xrtErrorFind(Error,"cron.tzdb.ambient",9))++O->Old;else ++O->Fresh;
    xcrontzdbstats S;testRequire(xcronTzdbInspect(O->DB,&S,NULL),"error callback can reenter publication-state inspection without a held lock");
}
static xcrontzdb* teCreate(uint32 Loads)
{xcrontzdbconfig C={8,Loads,1024*1024};xcrontzdb* D=NULL;testRequire(xcronTzdbCreate(&C,&D,NULL),"error-scope database");return D;}
static xcrontzdbstats teStats(xcrontzdb* D)
{xcrontzdbstats S;testRequire(xcronTzdbInspect(D,&S,NULL),"coherent database stats");return S;}
static void teEnd(xcrontzdb* D)
{testRequire(xcronTzdbClose(D,NULL)&&xcronTzdbDestroy(D,NULL),"physical database destruction");}
static void teAmbient(teobserver* O,xcrontzdb* D)
{*O=(teobserver){.DB=D,.Caller=xrtThreadCurrentId()};xrtSetErrorInfo(XERR_STATE,"cron.tzdb.ambient",9,"old caller diagnostic");O->Ambient=xrtGetError();testRequire(O->Ambient!=NULL,"ambient diagnostic");xrtSetErrorHandler(teNotice,O);}
static void teQuiet(teobserver* O,unsigned Fresh)
{testRequire(xrtGetError()==O->Ambient&&O->Fresh==Fresh,"success preserves exact ambient owner and expected fresh notices");}
static xcrontzdbsnapshot* teAcquire(xcrontzdb* D)
{xcrontzdbsnapshot* S=NULL;testRequire(xcronTzdbAcquire(D,&S,NULL),"retained snapshot");return S;}
static int32 teOffset(xcrontzdbsnapshot* S)
{xcronzone* Z=NULL;xcronzonestate State;testRequire(xcronTzdbFind(S,XRT_STR_LITERAL("UTC"),&Z,NULL)==XCRON_TZDB_FOUND&&xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&State,NULL),"independent zone owner");xcronZoneRelease(Z);return State.Offset;}
static void teMemory(void)
{
    xcrontzdb* D=teCreate(2);teobserver O;teAmbient(&O,D);xcrontzdbentry E={XRT_STR_LITERAL("UTC"),{TeUTC,sizeof(TeUTC)}};uint64 R=777;xcrontzdbissue I;
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("first"),&E,1,0,&R,&I)&&R==1&&I.Code==XCRON_TZDB_OK,"initial quiet publication");teQuiet(&O,0);xcrontzdbsnapshot* Old=teAcquire(D);
    uint8 Plus[sizeof(TeUTC)];memcpy(Plus,TeUTC,sizeof(Plus));Plus[46]=0x0e;Plus[47]=0x10;E.Tzif=(xbytesview){Plus,sizeof(Plus)};
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("second"),&E,1,1,&R,&I)&&R==2,"quiet replacement");teQuiet(&O,0);xcrontzdbsnapshot* New=teAcquire(D);testRequire(teOffset(Old)==0&&teOffset(New)==3600,"immutable old/new zones");
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("empty"),NULL,0,2,&R,&I)&&R==3,"quiet empty manifest");teQuiet(&O,0);testRequire(!teStats(D).Loading,"all builder credit returned");
    xrtSetErrorHandler(NULL,NULL);teEnd(D);testRequire(teOffset(Old)==0&&teOffset(New)==3600,"retained snapshots outlive quiet reload and database destruction");xcronTzdbSnapshotRelease(Old);xcronTzdbSnapshotRelease(New);
    printf("[TZDB NOTICES] old=%u new=%u\n",O.Old,O.Fresh);testRequire(!O.Old&&!O.Fresh,"successful loads must never notify an old error again");xrtClearError();
}
typedef struct tereader {unsigned Calls;bool Notice,Fail;} tereader;
static bool teRead(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{
    (void)Name;tereader* R=Raw;++R->Calls;if(R->Notice)xrtSetErrorInfo(XERR_STATE,"cron.reader.notice",1,"successful source diagnostic");
    if(R->Fail){Work->Code=XCRON_TZDB_IO;return false;}*Data=(xbytesview){TeUTC,sizeof(TeUTC)};*Owned=NULL;return true;
}
static void teReader(void)
{
    xcrontzdb* D=teCreate(1);teobserver O;teAmbient(&O,D);tereader Reader={.Notice=true};xstrview Name=XRT_STR_LITERAL("UTC");xcrontzdbsource Source={.Names=&Name,.Count=1,.Read=teRead,.Context=&Reader};uint64 R=777;
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"caller quota outside database-owned quota");
    testRequire(xcronTzdbLoadSource(D,XRT_STR_LITERAL("reader"),&Source,0,&R,NULL)&&R==1&&Reader.Calls==1,"quiet source construction under independent database quota");teQuiet(&O,1);
    testRequire(xrtAllocationBudgetLeave(&Scope),"original caller scope restored");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI)&&!BI.PeakBytes&&!BI.LiveBytes&&!BI.Rejections,"saved-error restoration adds no outer allocation");xrtAllocationBudgetFree(B);
    xcrontzdbsnapshot* S=teAcquire(D);xcrontzdbsnapshotinfo SI;testRequire(xcronTzdbSnapshotInfo(S,&SI,NULL),"reader snapshot bytes");xcrontzdbstats Before=teStats(D);testRequire(Before.LiveBytes==sizeof(*D)+SI.Bytes,"discarded successful source error physically refunded");xcronTzdbSnapshotRelease(S);
    Reader.Notice=false;Reader.Fail=true;xcrontzdbissue I;R=777;testRequire(!xcronTzdbLoadSource(D,XRT_STR_LITERAL("failure"),&Source,1,&R,&I)&&I.Code==XCRON_TZDB_IO&&R==777,"source false-with-no-error remains typed failure");
    testRequire(!O.Old&&O.Fresh==2&&!xrtErrorFind(xrtGetError(),"cron.tzdb.ambient",9)&&teStats(D).Revision==1&&!teStats(D).Loading,"fresh failure never reuses stale cause or leaks builder");xrtSetErrorHandler(NULL,NULL);xrtClearError();testRequire(teStats(D).LiveBytes==Before.LiveBytes,"source failure rollback refund");teEnd(D);
}
typedef struct tefiles {xroot Parent,Root;char Name[128];} tefiles;
static tefiles teFiles(void)
{
    tefiles F={0};int N=snprintf(F.Name,sizeof(F.Name),".xcron-tzdb-errors-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)(xrtTimer()*1000000));testRequire(N>0&&(size_t)N<sizeof(F.Name),"isolated release name");
    F.Parent=xrtRootOpen(".");testRequire(F.Parent&&xrtRootDirCreate(F.Parent,F.Name,0700),"create owned release directory");F.Root=xrtRootOpenIn(F.Parent,F.Name);testRequire(F.Root!=NULL,"anchored release root");xfileoptions O={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};xfile File=xrtRootFileOpen(F.Root,"UTC",&O);testRequire(File&&xrtWriteFull(File,TeUTC,sizeof(TeUTC),NULL)&&xrtClose(File),"write real release fixture");return F;
}
static void teFilesEnd(tefiles F)
{testRequire(xrtRootRemove(F.Root,"UTC")&&xrtRootClose(F.Root)&&xrtRootRemove(F.Parent,F.Name)&&xrtRootClose(F.Parent),"remove only known owned release file and directory");}
static void teRoot(void)
{
    tefiles F=teFiles();xcrontzdb* D=teCreate(1);teobserver O;teAmbient(&O,D);xstrview Name=XRT_STR_LITERAL("UTC");uint64 R=777;
    testRequire(xcronTzdbLoadRoot(D,F.Root,XRT_STR_LITERAL("root-a"),&Name,1,0,&R,NULL)&&R==1,"real root-backed quiet initial publication");teQuiet(&O,0);
    testRequire(xcronTzdbLoadRoot(D,F.Root,XRT_STR_LITERAL("root-b"),&Name,1,1,&R,NULL)&&R==2,"real root-backed quiet replacement");teQuiet(&O,0);testRequire(!O.Old,"root loader never repeats old diagnostic");xrtSetErrorHandler(NULL,NULL);xrtClearError();teEnd(D);teFilesEnd(F);
}
int main(void)
{teMemory();teReader();teRoot();puts("[PASS] quiet memory/source/root TZDB reloads, callbacks outside lock, exact ambient ownership, caller quota and retained generations");return 0;}
