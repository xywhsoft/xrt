/* Independent deterministic wire fixture: deliberately does not use Core WS
 * encoders/readers. HTTP negotiation is outside this attach-only fixture. */
typedef struct wsRaw {
    xnetengine* Engine;xnetlistener* Listener;xnetstream* Client;xnetstream* Alias;
    xrpcws* Peer;xrpcwsconfig Config;xatomic32 Ready,Open;unsigned Mode;
    bool Replied;size_t Used;unsigned char Request[512];xallocationbudget* Budget;
} wsRaw;
static void wsRawFrame(xnetstream* S,unsigned First,const void* Data,size_t Size,bool Mask,bool Split)
{
    unsigned char Wire[1024];testRequire(Size<=512,"raw WS fixture capacity");size_t N=0;
    Wire[N++]=(unsigned char)First;Wire[N++]=(unsigned char)((Mask?128:0)|(Size<126?Size:126));
    if(Size>=126){Wire[N++]=(unsigned char)(Size>>8);Wire[N++]=(unsigned char)Size;}
    static const unsigned char Key[]={17,39,83,151};if(Mask){memcpy(Wire+N,Key,4);N+=4;}
    for(size_t I=0;I<Size;++I)Wire[N++]=((const unsigned char*)Data)[I]^(Mask?Key[I%4]:0);
    /* A complete malformed frame is one offer: its early header can close
     * the peer before later bytewise offers. Valid replies exercise splits. */
    if(!Split){testRequire(xrtNetStreamSend(S,Wire,N)==XNET_RESULT_OK,"raw WS fault frame send");return;}
    for(size_t I=0;I<N;++I)testRequire(xrtNetStreamSend(S,Wire+I,1)==XNET_RESULT_OK,"raw WS wire send");
}
static void wsRawOpen(xnetstream* S,ptr Data)
{
    wsRaw* R=Data;static const unsigned char BadUTF8[]={192,175};unsigned char Large[300];memset(Large,'x',sizeof(Large));
    switch(R->Mode){
    case 0:wsRawFrame(S,129,"{}",2,false,false);break;
    case 1:wsRawFrame(S,130,"{}",2,true,false);break;
    case 2:wsRawFrame(S,129,BadUTF8,sizeof(BadUTF8),true,false);break;
    case 3:wsRawFrame(S,129,Large,129,true,false);break;
    case 4:wsRawFrame(S,1,"{",1,true,false);break;
    case 5:wsRawFrame(S,131,"{}",2,true,false);break;
    case 7:wsRawFrame(S,1,Large,200,true,false);wsRawFrame(S,128,Large,100,true,false);break;
    default:break;
    }
    xrtAtomic32Store(&R->Open,1,XMEMORY_RELEASE);
}
static void wsRawRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;wsRaw* R=Data;unsigned char Bytes[512];size_t N;
    while((N=xrtNetStreamRead(S,Bytes,sizeof(Bytes)))!=0){
        if((R->Mode!=6&&R->Mode!=8)||R->Replied)continue;
        testRequire(N<=sizeof(R->Request)-R->Used,"raw outgoing WS bound");memcpy(R->Request+R->Used,Bytes,N);R->Used+=N;
        if(R->Used<2)continue;
        testRequire(R->Request[0]==129 && !(R->Request[1]&128) && R->Request[1]<126,"unmasked native server Text output");
        if(R->Used<(size_t)R->Request[1]+2)continue;
        static const char Reply[]="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":[9]}";
        if(R->Mode==6){wsRawFrame(S,1,Reply,11,true,true);wsRawFrame(S,137,"p",1,true,true);wsRawFrame(S,128,Reply+11,sizeof(Reply)-1-11,true,true);}
        else wsRawFrame(S,130,Reply,sizeof(Reply)-1,true,true);
        R->Replied=true;testRequire(xrtNetStreamShutdownWrite(S),"WS final message followed by EOF");
    }
}
static bool wsRawAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;wsRaw* R=Data;R->Config.Stream.Role=XWS_ROLE_SERVER;
    char Protocol[]="jsonrpc";R->Config.Stream.Protocol=(xstrview){Protocol,7};
    if(R->Mode==9){
        R->Alias=xrtNetStreamRef(S);testRequire(R->Alias!=NULL,"native TCP alias");
        xnetbuf Initial;testRequire(xrtNetBufInit(&Initial,NULL),"owned initial buffer");
        static const unsigned char Ping[]={137,128,1,2,3,4};testRequire(xrtNetBufAppend(&Initial,Ping,sizeof(Ping)),"initial owned read-ahead");
        xrpcwsconfig Invalid=R->Config;Invalid.ReadBytes=0;xrpcws* Out=(xrpcws*)(uintptr_t)1;
        testRequire(!xrpcWsAttachTcp(S,&Initial,&Invalid,&Out)&&Out==(xrpcws*)(uintptr_t)1&&xrtNetBufSize(&Initial)==sizeof(Ping),"invalid constructor transaction");xrtClearError();
        static const size_t Limits[]={1,64,512,8192,65536,131072,262144,1048576};unsigned Failed=0,Passed=0;
        for(size_t I=0;I<sizeof(Limits)/sizeof(Limits[0]);++I){
            xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B)&&xrtAllocationBudgetEnter(B,&Scope),"WS constructor budget");
            bool OK=xrpcWsAttachTcp(S,&Initial,&R->Config,&Out);testRequire(xrtAllocationBudgetLeave(&Scope),"WS budget leave");
            if(OK){++Passed;R->Peer=Out;R->Budget=B;testRequire(!xrtNetBufSize(&Initial),"successful attach consumes initial blocks");break;}
            ++Failed;testRequire(Out==(xrpcws*)(uintptr_t)1&&xrtNetBufSize(&Initial)==sizeof(Ping)&&xrtNetStreamState(S)==XNET_STREAM_OPEN,"failed attach preserves native/output/initial");xrtClearError();
            xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info)&&!Info.LiveBytes&&!Info.LiveCount,"failed WS constructor allocation/ref/pin leak");xrtAllocationBudgetFree(B);
        }
        testRequire(Failed&&Passed,"WS actual allocation failure and success boundary");xrtNetBufClear(&Initial);
    }else testRequire(xrpcWsAttachTcp(S,NULL,&R->Config,&R->Peer),"raw WS product attach");
    memset(Protocol,'x',7);testRequire(xrpcWsWaitFor(R->Peer,0)==XWAIT_ERROR&&!xrpcWsDestroy(R->Peer),"same-Engine wait/live destruction refused");xrtClearError();
    xrtAtomic32Store(&R->Ready,1,XMEMORY_RELEASE);return true;
}
static void wsRawStop(wsRaw* R)
{
    testRequire(xrpcWsDestroy(R->Peer),"raw peer physical destroy");
    if(xrtNetStreamState(R->Client)!=XNET_STREAM_CLOSED)testRequire(xrtNetStreamAbort(R->Client),"raw client abort");
    testRequire(xrtNetStreamWaitRetired(R->Client,5000)==XWAIT_OK,"raw client physical join");xrtNetStreamDestroy(R->Client);
    testRequire(xrtNetListenerClose(R->Listener),"raw listener close");xrtNetListenerDestroy(R->Listener);
    double End=xrtTimer()+5;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(R->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"raw Engine tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"raw Engine join");
    if(R->Budget){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(R->Budget,&I)&&!I.LiveBytes&&!I.LiveCount,"successful budget physical release");xrtAllocationBudgetFree(R->Budget);}
}
static void wsFaults(void)
{
    for(unsigned Mode=0;Mode<10;++Mode){
        printf("[case] WS raw %u\n",Mode);wsRaw R={0};R.Mode=Mode;xrtAtomic32Init(&R.Ready,0);xrtAtomic32Init(&R.Open,0);
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;R.Engine=xrtNetEngineCreate(&E);testRequire(R.Engine&&xrtNetEngineStart(R.Engine),"raw Engine");
        xrpcWsConfigInit(&R.Config);R.Config.MaxMessageBytes=128;R.Config.MaxReceiveBytes=384;R.Config.KeepReceiveBytes=0;R.Config.ReadBytes=3;R.Config.MessageTimeout=Mode==4?40:5000;R.Config.AllowBinary=Mode==8;
        if(Mode==7){R.Config.MaxMessageBytes=512;R.Config.MaxReceiveBytes=512;}if(Mode==9)xrpcWsConfigInit(&R.Config);
        xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={wsRawAccept,NULL,NULL};R.Listener=xrtNetListen(R.Engine,&L,&A,NULL,&R);xnetaddr Address;testRequire(R.Listener&&xrtNetListenerLocal(R.Listener,&Address),"raw listener");
        xnetstreamevents Events={.Open=wsRawOpen,.Read=wsRawRead};R.Client=xrtNetStreamConnect(R.Engine,&Address,1,NULL,&Events,&R);testRequire(R.Client!=NULL,"raw client");rpcWsServiceAwait(&R.Ready);rpcWsServiceAwait(&R.Open);
        xfuture* F=NULL;if(Mode==6||Mode==8){testRequire(xrpcWsCall(R.Peer,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"final WS call");if(rpcWsServiceResult(F)->Status!=XRPC_CALL_OK){xrpcwsinfo Q={0};Q.Size=sizeof(Q);xrpcWsInfo(R.Peer,&Q);fprintf(stderr,"final mode=%u state=%d reason=%d input=%llu messages=%llu native=%d/%d/%s close=%u/%u/%u\n",Mode,Q.State,Q.Reason,(unsigned long long)Q.ReadBytes,(unsigned long long)Q.ReadMessages,Q.NativeErrorKind,Q.NativeErrorCode,Q.NativeErrorDomain,Q.NativeCloseFlags,Q.LocalCode,Q.RemoteCode);}wsResult(F,XRPC_CALL_OK,XRPC_REPLIED);const xrpccallresult* Result=rpcWsServiceResult(F);xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(Result->Responses,&I)&&I.Messages[Result->Index].Result.Raw.Size==3&&!memcmp(I.Messages[Result->Index].Result.Raw.Data,"[9]",3),"final response exact content before EOF");}
        if(Mode==9){testRequire(xrpcWsClose(R.Peer)&&xrpcWsWaitFor(R.Peer,20)==XWAIT_TIMEOUT,"native alias delays product physical retirement");xrtNetStreamDestroy(R.Alias);R.Alias=NULL;}
        testRequire(xrpcWsWaitFor(R.Peer,5000)==XWAIT_OK,"raw WS fault physical join");xrpcwsinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcWsInfo(R.Peer,&Info),"raw WS info");
        xrpcwsreason Expected=Mode==3||Mode==7?XRPC_WS_LIMIT:Mode==4?XRPC_WS_TIMEOUT:Mode==6||Mode==8?XRPC_WS_TRANSPORT:Mode==9?XRPC_WS_LOCAL:XRPC_WS_PROTOCOL;
        if(Info.Reason!=Expected)fprintf(stderr,"WS raw mode=%u reason=%d expected=%d native=%d/%d/%s\n",Mode,Info.Reason,Expected,Info.NativeErrorKind,Info.NativeErrorCode,Info.NativeErrorDomain);
        testRequire(Info.Reason==Expected&&Info.State==XRPC_WS_CLOSED&&Info.Duplex.Drained&&!Info.ReceiveBytes&&!Info.ReceiveCapacity&&Info.PeakReceiveBytes<=R.Config.MaxReceiveBytes&&Info.ProtocolBytes==7&&!memcmp(Info.Protocol,"jsonrpc",7),"raw typed bounds/protocol snapshot/physical ledger");
        wsRawStop(&R);if(F){wsResult(F,XRPC_CALL_OK,XRPC_REPLIED);xrtFutureDestroy(F);}
    }
}
