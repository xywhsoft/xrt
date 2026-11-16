/* Actual localhost UDP + authenticated xrudp. Fixed keys and virtual path
 * impairment are test fixtures; deployment keys/identity come from the host. */
#include <xtp.h>
#include <xrt/net.h>
#include <xrt/thread.h>
#include <xrt/crypto.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct game_test game_test;
typedef struct game_peer {
    game_test* Test;unsigned Index;xtpsession* Xtp;xrudpsession* Rudp;
    uint64_t Binding,LastSnapshot;size_t Commands,Completed,Snapshots,LocalDrops,SentSnapshots;
    uint32_t ResourceStream,BulkMask;size_t SmallCompleted;uint64_t SmallDoneAt;
    bool SnapshotDuringBulk;
} game_peer;
typedef struct game_packet {unsigned char Data[1200];size_t Size;uint64_t Due;unsigned Source;bool Used;} game_packet;
struct game_test {
    game_peer Peers[2];xnetsocket Sockets[2];xnetaddr Addresses[2];game_packet Queue[256];
    uint64_t Now,NextTick;uint32_t Random,Loss,Rtt,PathLimit;size_t Lost,Duplicated;unsigned char Body[32768];
};
static uint32_t game_random(game_test* t){uint32_t n=t->Random;n^=n<<13;n^=n>>17;n^=n<<5;return t->Random=n;}
static uint64_t game_clock(void){return (uint64_t)(xrtTimer()*1000000.0);}
static xrudpresult game_send(const void* bytes,size_t size,void* user)
{
    game_peer* p=user;game_test* t=p->Test;assert(size<=1200);
    if(t->PathLimit&&size>t->PathLimit){++t->Lost;return XRUDP_OK;}
    if(game_random(t)%100<t->Loss){++t->Lost;return XRUDP_OK;}
    size_t free_slots=0;for(size_t i=0;i<256;++i)if(!t->Queue[i].Used)++free_slots;
    unsigned copies=game_random(t)%17==0?2:1;if(free_slots<copies)return XRUDP_BACKPRESSURE;
    if(copies==2)++t->Duplicated;
    for(unsigned k=0;k<copies;++k){size_t i=0;while(t->Queue[i].Used)++i;game_packet* q=&t->Queue[i];
        q->Used=true;q->Source=p->Index;q->Size=size;q->Due=t->Now+(uint64_t)t->Rtt*450+game_random(t)%((t->Rtt*100)+1);
        memcpy(q->Data,bytes,size);}
    return XRUDP_OK;
}
static void game_flight(game_test* t,unsigned source,void* packet,size_t n)
{
    size_t sent;assert(xrtNetSocketSendTo(t->Sockets[source],packet,n,&sent,&t->Addresses[1-source])==XNET_RESULT_OK&&sent==n);
    double until=xrtTimer()+2;size_t received;xnetaddr from;
    for(;;){xnetresult r=xrtNetSocketRecvFrom(t->Sockets[1-source],packet,312,&received,&from);
        if(r==XNET_RESULT_AGAIN){assert(xrtTimer()<until);xrtSleep(1);continue;}
        assert(r==XNET_RESULT_OK&&received==n&&xrtNetAddrEqual(&from,&t->Addresses[source]));break;}
}
static void game_handshake(game_test* t,xrudpsessionconfig* cfg,xrudpcipher* cipher[2])
{
    unsigned char keys[2][32]={{3},{11}},pub[2][32],cookie[32]={17},scope[16]={23};
    unsigned char packet[312],retry[312];size_t n,rn;uint64_t due;
    xrudphandshakeconfig c[2];xrudphandshake* h[2]={NULL,NULL};xrudpaddress source={0};
    source.Family=4;source.Port=t->Addresses[0].Port;memcpy(source.Address,t->Addresses[0].Address,16);
    for(size_t i=0;i<2;++i)assert(xrtX25519Public(keys[i],pub[i]));
    for(size_t i=0;i<2;++i){memset(&c[i],0,sizeof(c[i]));c[i].StaticPrivate=keys[i];c[i].LocalPublic=pub[i];c[i].PeerPublic=pub[1-i];c[i].Scope=scope;c[i].Parameters=cfg->Local;}
    assert(xrtRudpHandshakeCreate(&c[0],0,&h[0])==XRUDP_OK);
    assert(xrtRudpHandshakeNext(h[0],0,packet,sizeof(packet),&n,&due)==XRUDP_OK);
    game_flight(t,0,packet,n);
    assert(xrtRudpHandshakeRetry(cookie,scope,pub[1],&source,0,packet,n,retry,sizeof(retry),&rn)==XRUDP_OK);
    game_flight(t,1,retry,rn);
    assert(xrtRudpHandshakeInput(h[0],retry,rn,0)==XRUDP_OK);
    assert(xrtRudpHandshakeNext(h[0],0,packet,sizeof(packet),&n,&due)==XRUDP_OK);
    game_flight(t,0,packet,n);
    assert(xrtRudpHandshakeAccept(&c[1],cookie,NULL,&source,0,packet,n,&h[1])==XRUDP_OK);
    assert(xrtRudpHandshakeNext(h[1],0,packet,sizeof(packet),&n,&due)==XRUDP_OK);
    game_flight(t,1,packet,n);
    assert(xrtRudpHandshakeInput(h[0],packet,n,0)==XRUDP_OK);
    assert(xrtRudpHandshakeNext(h[0],0,packet,sizeof(packet),&n,&due)==XRUDP_OK);
    game_flight(t,0,packet,n);
    assert(xrtRudpHandshakeInput(h[1],packet,n,0)==XRUDP_OK);
    assert(xrtRudpHandshakeNext(h[1],0,packet,sizeof(packet),&n,&due)==XRUDP_OK);
    game_flight(t,1,packet,n);
    assert(xrtRudpHandshakeInput(h[0],packet,n,0)==XRUDP_OK);
    for(size_t i=0;i<2;++i){xrudphandshakeinfo info;assert(xrtRudpHandshakeInfo(h[i],&info)==XRUDP_OK&&info.Phase==XRUDP_HANDSHAKE_ESTABLISHED);
        assert(!memcmp(info.PeerPublic,pub[1-i],32));assert(xrtRudpHandshakeTakeCipher(h[i],&cipher[i])==XRUDP_OK);xrtRudpHandshakeDestroy(h[i]);}
}
static uint32_t game_get32(const unsigned char* p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void game_put32(unsigned char* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(i*8));}
static xtpdecision game_business(xtpsession* s,const xtpmessageview* m,void* user)
{
    game_peer* p=user;game_test* t=p->Test;assert(m->Command.Kind==XTP_COMMAND_ID);
    if(m->Command.As.ID==2){xbytesview tick,entity;assert(m->Type==XTP_NOTIFY&&m->Params.Kind==XTP_PARAMS_ARRAY);
        assert(xtpParamAt(&m->Params,0,&tick)==XTP_PARAM_FOUND&&tick.Size==4&&xtpParamAt(&m->Params,1,&entity)==XTP_PARAM_FOUND&&entity.Size==4);
        uint32_t n=game_get32(tick.Data);assert(n>p->LastSnapshot);p->LastSnapshot=n;
        if(t->Now>(uint64_t)(n-1)*16667+200000)return xtpAccept(); /* Shared test-world tick; application expiry. */
        ++p->Snapshots;if(p->BulkMask!=255)p->SnapshotDuringBulk=true;
    }else{++p->Commands;
        if(m->Type==XTP_REQUEST){xtpresponse response={0};response.Body=m->Body;
            xbytesview block={0};if(m->Command.As.ID==4){assert(xtpParamAt(&m->Params,0,&block)==XTP_PARAM_FOUND&&block.Size==4);
                uint32_t i=game_get32(block.Data);assert(i<8&&m->Body.Size==4096&&!memcmp(m->Body.Data,t->Body+i*4096,4096));response.Params=xtpArray(&block,1);}
            else assert(m->Command.As.ID==1&&m->Body.Size==3&&!memcmp(m->Body.Data,"rpc",3));
            assert(xtpReply(s,m->Request,&response,t->Now)==XTP_OK);}
        else assert(m->Type==XTP_PUSH&&m->Body.Size==3);}
    return xtpAccept();
}
static void game_complete(xtpsession* s,const xtpcompletion* c,void* user)
{
    game_peer* p=user;(void)s;if(c->Reason==XTP_FINISH_CLOSED)return;
    if(c->Type==XTP_NOTIFY){if(c->Reason==XTP_FINISH_LOCAL_ERROR){assert(c->LocalResult==XTP_E_LIMIT&&c->Knowledge==XTP_KNOW_NOT_SENT);++p->LocalDrops;}
        else assert(c->Reason==XTP_FINISH_SENT||c->Reason==XTP_FINISH_TIMEOUT);
        return;}
    assert(c->Reason==XTP_FINISH_RESPONSE||c->Reason==XTP_FINISH_ACK);
    if(c->Reason==XTP_FINISH_RESPONSE&&c->Params.Count){xbytesview block;assert(xtpParamAt(&c->Params,0,&block)==XTP_PARAM_FOUND&&block.Size==4);
        uint32_t i=game_get32(block.Data);assert(i<8&&!(p->BulkMask&(1u<<i))&&c->Body.Size==4096&&!memcmp(c->Body.Data,p->Test->Body+i*4096,4096));p->BulkMask|=1u<<i;}
    else {if(c->Reason==XTP_FINISH_RESPONSE)assert(c->Body.Size==3&&!memcmp(c->Body.Data,"rpc",3));
        ++p->SmallCompleted;if(p->SmallCompleted==2)p->SmallDoneAt=p->Test->Now;}
    ++p->Completed;
}
static void game_fault(xtpsession* s,const xtpfault* f,void* user)
{(void)s;(void)user;fprintf(stderr,"game XTP fault %d\n",(int)f->Reason);assert(!f->Fatal);}
static void game_input(game_peer* p,uint32_t lane,const unsigned char* bytes,size_t n,bool end)
{
    size_t offset=0;while(offset<n){xtpreadprogress progress;xtpresult r=xtpInput(p->Xtp,p->Binding,lane,(xbytesview){bytes+offset,n-offset},end,p->Test->Now,&progress);
        assert(r>=0&&r!=XTP_PAUSED&&r!=XTP_CLOSED);offset+=progress.Used;}
}
static void game_udp(game_test* t)
{
    for(size_t i=0;i<256;++i){game_packet* q=&t->Queue[i];if(!q->Used||q->Due>t->Now)continue;size_t sent;
        xnetresult r=xrtNetSocketSendTo(t->Sockets[q->Source],q->Data,q->Size,&sent,&t->Addresses[1-q->Source]);
        if(r==XNET_RESULT_AGAIN)continue;
        assert(r==XNET_RESULT_OK&&sent==q->Size);q->Used=false;}
    for(size_t i=0;i<2;++i)for(size_t k=0;k<256;++k){unsigned char packet[1200];size_t n;xnetaddr from;
        xnetresult r=xrtNetSocketRecvFrom(t->Sockets[i],packet,sizeof(packet),&n,&from);if(r==XNET_RESULT_AGAIN)break;
        assert(r==XNET_RESULT_OK&&xrtNetAddrEqual(&from,&t->Addresses[1-i]));
        xrudpresult input=xrtRudpSessionInput(t->Peers[i].Rudp,packet,n,t->Now);
        assert(input==XRUDP_OK||input==XRUDP_DUPLICATE||input==XRUDP_TOO_OLD||input==XRUDP_BACKPRESSURE);}
}
static void game_io(game_peer* p)
{
    game_test* t=p->Test;unsigned char bytes[1200];size_t n;
    while(xrtRudpSessionRead(p->Rudp,bytes,97,&n)==XRUDP_OK&&n)game_input(p,1,bytes,n,false);
    uint32_t resource=p->Test->Peers[1-p->Index].ResourceStream;xrudpstreamstate stream;
    if(resource&&xrtRudpSessionStreamState(p->Rudp,resource,&stream)==XRUDP_OK)
        while(xrtRudpSessionStreamRead(p->Rudp,resource,bytes,97,&n)==XRUDP_OK&&n)game_input(p,3,bytes,n,false);
    xrudpdatagraminfo info;while(xrtRudpSessionDatagramRead(p->Rudp,bytes,sizeof(bytes),t->Now,&info)==XRUDP_OK)game_input(p,2,bytes,info.Size,true);
    uint64_t due;xtpresult r=xtpDrive(p->Xtp,t->Now,&due);assert(r==XTP_OK||r==XTP_AGAIN);
    xtpxrudpwriter writer={p->Rudp,1,1,t->Now};xtpwritebudget budget={4096,8,257};xtpwriteprogress progress;
    r=xtpDrain(p->Xtp,1,xtpXrudpWrite,&writer,&budget,t->Now,&progress);assert(r==XTP_OK||r==XTP_AGAIN);
    r=xtpDrain(p->Xtp,2,xtpXrudpWrite,&writer,&budget,t->Now,&progress);assert(r==XTP_OK||r==XTP_AGAIN||r==XTP_DROPPED);
    if(p->ResourceStream){writer.StreamID=p->ResourceStream;r=xtpDrain(p->Xtp,3,xtpXrudpWrite,&writer,&budget,t->Now,&progress);assert(r==XTP_OK||r==XTP_AGAIN);}
    xrudpresult native=xrtRudpSessionPoll(p->Rudp,t->Now,&due);assert(native==XRUDP_OK||native==XRUDP_BACKPRESSURE);
}
static void game_snapshot(game_peer* p,uint32_t tick)
{
    unsigned char encoded[8];game_put32(encoded,tick);game_put32(encoded+4,p->Index+1);
    xbytesview args[]={{encoded,4},{encoded+4,4}};xtpmessage m={0};m.Type=XTP_NOTIFY;m.Command=xtpCmdID(2);m.Params=xtpArray(args,2);
    xtpsubmitoptions options={0};options.LaneID=2;options.DeadlineUs=p->Test->Now+50000;
    xtpresult r=xtpSubmit(p->Xtp,&m,&options,p->Test->Now,NULL);assert(r==XTP_OK||r==XTP_E_LIMIT);if(r==XTP_OK)++p->SentSnapshots;
}
static void game_run(unsigned loss,unsigned rtt)
{
    game_test t;memset(&t,0,sizeof(t));t.Loss=loss;t.Rtt=rtt;t.Random=0x9e3779b9u+loss+rtt;
    for(size_t i=0;i<sizeof(t.Body);++i)t.Body[i]=(unsigned char)(i*37u);
    for(size_t i=0;i<2;++i){t.Sockets[i]=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);assert(t.Sockets[i]);
        xnetaddr address;assert(xrtNetAddrParse(&address,"127.0.0.1",0)&&xrtNetSocketBind(t.Sockets[i],&address)&&xrtNetSocketLocal(t.Sockets[i],&t.Addresses[i]));}
    xrudpsessionconfig c;xrtRudpSessionConfigInit(&c);c.ReceiveCapacity=65536;c.TransmitCapacity=32768;c.StreamCapacity=2;
    c.Local.StreamCredit=c.Peer.StreamCredit=c.Local.ConnectionCredit=c.Peer.ConnectionCredit=65536;c.Local.MaxStreams=c.Peer.MaxStreams=2;
    c.Local.DatagramSize=c.Peer.DatagramSize=1200;c.PacketCapacity=32;c.InitialRttUs=(uint64_t)rtt*1000;c.MaximumWindow=262144;
    xrtRudpDatagramConfigInit(&c.Datagrams);c.Datagrams.Channels=1;c.Datagrams.PayloadCapacity=256;
    xrudpcipher* cipher[2];game_handshake(&t,&c,cipher);
    for(unsigned i=0;i<2;++i){game_peer* p=&t.Peers[i];p->Test=&t;p->Index=i;c.Role=i?XRUDP_RESPONDER:XRUDP_INITIATOR;
        assert(xrtRudpSessionCreate(&c,cipher[i],0,game_send,p,&p->Rudp)==XRUDP_OK);
        xrudpstreamconfig resource;xrtRudpStreamConfigInit(&resource);resource.Priority=3;
        assert(xrtRudpSessionStreamOpen(p->Rudp,&resource,&p->ResourceStream)==XRUDP_OK);
        xrudpdatagramchannel channel;xrtRudpDatagramChannelInit(&channel);channel.Id=1;channel.Mode=XRUDP_DATAGRAM_LATEST;channel.ReplacePending=true;channel.ReceiveLifetimeUs=50000;
        assert(xrtRudpSessionDatagramConfigure(p->Rudp,&channel)==XRUDP_OK);
        xtpconfig config;xtpConfigInit(&config);config.DefaultOperationTimeoutUs=config.IncomingRequestTimeoutUs=30000000;config.Reader.ReceiveTimeoutUs=15000000;
        xtpevents events={game_business,game_complete,game_fault};assert(xtpCreate(&config,&events,p,&p->Xtp)==XTP_OK);
        xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200},{3,XTP_INPUT_STREAM,true,0}};xtpbinding binding={lanes,3,true,false,NULL};
        assert(xtpBind(p->Xtp,&binding,0,&p->Binding)==XTP_OK);
        xtpmessage m={0};m.Type=XTP_REQUEST;m.Command=xtpCmdID(1);m.Body=XRT_BYTES_LITERAL("rpc");
        assert(xtpSubmit(p->Xtp,&m,NULL,0,NULL)==XTP_OK);m.Type=XTP_PUSH;m.Command=xtpCmdID(3);m.Body=XRT_BYTES_LITERAL("cmd");assert(xtpSubmit(p->Xtp,&m,NULL,0,NULL)==XTP_OK);
        /* Configured record limit is intentionally larger than current native
         * payload capacity: reject this one frame without closing stream 1. */
        unsigned char too_big[300]={0};m.Type=XTP_NOTIFY;m.Command=xtpCmdID(2);m.Params=xtpArray(NULL,0);m.Body=(xbytesview){too_big,sizeof(too_big)};
        xtpsubmitoptions options={0};options.LaneID=2;assert(xtpSubmit(p->Xtp,&m,&options,0,NULL)==XTP_OK);
        for(uint32_t block=0;block<8;++block){unsigned char index[4];game_put32(index,block);xbytesview arg={index,4};
            m=(xtpmessage){0};m.Type=XTP_REQUEST;m.Command=xtpCmdID(4);m.Params=xtpArray(&arg,1);m.Body=(xbytesview){t.Body+block*4096,4096};
            options.LaneID=3;assert(xtpSubmit(p->Xtp,&m,&options,0,NULL)==XTP_OK);}
    }
    uint64_t begin=game_clock();uint32_t tick=0;
    for(;;){t.Now=game_clock()-begin;assert(t.Now<30000000);game_udp(&t);
        while(t.Now>=t.NextTick){++tick;for(size_t i=0;i<2;++i)game_snapshot(&t.Peers[i],tick);t.NextTick+=16667;}
        for(size_t i=0;i<2;++i)game_io(&t.Peers[i]);
        if(t.Now>=1000000&&t.Peers[0].Completed==10&&t.Peers[1].Completed==10&&t.Peers[0].Snapshots>=15&&t.Peers[1].Snapshots>=15)break;
        xrtSleep(1);
    }
    for(size_t i=0;i<2;++i){game_peer* p=&t.Peers[i];assert(p->Commands==10&&p->LocalDrops==1&&p->SnapshotDuringBulk&&p->BulkMask==255&&p->SmallCompleted==2);
        xtpresult r=xtpClose(p->Xtp,t.Now);while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(p->Xtp,t.Now,&due);}assert(r==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);
        xrtRudpSessionDestroy(p->Rudp);assert(xrtNetSocketClose(t.Sockets[i]));}
    printf("XTP game UDP: loss=%u%% RTT=%ums snapshots=%zu/%zu dropped_packets=%zu duplicates=%zu commands=%.3f/%.3fs total=%.3fs PASS\n",loss,rtt,t.Peers[0].Snapshots,t.Peers[1].Snapshots,t.Lost,t.Duplicated,(double)t.Peers[0].SmallDoneAt/1000000.0,(double)t.Peers[1].SmallDoneAt/1000000.0,(double)t.Now/1000000.0);
}
