#include <xcron.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static int tzdbFileCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static bool tzdbFileArgument(const char* Text,uint32 Limit,uint32* Value)
{
    char* End=NULL;errno=0;unsigned long N=strtoul(Text,&End,10);
    if(errno || End==Text || *End || !N || N>Limit)return false;
    *Value=(uint32)N;return true;
}
int main(int Argc,char** Argv)
{
    static const uint8 UTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
    uint32 Rounds=256,Count=16,Created=0;int Exit=2;bool DirectoryCreated=false;
    if(Argc>3 || (Argc>1 && !tzdbFileArgument(Argv[1],100000,&Rounds)) ||
       (Argc>2 && !tzdbFileArgument(Argv[2],128,&Count)))return 1;
    char Directory[96],Text[128][24];xstrview Names[128];
    xroot Parent=NULL,Root=NULL;xfile File=NULL;xcrontzdb* DB=NULL;double* Samples=NULL;
    xcrontzdbsnapshot* Snapshot=NULL;uint64 Revision=0,Sum=0;
    for(uint32 I=0;I<Count;++I){int N=snprintf(Text[I],sizeof(Text[I]),"Zone_%04u",I);
        if(N<=0 || (size_t)N>=sizeof(Text[I]))goto Done;
        Names[I]=(xstrview){Text[I],(size_t)N};}
    int N=snprintf(Directory,sizeof(Directory),".xcron-tzdb-bench-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    if(N<=0 || (size_t)N>=sizeof(Directory))goto Done;
    Parent=xrtRootOpen(".");if(!Parent || !xrtRootDirCreate(Parent,Directory,0700))goto Done;
    DirectoryCreated=true;Root=xrtRootOpenIn(Parent,Directory);if(!Root)goto Done;
    xfileoptions Options={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};
    for(uint32 I=0;I<Count;++I){File=xrtRootFileOpen(Root,Text[I],&Options);if(!File)goto Done;
        ++Created;if(!xrtWriteFull(File,UTC,sizeof(UTC),NULL))goto Done;
        xfile Closing=File;File=NULL;if(!xrtClose(Closing))goto Done;}
    xcrontzdbconfig Config={Count,1,16*1024*1024};
    if(!xcronTzdbCreate(&Config,&DB,NULL) || !(Samples=xrtMalloc((size_t)Rounds*sizeof(*Samples))))goto Done;
    double Start=xrtTimer();
    for(uint32 I=0;I<Rounds;++I){double T=xrtTimer();uint64 Next=0;
        if(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("benchmark"),Names,Count,Revision,&Next,NULL) || Next!=Revision+1)goto Done;
        Samples[I]=(xrtTimer()-T)*1e6;Revision=Next;Sum+=Next;}
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0) || !xcronTzdbAcquire(DB,&Snapshot,NULL))goto Done;
    xcrontzdbsnapshotinfo Info;
    if(!xcronTzdbSnapshotInfo(Snapshot,&Info,NULL) || Info.Revision!=Rounds || Info.Names!=Count)goto Done;
    for(uint32 I=0;I<Count;++I){xcronzone* Zone=NULL;xcronzonestate State;
        if(xcronTzdbFind(Snapshot,Names[I],&Zone,NULL)!=XCRON_TZDB_FOUND)goto Done;
        bool OK=xcronZoneAt(Zone,XRT_TIME_UNIX_EPOCH,&State,NULL) && !State.Offset;xcronZoneRelease(Zone);
        if(!OK)goto Done;}
    qsort(Samples,Rounds,sizeof(*Samples),tzdbFileCompare);
    printf("reloads_per_sec: %.3f\nreload_p50_us: %.3f\nreload_p95_us: %.3f\nreload_p99_us: %.3f\nreload_p999_us: %.3f\nreloads: %u\nnames: %u\ninput_bytes: %zu\nfinal_revision: %" PRIu64 "\nchecksum: %" PRIu64 "\n",
        (double)Rounds/Elapsed,Samples[(Rounds-1)/2],Samples[(Rounds-1)*95/100],Samples[(Rounds-1)*99/100],Samples[(Rounds-1)*999/1000],Rounds,Count,(size_t)Count*sizeof(UTC),Revision,Sum);Exit=0;
Done:
    if(File && !xrtClose(File))Exit=3;
    xcronTzdbSnapshotRelease(Snapshot);xrtFree(Samples);
    if(DB && (!xcronTzdbClose(DB,NULL) || !xcronTzdbDestroy(DB,NULL)))Exit=3;
    if(Root){for(uint32 I=0;I<Created;++I)if(!xrtRootRemove(Root,Text[I]))Exit=3;
        if(!xrtRootClose(Root))Exit=3;}
    if(DirectoryCreated && !xrtRootRemove(Parent,Directory))Exit=3;
    if(Parent && !xrtRootClose(Parent))Exit=3;
    xrtClearError();return Exit;
}
