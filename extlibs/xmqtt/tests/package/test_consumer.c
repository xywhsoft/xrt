#if !defined(XMQTT_FEATURE_XMQTT_WIRE)
#define XMQTT_MODULE_XMQTT
#endif
#include <xmqtt.h>
#include <string.h>

int main(void)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER)
    xmqttserverconfig ServerConfig;
    xmqttserver* Server = (xmqttserver*)(uintptr_t)1;
    if (!xmqttServerConfigInit(&ServerConfig) || ServerConfig.Allow311 ||
        ServerConfig.MaxConnections != 1024 || ServerConfig.Codec.MaxPacketBytes != 16384 ||
        xmqttServerCreate(NULL,&ServerConfig,&Server) || Server != (xmqttserver*)(uintptr_t)1 ||
        !xmqttServerDestroy(NULL)) return 71;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
    xmqttserverauthconfig Auth;
    if (!xmqttServerAuthConfigInit(&Auth) || Auth.Size != sizeof(Auth) ||
        Auth.MaxRounds != 32 || Auth.MaxExchangeBytes != 1048576 || Auth.ReauthTimeout != 5000 ||
        xmqttServerCreateAuth(NULL, &ServerConfig, &Auth, &Server) ||
        Server != (xmqttserver*)(uintptr_t)1) return 72;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
    if (xmqttServerVisitRetained(NULL, NULL, NULL) ||
        xmqttServerRestoreRetained(NULL, NULL, 0, 0) ||
        xmqttServerCloseForCheckpoint(NULL) || xmqttServerVisitSessions(NULL, NULL, NULL) ||
        xmqttServerRestoreSessions(NULL, NULL, 0, 0)) return 73;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
    xmqttjournalworkerconfig WC;xmqttjournalworker* Worker=(xmqttjournalworker*)(uintptr_t)1;xfuture* WF=(xfuture*)(uintptr_t)1;
    if(!xmqttJournalWorkerConfigInit(&WC)||WC.Size!=sizeof(WC)||WC.MaxRequests!=128||WC.MaxTasks!=2||WC.WorkTransactions!=32)return 56;
    if(xmqttJournalWorkerCreate(NULL,NULL,&WC,&Worker)||Worker!=(xmqttjournalworker*)(uintptr_t)1||xmqttJournalWorkerAppendAsync(NULL,(xbytesview){0},1000,&WF)!=XMQTT_JWORK_ERROR||WF!=(xfuture*)(uintptr_t)1||!xmqttJournalWorkerDestroy(NULL))return 57;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
    xmqttstoresessionconfig Import;xmqttsessionconfig ImportSession;xmqttsession* Imported=(xmqttsession*)(uintptr_t)1;
    if(!xmqttStoreSessionConfigInit(&Import)||Import.Size!=sizeof(Import)||Import.MaxScratchBytes!=16384||!xmqttSessionConfigInit(&ImportSession))return 53;
    Import.Binding=XRT_BYTES_LITERAL("package/broker/principal/slot");Import.Generation=1;ImportSession.ClientId=XRT_STR_LITERAL("package-session");
    if(xmqttStoreSessionRestore(NULL,&ImportSession,&Import,&Imported)!=XMQTT_SESSION_ERROR||Imported!=(xmqttsession*)(uintptr_t)1)return 54;
    ImportSession.ClientId=(xstrview){0};
    if(xmqttStoreSessionRestoreAssigned(NULL,&ImportSession,&Import,&Imported)!=XMQTT_SESSION_ERROR||Imported!=(xmqttsession*)(uintptr_t)1)return 55;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
    xmqttstorestateconfig StateConfig;xmqttstorestate* State=(xmqttstorestate*)(uintptr_t)1;
    xmqttstorestateinfo StateInfo;xmqttstoreview StateView;
    if(!xmqttStoreStateConfigInit(&StateConfig))return 51;
    StateConfig.Binding=XRT_BYTES_LITERAL("package/broker/principal/slot");StateConfig.Generation=1;
    if(xmqttStoreStateRecover(NULL,&StateConfig,&State)!=XMQTT_STORE_STATE_ERROR||State!=(xmqttstorestate*)(uintptr_t)1||
       xmqttStoreStateInfo(NULL,&StateInfo)||xmqttStoreStateView(NULL,XMQTT_STORE_INBOX,0,&StateView)!=XMQTT_STORE_ERROR)return 52;
    xmqttStoreStateDestroy(NULL);xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE) && defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    {
        xmqttclient* Client=(xmqttclient*)(uintptr_t)1;
        if(xmqttClientDialStore(NULL,NULL,XRT_STR_LITERAL("localhost"),1883,0,NULL,NULL,NULL,NULL,NULL,&Client)||
           xmqttClientDialAddressesStore(NULL,NULL,0,NULL,NULL,NULL,NULL,NULL,&Client)||
           Client!=(xmqttclient*)(uintptr_t)1)return 66;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(xmqttClientDialStoreAuth(NULL,NULL,XRT_STR_LITERAL("localhost"),1883,0,NULL,NULL,NULL,NULL,NULL,NULL,&Client)||
           xmqttClientDialAddressesStoreAuth(NULL,NULL,0,NULL,NULL,NULL,NULL,NULL,NULL,&Client)||
           Client!=(xmqttclient*)(uintptr_t)1)return 67;
#endif
        xrtClearError();
    }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS)
    xmqttclientwsconfig WS;xmqttclient* WSC=(xmqttclient*)(uintptr_t)1;
    if(!xmqttClientWsConfigInit(&WS)||WS.Size!=sizeof(WS)||WS.Stream.Role!=XWS_ROLE_CLIENT||WS.MaxStagedBytes!=1048576||xmqttClientWsAttachTcp(NULL,NULL,NULL,&WS,NULL,&WSC)||WSC!=(xmqttclient*)(uintptr_t)1)return 31;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(xmqttClientWsAttachTls(NULL,NULL,NULL,&WS,NULL,&WSC)||WSC!=(xmqttclient*)(uintptr_t)1)return 32;
#endif
    xrtClearError();
#endif

#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    xmqttclientinbox* Inbox=NULL;xmqttclientinboxinfo InboxInfo;xmqttclientinboxconfig InboxConfig;
    if(!xmqttClientInboxConfigInit(&InboxConfig) || !xmqttClientInboxCreate(&InboxConfig,&Inbox) ||
       !xmqttClientInboxInfo(Inbox,&InboxInfo) || InboxInfo.Attached || InboxInfo.Objects || InboxInfo.Messages || !xmqttClientInboxDestroy(Inbox))return 30;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
    xmqttsessionconfig SessionConfig;xmqttsession* Session=NULL;xmqttsessioninfo SessionInfo;
    if(!xmqttSessionConfigInit(&SessionConfig))return 28;
    SessionConfig.ClientId=XRT_STR_LITERAL("package-session");
    if(!xmqttSessionCreate(&SessionConfig,&Session) || !xmqttSessionInfo(Session,&SessionInfo) ||
       SessionInfo.Connected || SessionInfo.Objects || SessionInfo.LiveBytes!=SessionInfo.BaseBytes || !xmqttSessionDestroy(Session))return 29;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    xmqttclienttlsresumeinfo ResumeInfo={0};xtlsresume* Resume=(xtlsresume*)(uintptr_t)1;
    if(xmqttClientTlsResumeInfo(NULL,&ResumeInfo) || xmqttClientTlsTakeResume(NULL,&Resume) || Resume!=(xtlsresume*)(uintptr_t)1)return 27;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqttclienttlsconfig TLS;xmqttclient* TLSClient=(xmqttclient*)(uintptr_t)1;
    if(!xmqttClientTlsConfigInit(&TLS) || TLS.MaxMetadataBytes!=65536 || !TLS.ServerNameFromHost || TLS.ClientAuth ||
       xmqttClientTlsConnect(NULL,NULL,0,NULL,&TLS,NULL,&TLSClient) || TLSClient!=(xmqttclient*)(uintptr_t)1 ||
       xmqttClientTlsDial(NULL,NULL,(xstrview){0},0,0,NULL,&TLS,NULL,&TLSClient) ||
       xmqttClientTlsDialAddresses(NULL,NULL,0,NULL,&TLS,NULL,&TLSClient))return 25;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(xmqttClientTlsConnectAuth(NULL,NULL,0,NULL,&TLS,NULL,NULL,&TLSClient) ||
       xmqttClientTlsDialAuth(NULL,NULL,(xstrview){0},0,0,NULL,&TLS,NULL,NULL,&TLSClient) ||
       xmqttClientTlsDialAddressesAuth(NULL,NULL,0,NULL,&TLS,NULL,NULL,&TLSClient))return 26;
#endif
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xmqttclientdialconfig Dial;xmqttclient* DialClient=(xmqttclient*)(uintptr_t)1;
    if(!xmqttClientDialConfigInit(&Dial) || Dial.MaxAttempts!=8 ||
       xmqttClientDialAddresses(NULL,NULL,0,NULL,&Dial,NULL,&DialClient) || DialClient!=(xmqttclient*)(uintptr_t)1 ||
       xmqttClientDial(NULL,NULL,(xstrview){0},0,0,NULL,&Dial,NULL,&DialClient))return 23;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(xmqttClientDialAuth(NULL,NULL,(xstrview){0},0,0,NULL,&Dial,NULL,NULL,&DialClient) ||
       xmqttClientDialAddressesAuth(NULL,NULL,0,NULL,&Dial,NULL,NULL,&DialClient))return 24;
#endif
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttauthconfig Auth;xmqttclient* AuthClient=(xmqttclient*)(uintptr_t)1;
    if(!xmqttAuthConfigInit(&Auth) || Auth.MaxRounds!=32 || Auth.ResponseTimeout!=5000 ||
       xmqttClientConnectAuth(NULL,NULL,0,NULL,&Auth,NULL,&AuthClient) || AuthClient!=(xmqttclient*)(uintptr_t)1)return 21;
    if(xmqttAuthRespond(NULL,NULL)!=XMQTT_ADMISSION_ERROR || xmqttAuthComplete(NULL,true)!=XMQTT_ADMISSION_ERROR ||
       xmqttClientReauthenticateAsync(NULL,NULL,1,NULL)!=XMQTT_ADMISSION_ERROR || xmqttClientReauthenticate(NULL,NULL,1,NULL)!=XMQTT_ADMISSION_ERROR)return 22;
    xmqttAuthRelease(NULL);if(xmqttAuthMessage(NULL) || xmqttAuthPhase(NULL)!=XMQTT_AUTH_CHALLENGE)return 23;
    xrtClearError();
#endif

#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
    xmqttclientconfig ClientConfig;xmqttclient* Client=(xmqttclient*)(uintptr_t)1;
    if(!xmqttClientConfigInit(&ClientConfig) || ClientConfig.Version!=XMQTT_VERSION_5 ||
       xmqttClientConnect(NULL,NULL,0,&ClientConfig,NULL,&Client) || Client!=(xmqttclient*)(uintptr_t)1 || !xmqttClientDestroy(NULL))return 20;
    xrtClearError();
#endif

#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
    if(xmqttClientPollRetired(NULL)!=XWAIT_ERROR || xrtErrorKind(xrtGetError())!=XERR_ARGUMENT)return 31;
    xrtClearError();
#endif
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    xmqttsubscriptionsconfig Subscriptions;xmqttreconnect* SubscriptionOwner=(xmqttreconnect*)(uintptr_t)1;
    xmqttreconnectconfig Reconnect;
    if(!xmqttReconnectConfigInit(&Reconnect) || Reconnect.Subscriptions || Reconnect.Store ||
       !xmqttSubscriptionsConfigInit(&Subscriptions) || Subscriptions.MaxFilters!=1024 || Subscriptions.MaxBatch!=32 || Subscriptions.RetryDelay!=5) return 32;
    const xmqttreconnectsubscriptionplan SubscriptionPlan={.Config=&Subscriptions};
    Reconnect.Subscriptions=&SubscriptionPlan;
    if(xmqttReconnectCreate(NULL,NULL,0,&Reconnect,NULL,&SubscriptionOwner) || SubscriptionOwner!=(xmqttreconnect*)(uintptr_t)1 ||
       xmqttReconnectSubscribeIntent(NULL,NULL,NULL)!=XMQTT_ADMISSION_ERROR || xmqttReconnectUnsubscribeIntent(NULL,NULL,NULL)!=XMQTT_ADMISSION_ERROR ||
       xmqttReconnectSubscriptionsInfo(NULL,NULL) || xmqttReconnectSubscriptionInfo(NULL,(xstrview){0},NULL))return 32;
    xrtClearError();
#endif
	const unsigned char ConnectName[] = {0x10,13,0,4,'M','Q','T','T',5};
	xmqttversion AcceptedVersion = XMQTT_VERSION_311;
	if (xmqttConnectVersionRead((xbytesview){ConnectName,sizeof(ConnectName)},15,&AcceptedVersion) != XMQTT_WIRE_DONE ||
		AcceptedVersion != XMQTT_VERSION_5) return 33;
	unsigned char Output[128]; size_t Written; xmqttframe Frame;
	if (!xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PINGREQ,0,0,NULL,&Written) || Written != 2 ||
		xmqttFrameRead((xbytesview){Output,Written},NULL,&Frame) != XMQTT_WIRE_DONE || Frame.Header.Type != XMQTT_PACKET_PINGREQ) return 1;
	xstrview Text; size_t Read;
	if (!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\xef\xbb\xbf"),0,&Written) ||
		xmqttUtf8StringRead((xbytesview){Output,Written},0,&Text,&Read) != XMQTT_WIRE_DONE || Text.Size != 4 || memcmp(Text.Data,"a\xef\xbb\xbf",4)) return 2;
#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
	{
	xmqttproperty Properties[2]={0}; Properties[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Properties[0].Type=XMQTT_PROPERTY_BYTE; Properties[0].Value.Number=1;
	const unsigned char Data[]={0,255}; Properties[1].Id=XMQTT_PROPERTY_CORRELATION_DATA; Properties[1].Type=XMQTT_PROPERTY_BINARY; Properties[1].Value.Binary=(xbytesview){Data,2};
	xmqttproperty Parsed[2]; xmqttpropertydocument Doc;
	if (!xmqttPropertiesWrite(Output,sizeof(Output),Properties,2,XMQTT_PROPERTIES_PUBLISH,XMQTT_CLIENT_TO_SERVER,NULL,&Written) ||
		xmqttPropertiesRead((xbytesview){Output,Written},XMQTT_PROPERTIES_PUBLISH,XMQTT_CLIENT_TO_SERVER,NULL,Parsed,2,&Doc)!=XMQTT_PROPERTIES_DONE ||
		Doc.Count!=2 || Parsed[0].Value.Number!=1 || Parsed[1].Value.Binary.Size!=2 || memcmp(Parsed[1].Value.Binary.Data,Data,2)) return 4;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CODEC5)
	{
	xmqttproperty Properties[1]={{0}},Parsed[2];Properties[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT;Properties[0].Type=XMQTT_PROPERTY_BYTE;Properties[0].Value.Number=1;
	xmqttpacket Packet, Decoded; xmqttcodecconfig Config; xmqttcodecissue Issue;
	if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet) || !xmqttCodecConfigInit(&Config)) return 5;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("pkg/test");
	const unsigned char Payload[]={'x',0}; Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
	Packet.Properties=(xmqttpropertylist){Properties,1}; size_t Required;
	xmqttcodecbuffers Buffers={Parsed,2,NULL,0};
	if (!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) ||
		!xmqttPacketWrite5(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) || Required!=Written ||
		xmqttPacketRead5((xbytesview){Output,Written},XMQTT_CLIENT_TO_SERVER,&Config,&Buffers,&Decoded,&Issue)!=XMQTT_CODEC_DONE ||
		Decoded.Identifier!=42 || Decoded.Body.Publish.Payload.Size!=sizeof(Payload) || memcmp(Decoded.Body.Publish.Payload.Data,Payload,sizeof(Payload)) ||
		Decoded.Wire.Data!=Output || Decoded.Wire.Size!=Written || Issue.Error!=XMQTT_CODEC_OK) return 6;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	{
	xmqttpacket Packet, Decoded; xmqttcodecconfig Config; xmqttcodecissue Issue;
	if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet) || !xmqttCodecConfigInit(&Config)) return 5;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("pkg/test");
	const unsigned char Payload[]={'x',0}; Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
	size_t Required;
	Packet.Properties=(xmqttpropertylist){NULL,0}; const unsigned char Raw[]={255,0,0xc0,0x80}; Packet.Body.Publish.Payload=(xbytesview){Raw,sizeof(Raw)};
	Config.MaxPropertyBytes=0; Config.MaxProperties=0;
	if (!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) ||
		!xmqttPacketWrite311(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) || Written!=Required ||
		xmqttPacketRead311((xbytesview){Output,Written},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Decoded,&Issue)!=XMQTT_CODEC_DONE ||
		Decoded.Identifier!=42 || Decoded.Body.Publish.Payload.Size!=sizeof(Raw) || memcmp(Decoded.Body.Publish.Payload.Data,Raw,sizeof(Raw)) ||
		Decoded.Properties.Count!=0 || Issue.Error!=XMQTT_CODEC_OK) return 7;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
	{
	xmqttpacket Packet;size_t Required;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);Packet.Body.Publish.Topic=XRT_STR_LITERAL("package");
	xmqttpacket Connect,Connack; xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect); xmqttPacketInit(XMQTT_PACKET_CONNACK,&Connack);
	Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("package");xmqttnegotiated Peer;
	if(xmqttNegotiate(&Connect,&Connack,NULL,&Peer,NULL)!=XMQTT_NEGOTIATION_DONE || !Peer.Accepted || Peer.SendReceiveMaximum!=65535 ||
	   xmqttNegotiatedPacketCheck(&Peer,&Packet,NULL,&Required,NULL)!=XMQTT_NEGOTIATION_DONE)return 8;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL)
	{xmqttjournalconfig C;xmqttjournal* J=(xmqttjournal*)(uintptr_t)1;
	if(!xmqttJournalConfigInit(&C)||C.Generation||C.MaxTransactionBytes!=1048576||xmqttJournalOpen(NULL,&C,&J)!=XMQTT_JOURNAL_ERROR||J!=(xmqttjournal*)(uintptr_t)1)return 40;
	xrtClearError();}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_DELTA)
	{xmqttstoredeltaconfig C;xmqttstoredeltadocument D;unsigned char B[32];size_t N=0;
	if(!xmqttStoreDeltaConfigInit(&C)||xmqttStoreDeltaSize(&C,NULL,0,NULL,0,&N)!=XMQTT_STORE_DELTA_DONE||N!=32||xmqttStoreDeltaWrite(&C,NULL,0,NULL,0,B,sizeof(B),&N)!=XMQTT_STORE_DELTA_DONE||xmqttStoreDeltaRead(&C,(xbytesview){B,N},NULL,0,NULL,0,&D)!=XMQTT_STORE_DELTA_DONE||D.Count)return 41;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    {xmqttstorewriteconfig C;xmqttstorewrite* W=(xmqttstorewrite*)(uintptr_t)1;xfuture* F=(xfuture*)(uintptr_t)1;size_t Need=99;
    if(!xmqttStoreWriteConfigInit(&C)||C.MaxScratchBytes!=1048576||C.MaxModelWork!=65536||C.Worker.WorkTransactions!=32||
       xmqttStoreWriteCreate(NULL,NULL,NULL,&C,&W)||W!=(xmqttstorewrite*)(uintptr_t)1||
       xmqttStoreWriteAppendAsync(NULL,XRT_BYTES_LITERAL("x"),100,&F)!=XMQTT_JWORK_ERROR||F!=(xfuture*)(uintptr_t)1||
       xmqttStoreWriteInfo(NULL,NULL)||xmqttStoreWriteRead(NULL,1,NULL,0,NULL,&Need)!=XMQTT_STORE_ERROR||Need!=99||
       xmqttStoreWriteClose(NULL)||xmqttStoreWriteWaitFor(NULL,0)!=XWAIT_ERROR||!xmqttStoreWriteDestroy(NULL))return 45;
    xrtClearError();}
#endif
	if (xmqttFrameRead((xbytesview){NULL,1},NULL,&Frame) != XMQTT_WIRE_ERROR || xrtErrorDomain(xrtGetError()) == NULL ||
		strcmp(xrtErrorDomain(xrtGetError()),"xrt.mqtt") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 3;
	xrtClearError(); return 0;
}
