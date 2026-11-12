#include <xredis/command_keys.h>
#include <xrt.h>
#include "../src/internal/cluster_slot.h"
#include "../tests/support/command_keys_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static void ckBenchRequire(bool OK){if(!OK){fputs("[FAIL] command key benchmark\n",stderr);exit(1);}}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):0;bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;
    if(!Count || Count>100000000 || Mode>3 || Argc>4)return 2;
    const char* Wire=Mode&1?CK_METADATA2:CK_METADATA3;xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;xrediscommandcatalog* C=NULL;
    ckBenchRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,strlen(Wire)},&Used,&Reply)==XREDIS_REPLY_READY && xredisCommandCatalogCreate(Reply,NULL,&C)==XREDIS_KEYS_OK);xredisReplyReaderDestroy(Reader);
    xbytesview A[16][9];char Names[16][4][32];uint16 Slots[16];uint64 Expected=0;
    for(unsigned I=0;I<16;++I){A[I][0]=(xbytesview)XRT_BYTES_LITERAL("MSET");for(unsigned K=0;K<4;++K){int N=snprintf(Names[I][K],sizeof(Names[I][K]),"key%u:{tag%u}",K,I);A[I][1+2*K]=(xbytesview){(cbytes)Names[I][K],(size_t)N};A[I][2+2*K]=(xbytesview)XRT_BYTES_LITERAL("value");}Slots[I]=xredisClusterSlotRaw(A[I][1]);}
    for(size_t I=0;I<Count;++I)Expected+=Mode==2?20:Slots[I&15];
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Begin=xrtTimer();
    for(size_t I=0;I<Count;++I){
        if(Mode==2){xrediscommandcatalog* T=NULL;xrediscommandcataloginfo Info={0};Info.Size=sizeof(Info);ckBenchRequire(xredisCommandCatalogCreate(Reply,NULL,&T)==XREDIS_KEYS_OK && xredisCommandCatalogInfo(T,&Info));Sum+=Info.Commands;xredisCommandCatalogRelease(T);}
        else if(Mode==3){uint16 Slot=0;for(unsigned K=0;K<4;++K){uint16 N=xredisClusterSlotRaw(A[I&15][1+2*K]);if(K)ckBenchRequire(N==Slot);Slot=N;}Sum+=Slot;}
        else {xrediscommandkeyresult R={0};R.Size=sizeof(R);xrediscommandkey Keys[4];ckBenchRequire(xredisCommandCatalogEvaluate(C,A[I&15],9,Mode?Keys:NULL,Mode?4:0,&R)==XREDIS_KEYS_OK && R.Keys==4);Sum+=R.Slot;}
    }
    double Elapsed=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    ckBenchRequire(Sum==Expected);printf("operations_per_sec: %.3f\nns_per_operation: %.3f\nchecksum: %" PRIu64 "\n",Count/Elapsed,Elapsed*1e9/Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xredisCommandCatalogRelease(C);xredisReplyRelease(Reply);return 0;
}
