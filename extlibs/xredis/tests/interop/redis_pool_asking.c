#include <xredis/pool_asking.h>
#include <xredis/dial.h>
#include <xredis/cluster_topology.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void arRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real ASKING: %s\n",T);exit(1);}}
static const void* arWait(xfuture* F){arRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureValue(F),"typed native Future");return xrtFutureValue(F);}
static xfuture* arCall(xredislease* L,const xbytesview* A,size_t N)
{xfuture* F=NULL;arRequire(xredisLeaseCallAsync(L,A,N,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"ordinary lease call");return F;}
static xredisreplyvalue arValue(xredisreply* R)
{xredisreplyinfo I={0};I.Size=sizeof(I);xredisreplyvalue V;arRequire(xredisReplyInfo(R,&I) && xredisReplyGet(R,I.Root,&V),"immutable reply root");return V;}
static void arMoved(xfuture* F,uint16 Source)
{const xredisrequestresult* R=arWait(F);xredisclusterredirect D;uint16 Slot;arRequire(xredisClusterKeySlot(XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key"),&Slot),"real key slot");arRequire(R->Status==XREDIS_REQUEST_SERVER_ERROR && R->Delivery==XREDIS_REPLIED && xredisClusterRedirectRead(R->Reply,1024,&D)==XREDIS_CLUSTER_VALUE && D.Kind==XREDIS_CLUSTER_MOVED && D.Port==Source && D.Slot==Slot,"ordinary request cannot inherit ASKING flag");}
static bool arContains(xbytesview V,cstr Text)
{size_t N=strlen(Text);for(size_t I=0;I+N<=V.Size;++I)if(!memcmp(V.Data+I,Text,N))return true;return false;}
static void arPair(xfuture* F,bool Denied)
{const xredisaskingresult* R=arWait(F);arRequire(R->ControlReceived && R->Control.Status==(Denied?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK) && R->Control.Delivery==XREDIS_REPLIED && R->Query.Status==XREDIS_REQUEST_OK && R->Query.Delivery==XREDIS_REPLIED,"independent real control/query outcomes");xredisreplyvalue V=arValue(R->Query.Reply);arRequire(V.Data.Size==7 && !memcmp(V.Data.Data,"value\0x",7),"owned binary business reply");}
int main(int Argc,char** Argv)
{
    if(Argc!=4 || strlen(Argv[3])!=40)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10),Source=strtoul(Argv[2],NULL,10);if(!Port || Port>65535 || !Source || Source>65535)return 2;
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* E=xrtNetEngineCreate(&EC);arRequire(E && xrtNetEngineStart(E),"engine start");xnetresolver* Resolver=xrtNetResolverCreate(NULL);arRequire(Resolver!=NULL,"resolver");
    const xredisinitmode Modes[]={XREDIS_INIT_RESP2,XREDIS_INIT_RESP3,XREDIS_INIT_AUTO};xfuture* Retained[18]={0};bool Denials[18]={0};size_t Saved=0;
    for(unsigned Factory=0;Factory<3;++Factory)for(unsigned M=0;M<3;++M)for(unsigned Denied=0;Denied<2;++Denied){
        xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Mode=Modes[M];I.Timeout=10000;if(Denied){I.Flags=XREDIS_INIT_AUTH|XREDIS_INIT_USERNAME;I.Username=(xbytesview)XRT_BYTES_LITERAL("asking-denied");}
        xredisclientdialconfig D;xredisClientDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;xnetaddr Address;arRequire(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)Port),"owned numeric target");xredisclient* Client=NULL;bool OK;
        if(Factory==1)OK=xredisClientDial(E,Resolver,XRT_STR_LITERAL("localhost"),(uint16)Port,0,NULL,&I,&D,&Client);
        else if(Factory==2){xnetaddrlist* L=xrtNetAddrListCreate(&Address,1);arRequire(L!=NULL,"prepared addresses");OK=xredisClientDialAddresses(E,L,0,NULL,&I,&D,&Client);xrtNetAddrListDestroy(L);}
        else OK=xredisClientConnectInit(E,&Address,0,NULL,&I,&Client);
        arRequire(OK,"native numeric/DNS/prepared factory");xfuture* Ready=xredisClientReady(Client);arRequire(((const xredisreadyresult*)arWait(Ready))->Status==XREDIS_READY_OK,"real negotiated/authenticated readiness");xrtFutureDestroy(Ready);
        xredispool* Pool=NULL;arRequire(xredisPoolCreate(E,1,&Client,1,NULL,&Pool)==XREDIS_ADMISSION_ACCEPTED,"fixed native pool");xfuture* A=NULL;arRequire(xredisPoolAcquire(Pool,10000,&A)==XREDIS_ADMISSION_ACCEPTED,"lease acquisition");xredislease* Lease=((const xredisleaseresult*)arWait(A))->Lease;arRequire(Lease!=NULL,"lease grant");
        xbytesview Info[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* ID=arCall(Lease,Info,2);const xredisrequestresult* IR=arWait(ID);arRequire(IR->Status==XREDIS_REQUEST_OK && arContains(arValue(IR->Reply).Data,Argv[3]),"actual owned target run ID");xrtFutureDestroy(ID);
        xbytesview Get[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key")};xfuture* Before=arCall(Lease,Get,2);arMoved(Before,(uint16)Source);xrtFutureDestroy(Before);
        xbytesview ClientID[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("ID")};ID=arCall(Lease,ClientID,2);int64 Original=arValue(((const xredisrequestresult*)arWait(ID))->Reply).Integer;xrtFutureDestroy(ID);
        xbytesview Echo[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("value\0x")};xfuture* Pair=NULL;arRequire(xredisLeaseAskingCallAsync(Lease,Denied?Echo:Get,2,10000,&Pair)==XREDIS_ADMISSION_ACCEPTED,"real native ASKING pair");arPair(Pair,Denied!=0);
        xfuture* After=arCall(Lease,Get,2);arMoved(After,(uint16)Source);xrtFutureDestroy(After);
        xfuture* Closed=xredisLeaseClosed(Lease);arRequire(xredisLeaseReturn(Lease) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredisleaseclosedresult*)arWait(Closed))->Reusable,"real physical return");xrtFutureDestroy(A);xrtFutureDestroy(Closed);
        A=NULL;arRequire(xredisPoolAcquire(Pool,10000,&A)==XREDIS_ADMISSION_ACCEPTED,"second native generation");Lease=((const xredisleaseresult*)arWait(A))->Lease;arRequire(Lease!=NULL,"second grant");ID=arCall(Lease,ClientID,2);arRequire(arValue(((const xredisrequestresult*)arWait(ID))->Reply).Integer==Original,"same physical connection after return");xrtFutureDestroy(ID);After=arCall(Lease,Get,2);arMoved(After,(uint16)Source);xrtFutureDestroy(After);
        Closed=xredisLeaseClosed(Lease);arRequire(xredisLeaseReturn(Lease) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"second physical return");xrtFutureDestroy(A);xrtFutureDestroy(Closed);Closed=xredisPoolClosed(Pool);arRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"native pool physical close");
        xredisclientinfo CI={0};CI.Size=sizeof(CI);arRequire(xredisClientInfo(Client,&CI) && !CI.Requests && !CI.CommandBytes && CI.Accepted==CI.Completed,"all request/wire/cancel/timer owners retire");xredisPoolRelease(Pool);xredisClientRelease(Client);xrtFutureDestroy(Closed);Retained[Saved]=Pair;Denials[Saved++]=Denied!=0;
    }
    arRequire(xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"all native owners joined");for(size_t N=0;N<Saved;++N){arPair(Retained[N],Denials[N]);xrtFutureDestroy(Retained[N]);}
    puts("[PASS] real ASKING: 18 numeric/DNS/prepared RESP2/3/AUTO ACL cases, importing-slot GET, explicit denied control/business execution, no flag across same-connection lease reuse and retained outcomes");return 0;
}
