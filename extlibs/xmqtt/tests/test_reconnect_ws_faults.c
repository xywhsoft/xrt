#define main wrPositiveMain
#include "test_reconnect_ws.c"
#undef main
static void wrSetupStop(bool TLS,unsigned Mode,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,Mode==3?4:3,B);xrtAtomic32Store(&S.HoldHttp,Mode!=3,XMEMORY_RELEASE);xrtAtomic32Store(&S.BlockDns,Mode==3,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,Mode==0?80:Mode==3?200:10000,false);
    if(Mode==1||Mode==2){wrWait(&S.Connections[0].Peer.Header,1,"real HTTP held before stop");}if(Mode==3){wrWait(&S.DnsEntered,1,"real detached WS resolver job");}
    if(Mode==1)testRequire(xmqttReconnectClose(R),"stop controller inside HTTP Upgrade");
    if(Mode==2)testRequire(xrtTaskPoolClose(S.Pool)&&xmqttReconnectClose(R),"closed cleanup Pool retains fallback owner");
    testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"original outage budget/stop includes HTTP and DNS");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I)&&!I.Connections&&I.Attempts==1&&I.LastStatus==(Mode==0||Mode==3?XMQTT_STATUS_TIMEOUT:Mode==2?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_CLOSED),"no false ONLINE or fresh outage clock");wrStop(&S,R);
}
static void wrRejected(bool TLS,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,4,B);xrtAtomic32Store(&S.RejectHttp,8,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,10000,false);testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"rejected Upgrade terminates default retry policy");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I)&&I.LastStatus==XMQTT_STATUS_REJECTED&&I.Attempts==1&&!I.Connections,"HTTP rejection does not select stale ambient retry policy");wrStop(&S,R);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(TLS){wrStart(&S,true,XMQTT_VERSION_5,4,B);xrtAtomic32Store(&S.RejectTLS,1,XMEMORY_RELEASE);R=wrCreate(&S,10000,false);testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"bounded actual TLS rejection");testRequire(xmqttReconnectInfo(R,&I)&&I.Attempts==1&&!I.Connections&&I.LastStatus==XMQTT_STATUS_TRANSPORT,"untrusted TLS never enters HTTP/MQTT online");wrStop(&S,R);}
#endif
}
static void wrBudget(bool TLS,xnetportbackend B)
{
    size_t Limits[]={1,2048,8192,16384,65536,262144};bool Failed=false,Passed=false;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,4,B);testRequire(xrtAllocationBudgetCreate(Limits[N],&S.Budget),"actual factory allocation quota");xmqttreconnect* R=wrCreate(&S,10000,false);double End=xrtTimer()+15;xmqttreconnectinfo I;for(;;){testRequire(xmqttReconnectInfo(R,&I)&&xrtTimer()<End,"bounded actual OOM outcome");if(I.State==XMQTT_RECONNECT_ONLINE){Passed=true;break;}if(I.State>=XMQTT_RECONNECT_STOPPED){Failed=true;testRequire(!I.Connections&&(I.LastStatus==XMQTT_STATUS_MEMORY||I.LastStatus==XMQTT_STATUS_INITIALIZATION),"local construction refusal is terminal and never online");break;}xrtSleep(1);}wrStop(&S,R);}testRequire(Failed&&Passed,"real constructor failures and successful retry-free controls");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        2
#else
        1
#endif
        ;++TLS){for(unsigned Mode=0;Mode<4;++Mode)wrSetupStop(TLS!=0,Mode,B);wrRejected(TLS!=0,B);wrBudget(TLS!=0,B);}puts("[PASS] automatic WS HTTP/DNS clocks, stop, cleanup fallback, rejection and OOM");return 0;
}
