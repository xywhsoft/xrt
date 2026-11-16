/* Real UDP source-port rebinding while XTP RPC/PUSH are in flight.
 * This is a lab NAT/path test, not physical Wi-Fi/carrier qualification. */
#include "../examples/support/game_loopback.h"
#include <stdlib.h>
typedef struct migration {game_test Game;uint64_t DropUntil;size_t Blackholed,Rejected;} migration;
static xrudpaddress migrate_address(const xnetaddr* a)
{xrudpaddress r={0};r.Family=4;r.Port=a->Port;memcpy(r.Address,a->Address,16);return r;}
static xrudpresult migrate_send(const void* bytes,size_t n,const xrudpaddress* destination,void* user)
{
    game_peer* p=user;migration* m=(migration*)p->Test;
    if(m->Game.Now<m->DropUntil){++m->Blackholed;return XRUDP_OK;}
    xnetaddr to;assert(destination->Family==4&&xrtNetAddrParse(&to,"127.0.0.1",destination->Port));
    size_t sent;xnetresult r=xrtNetSocketSendTo(m->Game.Sockets[p->Index],bytes,n,&sent,&to);
    if(r==XNET_RESULT_AGAIN)return XRUDP_BACKPRESSURE;
    assert(r==XNET_RESULT_OK&&sent==n);return XRUDP_OK;
}
static xrudpresult migrate_original_send(const void* bytes,size_t n,void* user)
{game_peer* p=user;xrudpaddress to=migrate_address(&p->Test->Addresses[1-p->Index]);return migrate_send(bytes,n,&to,user);}
static void migrate_receive(migration* m)
{
    for(size_t i=0;i<2;++i)for(size_t k=0;k<256;++k){unsigned char bytes[1200];size_t n;xnetaddr from;
        xnetresult r=xrtNetSocketRecvFrom(m->Game.Sockets[i],bytes,sizeof(bytes),&n,&from);if(r==XNET_RESULT_AGAIN)break;
        assert(r==XNET_RESULT_OK);xrudpaddress source=migrate_address(&from);
        xrudpresult input=xrtRudpSessionPathInput(m->Game.Peers[i].Rudp,&source,bytes,n,m->Game.Now);
        if(input==XRUDP_AUTH_FAILED||input==XRUDP_MALFORMED||input==XRUDP_TRUNCATED||input==XRUDP_PROTOCOL_MISMATCH)++m->Rejected;
        else assert(input==XRUDP_OK||input==XRUDP_DUPLICATE||input==XRUDP_TOO_OLD||input==XRUDP_BACKPRESSURE);}
}
static xnetsocket migrate_socket(xnetaddr* address)
{xnetsocket s=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);assert(s);
    xnetaddr bind;assert(xrtNetAddrParse(&bind,"127.0.0.1",0)&&xrtNetSocketBind(s,&bind)&&xrtNetSocketLocal(s,address));return s;}
int main(int argc,char** argv)
{
    unsigned cycles=argc>1?(unsigned)strtoul(argv[1],NULL,10):1;assert(cycles&&cycles<=1000);
    (void)game_run;migration m;memset(&m,0,sizeof(m));game_test* t=&m.Game;
    for(unsigned i=0;i<2;++i)t->Sockets[i]=migrate_socket(&t->Addresses[i]);
    xnetaddr old_address=t->Addresses[0],new_address,rogue_address;
    xnetsocket old_socket=t->Sockets[0],new_socket=migrate_socket(&new_address),rogue=migrate_socket(&rogue_address);
    xrudpsessionconfig c;xrtRudpSessionConfigInit(&c);c.StreamCapacity=c.Local.MaxStreams=c.Peer.MaxStreams=1;
    c.ReceiveCapacity=c.TransmitCapacity=65536;c.Local.StreamCredit=c.Peer.StreamCredit=65536;
    c.Local.ConnectionCredit=c.Peer.ConnectionCredit=65536;c.Local.DatagramSize=c.Peer.DatagramSize=1200;
    c.InitialRttUs=10000;c.PacketCapacity=64;c.KeyUpdatePackets=16;xrtRudpDatagramConfigInit(&c.Datagrams);c.Datagrams.Channels=1;c.Datagrams.PayloadCapacity=256;
    xrudpcipher* cipher[2];game_handshake(t,&c,cipher);
    for(unsigned i=0;i<2;++i){game_peer* p=&t->Peers[i];p->Test=t;p->Index=i;c.Role=i?XRUDP_RESPONDER:XRUDP_INITIATOR;
        assert(xrtRudpSessionCreate(&c,cipher[i],0,migrate_original_send,p,&p->Rudp)==XRUDP_OK);
        xrudpaddress verified=migrate_address(&t->Addresses[1-i]);assert(xrtRudpSessionEnablePaths(p->Rudp,&verified,migrate_send)==XRUDP_OK);
        xrudpdatagramchannel channel;xrtRudpDatagramChannelInit(&channel);channel.Id=1;channel.Mode=XRUDP_DATAGRAM_LATEST;channel.ReplacePending=true;channel.ReceiveLifetimeUs=50000;
        assert(xrtRudpSessionDatagramConfigure(p->Rudp,&channel)==XRUDP_OK);
        xtpevents events={game_business,game_complete,game_fault};xtpconfig config;xtpConfigInit(&config);config.DefaultOperationTimeoutUs=30000000;
        assert(xtpCreate(&config,&events,p,&p->Xtp)==XTP_OK);xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200}};
        xtpbinding binding={lanes,2,true,false,NULL};assert(xtpBind(p->Xtp,&binding,0,&p->Binding)==XTP_OK);}
    uint64_t begin=game_clock();uint32_t tick=0;
    for(unsigned step=0;step<5*cycles;++step){unsigned phase=step%5;t->Now=game_clock()-begin;
        if(phase==3)m.DropUntil=t->Now+500000;
        for(size_t i=0;i<2;++i){xtpmessage message={0};message.Type=XTP_REQUEST;message.Command=xtpCmdID(1);message.Body=XRT_BYTES_LITERAL("rpc");
            assert(xtpSubmit(t->Peers[i].Xtp,&message,NULL,t->Now,NULL)==XTP_OK);message.Type=XTP_PUSH;message.Command=xtpCmdID(3);message.Body=XRT_BYTES_LITERAL("cmd");
            assert(xtpSubmit(t->Peers[i].Xtp,&message,NULL,t->Now,NULL)==XTP_OK);}
        game_io(&t->Peers[0]);migrate_receive(&m);game_io(&t->Peers[1]);
        if(phase==1){t->Sockets[0]=new_socket;t->Addresses[0]=new_address;}
        if(phase==2){t->Sockets[0]=old_socket;t->Addresses[0]=old_address;}
        if(phase==4){unsigned char invalid[64]={0};size_t sent;assert(xrtNetSocketSendTo(rogue,invalid,sizeof(invalid),&sent,&t->Addresses[1])==XNET_RESULT_OK&&sent==sizeof(invalid));}
        uint64_t deadline=t->Now+10000000;
        for(;;){t->Now=game_clock()-begin;assert(t->Now<deadline);migrate_receive(&m);
            if(t->Now>=t->NextTick){++tick;for(size_t i=0;i<2;++i)game_snapshot(&t->Peers[i],tick);t->NextTick+=16667;}
            for(size_t i=0;i<2;++i)game_io(&t->Peers[i]);
            if(t->Peers[0].Completed==2*(step+1)&&t->Peers[1].Completed==2*(step+1))break;
            xrtSleep(1);}
        for(size_t i=0;i<2;++i)assert(t->Peers[i].Commands==2*(step+1));
        xrudpsessionstate state;assert(xrtRudpSessionState(t->Peers[1].Rudp,&state)==XRUDP_OK&&state.Reason==XRUDP_OK);
        if(phase==1)assert(state.Path.Validations>=1&&state.Path.Active.Port==new_address.Port);
        if(phase>=2)assert(state.Path.Validations>=2&&state.Path.Active.Port==old_address.Port);
        if(phase>=3)assert(state.KeyUpdates);
        printf("XTP UDP migration phase=%u reliable_completed=%zu/%zu validations=%llu key_updates=%llu PASS\n",phase,t->Peers[0].Completed,t->Peers[1].Completed,(unsigned long long)state.Path.Validations,(unsigned long long)state.KeyUpdates);
    }
    assert(m.Blackholed&&m.Rejected);
    for(size_t i=0;i<2;++i){xtpresult r=xtpClose(t->Peers[i].Xtp,t->Now);while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(t->Peers[i].Xtp,t->Now,&due);}
        assert(r==XTP_CLOSED&&xtpDestroy(t->Peers[i].Xtp)==XTP_OK);xrtRudpSessionDestroy(t->Peers[i].Rudp);}
    assert(xrtNetSocketClose(old_socket)&&xrtNetSocketClose(new_socket)&&xrtNetSocketClose(rogue)&&xrtNetSocketClose(t->Sockets[1]));
    printf("XTP UDP NAT/return/500ms blackout/unauthenticated path: rejected=%zu dropped=%zu PASS\n",m.Rejected,m.Blackholed);return 0;
}
