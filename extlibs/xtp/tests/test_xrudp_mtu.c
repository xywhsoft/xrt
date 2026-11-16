#include "../examples/support/game_loopback.h"
static xtpdecision mtu_business(xtpsession* s,const xtpmessageview* m,void* user)
{
    game_peer* p=user;(void)s;assert(m->Type==XTP_NOTIFY);
    if(m->Command.As.ID==4){assert(m->Body.Size==32768&&!memcmp(m->Body.Data,p->Test->Body,32768));++p->Commands;}
    else {assert(m->Command.As.ID==2&&m->Body.Size==5&&!memcmp(m->Body.Data,"small",5));++p->Snapshots;}
    return xtpAccept();
}
static void mtu_submit(game_peer* p,uint32_t command,const void* bytes,size_t size,uint32_t lane)
{
    xtpmessage m={0};m.Type=XTP_NOTIFY;m.Command=xtpCmdID(command);m.Body=(xbytesview){bytes,size};
    xtpsubmitoptions o={0};o.LaneID=lane;assert(xtpSubmit(p->Xtp,&m,&o,p->Test->Now,NULL)==XTP_OK);
}
int main(void)
{
    /* Reuse the real-time game baseline, then run PMTU's deterministic injected
     * clock over the same real UDP sockets. Do not interpret this as latency. */
    game_run(0,20);game_test t;memset(&t,0,sizeof(t));t.Random=0x4ac701; t.Rtt=20;t.PathLimit=1200;
    for(size_t i=0;i<sizeof(t.Body);++i)t.Body[i]=(unsigned char)(i*17u);
    for(size_t i=0;i<2;++i){t.Sockets[i]=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);assert(t.Sockets[i]);
        assert(xrtNetSocketSet(t.Sockets[i],XNET_OPTION_PATH_MTU_MODE,XNET_PMTU_PROBE));
        xnetaddr a;assert(xrtNetAddrParse(&a,"127.0.0.1",0)&&xrtNetSocketBind(t.Sockets[i],&a)&&xrtNetSocketLocal(t.Sockets[i],&t.Addresses[i]));}
    xrudpsessionconfig c;xrtRudpSessionConfigInit(&c);c.DiscoverMtu=true;c.ReceiveCapacity=c.TransmitCapacity=65536;
    c.Local.StreamCredit=c.Peer.StreamCredit=c.Local.ConnectionCredit=c.Peer.ConnectionCredit=65536;c.Local.MaxStreams=c.Peer.MaxStreams=c.StreamCapacity=1;
    c.Local.DatagramSize=c.Peer.DatagramSize=1200;xrtRudpDatagramConfigInit(&c.Datagrams);c.Datagrams.Channels=1;
    xrudpcipher* cipher[2];game_handshake(&t,&c,cipher);
    for(unsigned i=0;i<2;++i){game_peer* p=&t.Peers[i];p->Test=&t;p->Index=i;c.Role=i?XRUDP_RESPONDER:XRUDP_INITIATOR;
        assert(xrtRudpSessionCreate(&c,cipher[i],0,game_send,p,&p->Rudp)==XRUDP_OK);
        xrudpdatagramchannel channel;xrtRudpDatagramChannelInit(&channel);channel.Id=1;assert(xrtRudpSessionDatagramConfigure(p->Rudp,&channel)==XRUDP_OK);
        xtpconfig config;xtpConfigInit(&config);config.Reader.ReceiveTimeoutUs=15000000;
        xtpevents events={mtu_business,game_complete,game_fault};assert(xtpCreate(&config,&events,p,&p->Xtp)==XTP_OK);
        xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200}};xtpbinding b={lanes,2,true,false};assert(xtpBind(p->Xtp,&b,0,&p->Binding)==XTP_OK);}
    bool warm=false;for(t.Now=1000;t.Now<8000000;t.Now+=1000){game_udp(&t);for(size_t i=0;i<2;++i)game_io(&t.Peers[i]);
        xrudpsessionstate a,b;assert(xrtRudpSessionState(t.Peers[0].Rudp,&a)==XRUDP_OK&&xrtRudpSessionState(t.Peers[1].Rudp,&b)==XRUDP_OK);
        if(a.Mtu.Effective==1200&&b.Mtu.Effective==1200){warm=true;break;}}
    assert(warm);unsigned char big[800]={0};mtu_submit(&t.Peers[0],9,big,sizeof(big),2);mtu_submit(&t.Peers[0],4,t.Body,sizeof(t.Body),1);
    t.PathLimit=512;uint64_t started=t.Now;bool fell=false,done=false;
    for(;t.Now<started+16000000;t.Now+=1000){game_udp(&t);for(size_t i=0;i<2;++i)game_io(&t.Peers[i]);
        xrudpsessionstate state;assert(xrtRudpSessionState(t.Peers[0].Rudp,&state)==XRUDP_OK);
        if(!fell&&state.Mtu.Fallbacks){assert(state.Mtu.Effective==512&&state.DatagramSendLimit==424);fell=true;
            mtu_submit(&t.Peers[0],9,big,sizeof(big),2);mtu_submit(&t.Peers[0],2,"small",5,2);}
        if(fell&&t.Peers[0].LocalDrops==1&&t.Peers[1].Snapshots==1&&t.Peers[1].Commands==1){done=true;break;}}
    assert(done&&t.Lost>0);for(size_t i=0;i<2;++i){assert(xtpClose(t.Peers[i].Xtp,t.Now)==XTP_CLOSED&&xtpDestroy(t.Peers[i].Xtp)==XTP_OK);
        xrtRudpSessionDestroy(t.Peers[i].Rudp);assert(xrtNetSocketClose(t.Sockets[i]));}
    puts("XTP xrudp PMTU:1200->512, oversized NOTIFY locally rejected, reliable resource and small snapshot continue PASS");return 0;
}
