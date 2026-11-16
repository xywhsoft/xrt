/* Local, synchronous capacity/latency probe. No sockets or HTTP comparison. */
#include <xtp.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "bench_clock.h"
typedef union bench_memory {size_t Size;long double Align;} bench_memory;
typedef struct bench_alloc {size_t Calls,Live,Peak;} bench_alloc;
static void* alloc(void* u,void* memory,size_t n)
{bench_alloc* a=u;bench_memory* old=memory?(bench_memory*)memory-1:NULL;size_t oldn=old?old->Size:0;
    if(!n){if(old){a->Live-=oldn;free(old);}return NULL;}++a->Calls;
    bench_memory* p=realloc(old,sizeof(*p)+n);if(!p)return NULL;p->Size=n;a->Live=a->Live-oldn+n;
    if(a->Live>a->Peak)a->Peak=a->Live;
    return p+1;}
typedef struct bench_peer {xtpsession* Session;uint64_t Binding,Now;size_t Messages,Completed,Timeouts;double* Latency;double Submitted;} bench_peer;
static volatile uint64_t checksum;
static xtpdecision receive(xtpsession* s,const xtpmessageview* m,void* user)
{bench_peer* p=user;++p->Messages;xbytesview v;
    if(xtpParamAt(&m->Params,1,&v)!=XTP_PARAM_FOUND)abort();
    checksum+=v.Size+m->Body.Size;
    if(m->Command.Kind==XTP_COMMAND_ID){if(m->Command.As.ID!=7)abort();}else if(!xtpCommandIsName(&m->Command,XRT_STR_LITERAL("move")))abort();
    if(m->Type==XTP_REQUEST){xtpresponse r={0};r.Body=m->Body;if(xtpReply(s,m->Request,&r,p->Now)!=XTP_OK)abort();}return xtpAccept();}
static void complete(xtpsession* s,const xtpcompletion* c,void* user)
{bench_peer* p=user;(void)s;p->Latency[p->Completed++]=(xrtTimer()-p->Submitted)*1e6;
    if(c->Reason==XTP_FINISH_TIMEOUT)++p->Timeouts;
    checksum+=c->Body.Size;}
static int compare(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
static size_t move(bench_peer* a,bench_peer* b,unsigned char* copy,size_t* wire,size_t* writes)
{xtpoutput o;size_t frames=0;while(xtpOutput(a->Session,0,&o)==XTP_OK){size_t n=o.Data.Size;memcpy(copy,o.Data.Data,n);
    if(xtpAdvance(a->Session,o.Ticket,n,a->Now)!=XTP_OK)abort();
    xtpreadprogress p;
    if(xtpInput(b->Session,b->Binding,1,(xbytesview){copy,n},false,b->Now,&p)<0||p.Used!=n)abort();
    *wire+=n;++*writes;++frames;}return frames;}
int main(int argc,char** argv)
{
    size_t count=argc>1?strtoul(argv[1],NULL,10):10000,shape=argc>2?strtoul(argv[2],NULL,10):0;
    size_t body=argc>3?strtoul(argv[3],NULL,10):64,batch=argc>4?strtoul(argv[4],NULL,10):8;
    unsigned kind=argc>5?(unsigned)strtoul(argv[5],NULL,10):1,mode=argc>6?(unsigned)strtoul(argv[6],NULL,10):1;
    bool replay=argc>7&&strtoul(argv[7],NULL,10)!=0;
    if(!count||count>1000000||shape>3||body>65536||!batch||batch>32||(kind!=1&&kind!=3&&kind!=5)||mode>1||(replay&&kind==5))return 1;
    unsigned char* payload=calloc(1,body?body:1);unsigned char* wire=malloc((body+128)*batch);
    double* latency=calloc(count,sizeof(*latency));if(!payload||!wire||!latency)return 2;
    xbytesview values[]={XRT_BYTES_INIT("x"),XRT_BYTES_INIT("yz")};xtpfield fields[]={{XRT_STR_INIT("z"),XRT_BYTES_INIT("x")},{XRT_STR_INIT("a"),XRT_BYTES_INIT("yz")}};
    xtpmessage m[32];xtpsubmitoptions opt[32];xtppacket packets[32];memset(opt,0,sizeof(opt));memset(packets,0,sizeof(packets));
    for(size_t i=0;i<batch;++i){m[i]=(xtpmessage){0};m[i].Type=(xtpmessagekind)kind;m[i].Command=shape&1?xtpCmdName(XRT_STR_LITERAL("move")):xtpCmdID(7);
        m[i].Params=shape&2?xtpTable(fields,2):xtpArray(values,2);m[i].Body=(xbytesview){payload,body};opt[i].Replay=replay;
        packets[i].Type=m[i].Type;packets[i].Command=m[i].Command;packets[i].Params=m[i].Params;packets[i].Body=m[i].Body;packets[i].MessageID=kind==5?0:i+1;}
    size_t submitted=0,rejected=0,totalwire=0,writes=0,cold=0,hot=0,peak=0,expired=0;double idle=0;
    bench_alloc allocation={0};bench_peer a={0},b={0};a.Latency=latency;b.Latency=latency;
    if(mode){xtpconfig c;xtpConfigInit(&c);c.MaxResponseBytes=131072;c.MaxTxBytes=8388608;c.MaxSessionBytes=33554432;
        c.Realloc=alloc;c.AllocatorUser=&allocation;if(replay){xtpDeliveryConfigInit(&c.Delivery);c.Delivery.AllowRequests=true;}
        xtpevents e={receive,complete,NULL};if(xtpCreate(&c,&e,&a,&a.Session)!=XTP_OK||xtpCreate(&c,&e,&b,&b.Session)!=XTP_OK)return 3;
        xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpdeliveryproof p={0};p.Identity.SessionID[0]=1;p.Identity.Domain[0]=2;p.Identity.Local[0]=3;p.Identity.Peer[0]=4;
        p.UntilUs=1000000000;p.LifetimeUs=60000000;p.PeerReceiveFloor=p.PeerResultFloor=1;p.StateRetained=p.AllowRequests=true;
        xtpbinding binding={&lane,1,replay,!replay,replay?&p:NULL};
        if(xtpBind(a.Session,&binding,0,&a.Binding)!=XTP_OK)return 4;
        p.Identity.Local[0]=4;p.Identity.Peer[0]=3;
        if(xtpBind(b.Session,&binding,0,&b.Binding)!=XTP_OK)return 4;
        cold=allocation.Calls;}
    double start=xrtTimer(),cpu=xtp_bench_cpu();
    for(size_t base=0;base<count;base+=batch){size_t n=count-base;if(n>batch)n=batch;
        if(!mode){size_t bytes;if(xtpEncode(packets,n,NULL,wire,(body+128)*batch,&bytes)!=XTP_OK)return 5;
            size_t offset=0;for(size_t i=0;i<n;++i){size_t framesize=(size_t)wire[offset+4]|(size_t)wire[offset+5]<<8|(size_t)wire[offset+6]<<16|(size_t)wire[offset+7]<<24;
                xtpframeview view;if(xtpDecode((xbytesview){wire+offset,framesize},NULL,&view)!=XTP_OK)return 6;checksum+=view.Body.Size;offset+=framesize;}
            submitted+=n;totalwire+=bytes;continue;}
        a.Submitted=xrtTimer();a.Now=b.Now=(uint64_t)((a.Submitted-start)*1e6);
        xtpresult r=xtpSubmitBatch(a.Session,m,n,opt,a.Now,NULL);if(r==XTP_E_LIMIT){rejected+=n;continue;}if(r!=XTP_OK)return 7;submitted+=n;
        move(&a,&b,wire,&totalwire,&writes);move(&b,&a,wire,&totalwire,&writes);move(&a,&b,wire,&totalwire,&writes);
    }
    double seconds=xrtTimer()-start,cpuseconds=xtp_bench_cpu()-cpu;
    if(mode){hot=allocation.Calls-cold;xtpinfo ai,bi;xtpInfo(a.Session,&ai);xtpInfo(b.Session,&bi);peak=ai.PeakReservedBytes+bi.PeakReservedBytes;expired=a.Timeouts;
        if(a.Completed!=submitted||b.Messages!=submitted)return 8;
        double t=xrtTimer();for(size_t i=0;i<1000;++i){uint64_t due;if(xtpDrive(a.Session,a.Now,&due)<0)return 9;}idle=(xrtTimer()-t)*1e6/1000;
        if(xtpClose(a.Session,a.Now)!=XTP_CLOSED||xtpClose(b.Session,b.Now)!=XTP_CLOSED||xtpDestroy(a.Session)!=XTP_OK||xtpDestroy(b.Session)!=XTP_OK||allocation.Live)return 10;}
    size_t completed=mode?a.Completed:submitted;if(completed)qsort(latency,completed,sizeof(*latency),compare);
    printf("{\"mode\":%u,\"shape\":%zu,\"body\":%zu,\"batch\":%zu,\"kind\":%u,\"replay\":%u,\"offered\":%zu,\"submitted\":%zu,\"rejected\":%zu,\"completed\":%zu,\"timed_out\":%zu,\"seconds\":%.9f,\"cpu_seconds\":%.9f,\"messages_per_sec\":%.3f,\"useful_bytes\":%zu,\"wire_bytes\":%zu,\"host_copy_bytes\":%zu,\"writes\":%zu,\"cold_allocations\":%zu,\"hot_allocations\":%zu,\"peak_reserved_bytes\":%zu,\"idle_drive_us\":%.3f,\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"p999_us\":%.3f,\"max_us\":%.3f,\"checksum\":%" PRIu64 "}\n",
        mode,shape,body,batch,kind,(unsigned)replay,count,submitted,rejected,completed,expired,seconds,cpuseconds,submitted/seconds,
        submitted*body*(kind==1?2:1),totalwire,mode?totalwire:0,writes,cold,hot,peak,idle,
        completed?latency[completed/2]:0,completed?latency[(completed-1)*95/100]:0,completed?latency[(completed-1)*99/100]:0,completed?latency[(completed-1)*999/1000]:0,completed?latency[completed-1]:0,checksum);
    if(argc>8)printf("messages_per_sec: %.3f\nrejected: %zu\np99_us: %.3f\npeak_reserved_bytes: %zu\n",
        submitted/seconds,rejected,completed?latency[(completed-1)*99/100]:0,peak);
    free(payload);free(wire);free(latency);return 0;
}
