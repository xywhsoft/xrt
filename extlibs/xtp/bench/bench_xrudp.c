/* Actual UDP/Noise capacity probe. Closed-loop, one serialized executor.
 * Optional impairment uses the same seeded flight queue as the game tests. */
#include <stdlib.h>
#include <time.h>
#include "bench_clock.h"
#include "../examples/support/game_loopback.h"
typedef struct probe {size_t Submitted,Calls,Plain;double *Begin,*Latency;uint64_t Wire,Packets;} probe;
static probe probes[2];
static size_t count=2000,body=64,window=32;
static unsigned kind=1;
static xrudpresult probe_send(const void* data,size_t n,void* user)
{
    game_peer* p=user;probe* q=&probes[p->Index];
    if(p->Test->Loss||p->Test->Rtt){xrudpresult r=game_send(data,n,user);if(r==XRUDP_OK){q->Wire+=n;++q->Packets;}return r;}
    size_t sent;xnetresult r=xrtNetSocketSendTo(p->Test->Sockets[p->Index],data,n,&sent,&p->Test->Addresses[1-p->Index]);
    if(r==XNET_RESULT_AGAIN)return XRUDP_BACKPRESSURE;
    assert(r==XNET_RESULT_OK&&sent==n);q->Wire+=n;++q->Packets;return XRUDP_OK;
}
static xtpdecision probe_receive(xtpsession* s,const xtpmessageview* m,void* user)
{game_peer* p=user;assert(m->Type==kind&&m->Body.Size==body);++p->Commands;
    if(m->Type==XTP_REQUEST){xtpresponse r={0};r.Body=m->Body;assert(xtpReply(s,m->Request,&r,p->Test->Now)==XTP_OK);}return xtpAccept();}
static void probe_complete(xtpsession* s,const xtpcompletion* c,void* user)
{game_peer* p=user;probe* q=&probes[p->Index];(void)s;if(c->Reason==XTP_FINISH_CLOSED)return;
    assert(c->Reason==(kind==1?XTP_FINISH_RESPONSE:kind==3?XTP_FINISH_ACK:XTP_FINISH_SENT));
    q->Latency[p->Completed++]=(xrtTimer()-*(double*)c->Tag)*1e6;}
static void probe_fill(game_peer* p,const unsigned char* payload)
{probe* q=&probes[p->Index];size_t n=window-(q->Submitted-p->Completed);if(n>count-q->Submitted)n=count-q->Submitted;
    while(n){size_t batch=n>32?32:n;xtpmessage m[32];xtpsubmitoptions options[32];memset(options,0,sizeof(options));
        for(size_t i=0;i<batch;++i){q->Begin[q->Submitted+i]=xrtTimer();m[i]=(xtpmessage){0};m[i].Type=(xtpmessagekind)kind;
            m[i].Command=xtpCmdID(1001);m[i].Body=(xbytesview){payload,body};options[i].LaneID=kind==5?2:1;options[i].Tag=&q->Begin[q->Submitted+i];}
        assert(xtpSubmitBatch(p->Xtp,m,batch,options,p->Test->Now,NULL)==XTP_OK);q->Submitted+=batch;n-=batch;}}
static void probe_io(game_peer* p)
{unsigned char bytes[16384];size_t n;while(xrtRudpSessionRead(p->Rudp,bytes,sizeof(bytes),&n)==XRUDP_OK&&n)game_input(p,1,bytes,n,false);
    xrudpdatagraminfo info;while(xrtRudpSessionDatagramRead(p->Rudp,bytes,sizeof(bytes),p->Test->Now,&info)==XRUDP_OK)game_input(p,2,bytes,info.Size,true);
    uint64_t due;xtpresult r=xtpDrive(p->Xtp,p->Test->Now,&due);assert(r==XTP_OK||r==XTP_AGAIN);
    xtpxrudpwriter writer={p->Rudp,1,1,p->Test->Now};xtpwritebudget budget={1048576,64,16384};xtpwriteprogress progress;
    r=xtpDrain(p->Xtp,kind==5?2:1,xtpXrudpWrite,&writer,&budget,p->Test->Now,&progress);assert(r==XTP_OK||r==XTP_AGAIN);
    probes[p->Index].Calls+=progress.Calls;probes[p->Index].Plain+=progress.Bytes;
    xrudpresult native=xrtRudpSessionPoll(p->Rudp,p->Test->Now,&due);assert(native==XRUDP_OK||native==XRUDP_BACKPRESSURE);}
static int compare_latency(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
int main(int argc,char** argv)
{
    (void)game_run; /* retain shared fixture's static helpers without executing its scenario */
    count=argc>1?strtoul(argv[1],NULL,10):2000;body=argc>2?strtoul(argv[2],NULL,10):64;
    window=argc>3?strtoul(argv[3],NULL,10):32;kind=argc>4?(unsigned)strtoul(argv[4],NULL,10):1;
    unsigned loss=argc>5?(unsigned)strtoul(argv[5],NULL,10):0,rtt=argc>6?(unsigned)strtoul(argv[6],NULL,10):0;
    if(!count||count>1000000||body>65536||!window||window>32||(kind!=1&&kind!=3&&kind!=5)||(kind==5&&body>256)||loss>10||rtt>200)return 1;
    game_test t;memset(&t,0,sizeof(t));t.Loss=loss;t.Rtt=rtt;t.Random=0x9e3779b9u+loss+rtt;
    unsigned char* payload=calloc(body?body:1,1);assert(payload);
    for(size_t i=0;i<2;++i){probes[i].Begin=calloc(count,sizeof(double));probes[i].Latency=calloc(count,sizeof(double));assert(probes[i].Begin&&probes[i].Latency);
        t.Sockets[i]=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);assert(t.Sockets[i]);
        xnetaddr a;assert(xrtNetAddrParse(&a,"127.0.0.1",0)&&xrtNetSocketBind(t.Sockets[i],&a)&&xrtNetSocketLocal(t.Sockets[i],&t.Addresses[i]));}
    xrudpsessionconfig c;xrtRudpSessionConfigInit(&c);c.ReceiveCapacity=c.TransmitCapacity=262144;c.StreamCapacity=1;
    c.Local.StreamCredit=c.Peer.StreamCredit=c.Local.ConnectionCredit=c.Peer.ConnectionCredit=262144;c.Local.MaxStreams=c.Peer.MaxStreams=1;
    c.Local.DatagramSize=c.Peer.DatagramSize=1200;c.PacketCapacity=128;c.MaximumWindow=262144;c.InitialRttUs=rtt?rtt*1000:1000;
    xrtRudpDatagramConfigInit(&c.Datagrams);c.Datagrams.Channels=1;c.Datagrams.PayloadCapacity=512;
    xrudpcipher* cipher[2];game_handshake(&t,&c,cipher);
    for(unsigned i=0;i<2;++i){game_peer* p=&t.Peers[i];p->Test=&t;p->Index=i;c.Role=i?XRUDP_RESPONDER:XRUDP_INITIATOR;
        assert(xrtRudpSessionCreate(&c,cipher[i],0,probe_send,p,&p->Rudp)==XRUDP_OK);
        xrudpdatagramchannel channel;xrtRudpDatagramChannelInit(&channel);channel.Id=1;channel.Mode=XRUDP_DATAGRAM_INDEPENDENT;
        assert(xrtRudpSessionDatagramConfigure(p->Rudp,&channel)==XRUDP_OK);
        xtpconfig config;xtpConfigInit(&config);config.DefaultOperationTimeoutUs=config.IncomingRequestTimeoutUs=120000000;
        config.MaxResponseBytes=131072;config.MaxTxBytes=16777216;config.MaxSessionBytes=33554432;
        xtpevents events={probe_receive,probe_complete,game_fault};assert(xtpCreate(&config,&events,p,&p->Xtp)==XTP_OK);
        xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200}};xtpbinding binding={lanes,2,true,false,NULL};
        assert(xtpBind(p->Xtp,&binding,0,&p->Binding)==XTP_OK);}
    double begin=xrtTimer(),cpu=xtp_bench_cpu();uint64_t completed_at=0;
    for(;;){t.Now=(uint64_t)((xrtTimer()-begin)*1e6);assert(t.Now<120000000);game_udp(&t);
        for(size_t i=0;i<2;++i){probe_fill(&t.Peers[i],payload);probe_io(&t.Peers[i]);}
        if(t.Peers[0].Completed==count&&t.Peers[1].Completed==count){if(!completed_at)completed_at=t.Now;
            if(kind!=5||t.Now-completed_at>250000+(uint64_t)rtt*2000)break;}}
    double seconds=(double)completed_at/1e6,cpu_seconds=xtp_bench_cpu()-cpu;
    for(size_t i=0;i<2;++i){game_peer* p=&t.Peers[i];probe* q=&probes[i];xtpinfo info;assert(xtpInfo(p->Xtp,&info)==XTP_OK);
        xrudpsessionstate state;assert(xrtRudpSessionState(p->Rudp,&state)==XRUDP_OK);qsort(q->Latency,count,sizeof(double),compare_latency);
        if(kind!=5)assert(p->Commands==count);
        printf("{\"kind\":%u,\"peer\":%zu,\"body\":%zu,\"window\":%zu,\"loss_percent\":%u,\"rtt_ms\":%u,\"offered\":%zu,\"completed\":%zu,\"received\":%zu,\"seconds\":%.9f,\"messages_per_sec\":%.3f,\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"p999_us\":%.3f,\"max_us\":%.3f,\"cpu_seconds_process\":%.6f,\"writer_calls\":%zu,\"plaintext_bytes\":%zu,\"admitted_udp_bytes\":%llu,\"admitted_packets\":%llu,\"transport_lost_packets\":%llu,\"peak_xtp_bytes\":%zu,\"batch_copied_bytes\":%llu}\n",
            kind,i,body,window,loss,rtt,count,p->Completed,p->Commands,seconds,count/seconds,q->Latency[count/2],q->Latency[(count-1)*95/100],q->Latency[(count-1)*99/100],q->Latency[(count-1)*999/1000],q->Latency[count-1],cpu_seconds,q->Calls,q->Plain,
            (unsigned long long)q->Wire,(unsigned long long)q->Packets,(unsigned long long)state.LostPackets,info.PeakReservedBytes,(unsigned long long)info.BatchCopiedBytes);
        xtpresult r=xtpClose(p->Xtp,t.Now);while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(p->Xtp,t.Now,&due);}assert(r==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);
        xrtRudpSessionDestroy(p->Rudp);assert(xrtNetSocketClose(t.Sockets[i]));free(q->Begin);free(q->Latency);}
    free(payload);return 0;
}
