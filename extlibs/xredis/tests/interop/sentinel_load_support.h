#include "../../bench/openloop_schedule.h"

enum { SPR_LOAD_PAYLOAD = 32 };
typedef enum sprloadstage {
    SPR_LOAD_PENDING, SPR_LOAD_CAPACITY, SPR_LOAD_EXPIRED,
    SPR_LOAD_ACQUIRE_REJECTED, SPR_LOAD_ACQUIRE_RESULT, SPR_LOAD_LEASE_EXPIRED,
    SPR_LOAD_CALL_REJECTED, SPR_LOAD_CALL_RESULT
} sprloadstage;

typedef struct sprloadrecord {
    uint64 Due, Submitted, Finished;
    sprloadstage Stage;
    int Status;
    xredisdelivery Delivery;
} sprloadrecord;

typedef struct sprloadslot {
    size_t Index;
    xfuture* Acquire;
    xfuture* Call;
    xredislease* Lease;
} sprloadslot;

static uint64 sprLoadNow(void)
{
    return (uint64)(xrtTimer()*REDIS_OPEN_NS);
}

static bool sprLoadConfig(char** Args, redisopenconfig* Config)
{
    uint64 Values[4];
    for(size_t I=0;I<4;++I){
        char* End;
        if(Args[I][0]<'1' || Args[I][0]>'9')return false;
        Values[I]=strtoull(Args[I],&End,10);
        if(*End || Values[I]>REDIS_OPEN_NS)return false;
    }
    if(Values[0]>300000 || Values[0]<3*Values[1] || Values[2]>1024 || Values[3]>10000)return false;
    *Config=(redisopenconfig){.Count=(size_t)Values[0],.Slots=(size_t)Values[2],.Payload=SPR_LOAD_PAYLOAD,
        .Rate=Values[1],.Timeout=(int64)Values[3]};
    return redisOpenConfigValid(Config);
}

static void sprLoadFinish(sprloadrecord* R,sprloadstage Stage,int Status,xredisdelivery Delivery)
{
    R->Finished=sprLoadNow();
    sprRequire(!R->Stage && R->Due<=R->Submitted && R->Submitted<=R->Finished,"complete one scheduled offer");
    R->Stage=Stage;R->Status=Status;R->Delivery=Delivery;
}

static bool sprLoadReply(const xredisrequestresult* R,size_t Index)
{
    xredisreplyinfo Info={.Size=sizeof(Info)};
    xredisreplyvalue Value;
    if(R->Delivery!=XREDIS_REPLIED || !R->Reply || !xredisReplyInfo(R->Reply,&Info) ||
       !xredisReplyGet(R->Reply,Info.Root,&Value) || Value.Type!=XREDIS_REPLY_BULK_STRING ||
       Value.Data.Size!=SPR_LOAD_PAYLOAD)return false;
    for(size_t I=0;I<Value.Data.Size;++I){
        if(Value.Data.Data[I]!=redisOpenByte(Index,I))return false;
    }
    return true;
}

static bool sprLoadAdvance(sprloadslot* Slot,sprloadrecord* R,uint64 Budget)
{
    if(Slot->Call){
        if(!xrtFutureDone(Slot->Call))return false;
        const xredisrequestresult* Result=xrtFutureValue(Slot->Call);
        sprRequire(Result!=NULL,"typed call outcome");
        if(Result->Status==XREDIS_REQUEST_OK)sprRequire(sprLoadReply(Result,Slot->Index),"binary PING reply matches offer");
        sprLoadFinish(R,SPR_LOAD_CALL_RESULT,Result->Status,Result->Delivery);
        return true;
    }
    if(!xrtFutureDone(Slot->Acquire))return false;
    const xredisleaseresult* Result=xrtFutureValue(Slot->Acquire);
    sprRequire(Result && (Result->Status==XREDIS_REQUEST_OK)==(Result->Lease!=NULL),"typed lease outcome");
    if(Result->Status!=XREDIS_REQUEST_OK){
        sprLoadFinish(R,SPR_LOAD_ACQUIRE_RESULT,Result->Status,XREDIS_NOT_SENT);
        return true;
    }
    Slot->Lease=Result->Lease;
    uint64 Elapsed=sprLoadNow()-R->Due;
    if(Elapsed>=Budget){
        sprLoadFinish(R,SPR_LOAD_LEASE_EXPIRED,0,XREDIS_NOT_SENT);
        return true;
    }
    unsigned char Payload[SPR_LOAD_PAYLOAD];
    for(size_t I=0;I<sizeof(Payload);++I)Payload[I]=redisOpenByte(Slot->Index,I);
    const xbytesview Args[]={XRT_BYTES_LITERAL("PING"),{Payload,sizeof(Payload)}};
    int64 Remaining=(int64)((Budget-Elapsed)/1000000u);
    xredisadmission Status=xredisLeaseCallAsync(Slot->Lease,Args,2,Remaining,&Slot->Call);
    if(Status==XREDIS_ADMISSION_ACCEPTED)return false;
    sprLoadFinish(R,SPR_LOAD_CALL_REJECTED,Status,XREDIS_NOT_SENT);
    return true;
}

static void sprLoad(xredispool* Pool,const redisopenconfig* Config,cstr NextRun,uint16 NextPort,
                    size_t Members,bool Switch,uint64 PreviousHints)
{
    sprloadrecord* Records=calloc(Config->Count,sizeof(*Records));
    sprloadslot* Slots=calloc(Config->Slots,sizeof(*Slots));
    sprRequire(Records && Slots,"bounded load ledger and slots");
    puts("READY");fflush(stdout);
    char Go[16];
    sprRequire(fgets(Go,sizeof(Go),stdin) && !strcmp(Go,"GO\n"),"start scheduled offers");

    uint64 Start=sprLoadNow(),Budget=(uint64)Config->Timeout*1000000u;
    uint64 Trigger=0,Event=0,Online=0,NextSample=Start;
    uint64 End=Start+Config->Count*REDIS_OPEN_NS/Config->Rate;
    uint64 Limit=End+Budget+5u*REDIS_OPEN_NS;
    size_t Index=0,Active=0;
    while(Index<Config->Count || Active || sprLoadNow()<End){
        uint64 Now=sprLoadNow();
        sprRequire(Now<Limit,"bounded load drain");
        if(!Trigger && Now-Start>=REDIS_OPEN_NS){
            Trigger=Now;
            puts("ELECT");fflush(stdout);
        }
        if(Switch && Trigger && (!Event || !Online) && Now>=NextSample){
            xredissentineleventinfo Events={.Size=sizeof(Events)};
            xredissentinelpoolinfo Info={.Size=sizeof(Info)};
            xredispoolinfo PoolInfo={.Size=sizeof(PoolInfo)};
            sprRequire(xredisSentinelPoolEventInfo(Pool,&Events) && xredisSentinelPoolInfo(Pool,&Info),"sample failover state");
            if(!Event && Events.Matched && Events.Hints>PreviousHints)Event=sprLoadNow();
            if(!Online && Info.Generation==2 && Info.Port==NextPort && Info.Phase==XREDIS_SENTINEL_POOL_ONLINE &&
               xredisPoolInfo(Pool,&PoolInfo) && PoolInfo.Live==Members)Online=sprLoadNow();
            NextSample=Now+1000000u;
        }

        for(size_t I=0;I<Active;){
            sprloadslot* Slot=&Slots[I];
            if(!sprLoadAdvance(Slot,&Records[Slot->Index],Budget)){++I;continue;}
            if(Slot->Lease)sprRequire(xredisLeaseReturn(Slot->Lease),"return completed load lease");
            xrtFutureDestroy(Slot->Call);
            xrtFutureDestroy(Slot->Acquire);
            *Slot=Slots[--Active];
        }
        if(Index<Config->Count){
            sprloadrecord* R=&Records[Index];
            sprRequire(redisOpenDue(Start,Index,Config->Rate,&R->Due),"scheduled offer timestamp");
            Now=sprLoadNow();
            if(Now>=R->Due){
                R->Submitted=Now;
                if(Now-R->Due>=Budget)sprLoadFinish(R,SPR_LOAD_EXPIRED,0,XREDIS_NOT_SENT);
                else if(Active==Config->Slots)sprLoadFinish(R,SPR_LOAD_CAPACITY,0,XREDIS_NOT_SENT);
                else{
                    sprloadslot* Slot=&Slots[Active];
                    *Slot=(sprloadslot){.Index=Index};
                    int64 Remaining=(int64)((Budget-(Now-R->Due))/1000000u);
                    xredisadmission Status=xredisPoolAcquireAsync(Pool,Remaining,&Slot->Acquire);
                    if(Status==XREDIS_ADMISSION_ACCEPTED)++Active;
                    else sprLoadFinish(R,SPR_LOAD_ACQUIRE_REJECTED,Status,XREDIS_NOT_SENT);
                }
                ++Index;
                continue;
            }
        }
        xrtThreadYield();
    }
    uint64 Finished=sprLoadNow();
    printf("LOAD-MARKERS %llu %llu %llu %llu\n",(unsigned long long)(Finished-Start),
        (unsigned long long)(Trigger-Start),(unsigned long long)(Event?Event-Start:0),
        (unsigned long long)(Online?Online-Start:0));
    for(size_t I=0;I<Config->Count;++I){
        sprloadrecord* R=&Records[I];
        sprRequire(R->Stage!=SPR_LOAD_PENDING,"offer has one terminal record");
        printf("LOAD %zu %llu %llu %llu %d %d %d\n",I,(unsigned long long)(R->Due-Start),
            (unsigned long long)(R->Submitted-Start),(unsigned long long)(R->Finished-Start),
            R->Stage,R->Status,R->Delivery);
    }
    fflush(stdout);
    xredissentineleventinfo Events={.Size=sizeof(Events)};
    xredissentinelpoolinfo Info={.Size=sizeof(Info)};
    sprRequire(xredisSentinelPoolInfo(Pool,&Info) && xredisSentinelPoolEventInfo(Pool,&Events),
        "final loaded pool snapshots");
    fprintf(stdout,"LOAD-TOPOLOGY %llu %llu %u %d %d %llu\n",
        (unsigned long long)Info.Generation,(unsigned long long)Info.Changes,
        (unsigned)Info.Port,(int)Info.Phase,(int)Info.LastStatus,(unsigned long long)Events.Matched);
    sprRequire(Info.Port==NextPort && Info.Generation==(Switch?2u:1u) && Info.Changes==(Switch?1u:0u) &&
        Info.Phase==XREDIS_SENTINEL_POOL_ONLINE && Info.LastStatus==XREDIS_SENTINEL_OK,
        "final verified loaded generation");
    sprRequire(Trigger && (Switch?(Event && Online && Online+REDIS_OPEN_NS<=End):!Events.Matched),
        "observed event and at least one second of post-recovery offers");
    xfuture* Acquired=sprAcquire(Pool);
    xredislease* Lease=sprLease(Acquired);
    xrtFutureDestroy(sprIdentity(Lease,NextRun));
    sprRequire(xredisLeaseReturn(Lease),"return final identity lease");
    xrtFutureDestroy(Acquired);

    free(Slots);
    free(Records);
}
