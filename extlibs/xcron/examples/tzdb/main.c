#include <xcron.h>
#include <stdio.h>
int main(void)
{
    uint8 Bytes[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='O',[51]='F',[52]='F'};
    xcrontzdbconfig C={8,1,1024*1024};xcrontzdb* D=NULL;xcrontzdbsnapshot *Old=NULL,*New=NULL;xcronzone *A=NULL,*B=NULL;int Result=1;
    xcrontzdbentry E={XRT_STR_LITERAL("Demo/Office"),{Bytes,sizeof(Bytes)}};uint64 Revision=0;
    if(!xcronTzdbCreate(&C,&D,NULL) || !xcronTzdbLoad(D,XRT_STR_LITERAL("release-a"),&E,1,0,&Revision,NULL) ||
        !xcronTzdbAcquire(D,&Old,NULL) || xcronTzdbFind(Old,E.Name,&A,NULL)!=XCRON_TZDB_FOUND)goto Done;
    Bytes[46]=0x0e;Bytes[47]=0x10;
    if(!xcronTzdbLoad(D,XRT_STR_LITERAL("release-b"),&E,1,Revision,&Revision,NULL) || !xcronTzdbAcquire(D,&New,NULL) ||
        xcronTzdbFind(New,E.Name,&B,NULL)!=XCRON_TZDB_FOUND)goto Done;
    if(!xcronTzdbClose(D,NULL) || !xcronTzdbDestroy(D,NULL))goto Done;
    D=NULL;xcronzonestate Before,After;
    if(!xcronZoneAt(A,XRT_TIME_UNIX_EPOCH,&Before,NULL) || !xcronZoneAt(B,XRT_TIME_UNIX_EPOCH,&After,NULL) || Before.Offset!=0 || After.Offset!=3600)goto Done;
    printf("[PASS] named snapshot revision=%llu old=%d new=%d after manager destruction\n",(unsigned long long)Revision,Before.Offset,After.Offset);Result=0;
Done:
    xcronZoneRelease(A);xcronZoneRelease(B);xcronTzdbSnapshotRelease(Old);xcronTzdbSnapshotRelease(New);
    if(D && (!xcronTzdbClose(D,NULL) || !xcronTzdbDestroy(D,NULL)))Result=2;
    xrtClearError();return Result;
}
