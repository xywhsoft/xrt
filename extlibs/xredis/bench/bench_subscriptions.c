#include <xredis/subscriptions.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static void subscriptionsBenchRequire(bool OK,const char* What)
{if(!OK){fprintf(stderr,"[FAIL] subscriptions bench: %s\n",What);exit(1);}}
static int subscriptionsBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):10000,Entries=Argc>2?(size_t)strtoull(Argv[2],NULL,10):32;bool Edit=Argc>3 && atoi(Argv[3])!=0,Instrument=Argc>4 && atoi(Argv[4])!=0;
    if(!Count || Count>2000000 || Entries>16384 || Argc>5)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));subscriptionsBenchRequire(Samples!=NULL,"timing storage");xredissubscriptionsconfig K;xredisSubscriptionsConfigInit(&K);K.MaxSubscriptions=Entries+1;xredissubscriptions* S=NULL;subscriptionsBenchRequire(xredisSubscriptionsCreate(&K,&S),"store");
    unsigned char (*Bytes)[32]=malloc((Entries?Entries:1)*32);xbytesview* Names=malloc((Entries?Entries:1)*sizeof(*Names));subscriptionsBenchRequire(Bytes && Names,"input fixture");
    for(size_t N=0;N<Entries;++N){int Len=snprintf((char*)Bytes[N],32,"n-%08zu",N);subscriptionsBenchRequire(Len>0 && Len<32,"fixture name");Names[N]=(xbytesview){Bytes[N],(size_t)Len};}
    uint64 Revision=0;for(size_t N=0;N<Entries;){size_t B=Entries-N<1024?Entries-N:1024;subscriptionsBenchRequire(xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Names+N,B,&Revision)==XREDIS_ADMISSION_ACCEPTED,"seed set");N+=B;}
    free(Names);free(Bytes);xbytesview Extra=XRT_BYTES_LITERAL("extra\0x");uint64 Sum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    size_t BatchOperations=Edit?1:32;double Begin=xrtTimer();
    for(size_t N=0;N<Count;++N){double Start=xrtTimer();for(size_t Repeat=0;Repeat<BatchOperations;++Repeat){
        if(Edit){uint64 Actual=0;subscriptionsBenchRequire(xredisSubscriptionsApply(S,(N&1u)?XREDIS_PUBSUB_CMD_UNSUBSCRIBE:XREDIS_PUBSUB_CMD_SUBSCRIBE,&Extra,1,&Actual)==XREDIS_ADMISSION_ACCEPTED && Actual==++Revision,"atomic edit revision");}
        xredissubscriptionsnapshot* P=NULL;subscriptionsBenchRequire(xredisSubscriptionsSnapshot(S,&P),"snapshot");xredissubscriptionsinfo I={0};I.Size=sizeof(I);subscriptionsBenchRequire(xredisSubscriptionSnapshotInfo(P,&I) && I.Revision==Revision && I.Entries==Entries+(Edit && !(N&1u)),"full immutable set checksum");
        if(I.Entries){xredissubscriptionview V;subscriptionsBenchRequire(xredisSubscriptionSnapshotAt(P,0,&V) && V.Family==XREDIS_PUBSUB_CMD_SUBSCRIBE && V.Name.Size>0,"stable name view");}
        Sum+=I.Entries+1;xredisSubscriptionSnapshotRelease(P);}
        Samples[N]=xrtTimer()-Start;
    }
    double Seconds=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    subscriptionsBenchRequire(Sum==(uint64)Count*BatchOperations*(Entries+1)+(Edit?(Count+1)/2:0),"benchmark checksum");xredisSubscriptionsRelease(S);qsort(Samples,Count,sizeof(*Samples),subscriptionsBenchCompare);
    printf("operations_per_sec: %.3f\np50_us: %.6f\np95_us: %.6f\np99_us: %.6f\np999_us: %.6f\nchecksum: %" PRIu64 "\n",(Count*BatchOperations)/Seconds,Samples[(Count-1)/2]*1e6,Samples[(Count-1)*95/100]*1e6,Samples[(Count-1)*99/100]*1e6,Samples[(Count-1)*999/1000]*1e6,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count/BatchOperations,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count/BatchOperations);
#endif
    printf("batch_operations: %zu\n",BatchOperations);free(Samples);return 0;
}
