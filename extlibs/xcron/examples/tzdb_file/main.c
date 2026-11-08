#include <xcron.h>
#include <xrt/file.h>
#include <stdio.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    static const uint8 UTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
    xroot Parent=NULL,Root=NULL;xcrontzdb* D=NULL;xcrontzdbsnapshot* S=NULL;xcronzone* Z=NULL;
    char Directory[96]={0};bool OwnedDir=false,OwnedFile=false;int Result=1;xstrview Name=XRT_STR_LITERAL("UTC");
    if(Argc==1){
        int N=snprintf(Directory,sizeof(Directory),".xcron-tzdb-example-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
        if(N<=0 || (size_t)N>=sizeof(Directory) || !(Parent=xrtRootOpen(".")) || !xrtRootDirCreate(Parent,Directory,0700))goto Done;
        OwnedDir=true;if(!(Root=xrtRootOpenIn(Parent,Directory)))goto Done;
        xfileoptions O={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};xfile F=xrtRootFileOpen(Root,"UTC",&O);if(!F)goto Done;OwnedFile=true;
        bool Written=xrtWriteFull(F,UTC,sizeof(UTC),NULL),Closed=xrtClose(F);if(!Written || !Closed)goto Done;
    }else if(Argc==3){Root=xrtRootOpen(Argv[1]);Name=(xstrview){Argv[2],strlen(Argv[2])};if(!Root)goto Done;}
    else {fprintf(stderr,"usage: %s [immutable-release-directory zone-name]\n",Argv[0]);goto Done;}
    xcrontzdbconfig C={8,1,32*1024*1024};uint64 Revision=0;
    if(!xcronTzdbCreate(&C,&D,NULL) || !xcronTzdbLoadRoot(D,Root,XRT_STR_LITERAL("explicit-example"),&Name,1,0,&Revision,NULL) ||
        !xcronTzdbAcquire(D,&S,NULL) || xcronTzdbFind(S,Name,&Z,NULL)!=XCRON_TZDB_FOUND)goto Done;
    if(OwnedFile){if(!xrtRootRemove(Root,"UTC"))goto Done;OwnedFile=false;}
    bool Closed=xrtRootClose(Root);Root=NULL;if(!Closed)goto Done;
    xcronzonestate State;if(!xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&State,NULL))goto Done;
    printf("[PASS] explicit root snapshot: %.*s offset=%d revision=%llu after root close\n",(int)Name.Size,Name.Data,State.Offset,(unsigned long long)Revision);Result=0;
Done:
    xcronZoneRelease(Z);xcronTzdbSnapshotRelease(S);
    if(D && (!xcronTzdbClose(D,NULL) || !xcronTzdbDestroy(D,NULL)))Result=2;
    if(OwnedFile && Root && !xrtRootRemove(Root,"UTC"))Result=2;
    if(Root && !xrtRootClose(Root))Result=2;
    if(OwnedDir && Parent && !xrtRootRemove(Parent,Directory))Result=2;
    if(Parent && !xrtRootClose(Parent))Result=2;
    xrtClearError();return Result;
}
