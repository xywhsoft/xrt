#include "test.h"
static xmqttproperty negNumber(xmqttpropertyid Id,uint32 Number)
{
    xmqttproperty P={0};P.Id=Id;P.Type=XMQTT_PROPERTY_UINT16;P.Value.Number=Number;
    if(Id==XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE || Id==XMQTT_PROPERTY_SESSION_EXPIRY)P.Type=XMQTT_PROPERTY_UINT32;
    if(Id==XMQTT_PROPERTY_MAXIMUM_QOS || Id==XMQTT_PROPERTY_RETAIN_AVAILABLE || Id==XMQTT_PROPERTY_WILDCARD_AVAILABLE || Id==XMQTT_PROPERTY_SHARED_AVAILABLE || Id==XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE || Id==XMQTT_PROPERTY_REQUEST_RESPONSE_INFO || Id==XMQTT_PROPERTY_REQUEST_PROBLEM_INFO)P.Type=XMQTT_PROPERTY_BYTE;
    if(Id==XMQTT_PROPERTY_SUBSCRIPTION_ID)P.Type=XMQTT_PROPERTY_VARIABLE;
    return P;
}
static xmqttproperty negText(xmqttpropertyid Id,xstrview Text)
{xmqttproperty P={0};P.Id=Id;P.Type=XMQTT_PROPERTY_STRING;P.Value.Text=Text;return P;}
static void negPackets(xmqttpacket* C,xmqttpacket* A)
{testRequire(xmqttPacketInit(XMQTT_PACKET_CONNECT,C) && xmqttPacketInit(XMQTT_PACKET_CONNACK,A),"negotiation packet init");C->Body.Connect.CleanStart=true;C->Body.Connect.ClientId=XRT_STR_LITERAL("client");C->Body.Connect.KeepAlive=60;}
static void negFailure(const xmqttpacket* C,const xmqttpacket* A,const xmqttnegotiationconfig* Config,xmqttnegotiationstatus S,xmqttnegotiationerror E)
{
    xmqttnegotiated N,Before;memset(&N,0xa5,sizeof(N));Before=N;xmqttnegotiationissue I={0};
    testRequire(xmqttNegotiate(C,A,Config,&N,&I)==S && I.Status==S && I.Error==E && !memcmp(&N,&Before,sizeof(N)) && xrtErrorKind(xrtGetError())==(S==XMQTT_NEGOTIATION_PROTOCOL_ERROR?XERR_PROTOCOL:XERR_STATE),"negotiation failure status/atomicity/kind");xrtClearError();
}
static void negOutboundFailure(const xmqttnegotiated* N,const xmqttpacket* P,xmqttnegotiationerror E)
{
    size_t Bytes=123456;xmqttnegotiationissue I={0};
    xmqttnegotiationstatus S=xmqttNegotiatedPacketCheck(N,P,NULL,&Bytes,&I);
    if(S!=XMQTT_NEGOTIATION_RESTRICTED || I.Error!=E || Bytes!=123456)fprintf(stderr,"outbound type=%u status=%d error=%d expected=%d bytes=%zu diagnostic=%d\n",(unsigned)P->Type,(int)S,(int)I.Error,(int)E,Bytes,(int)xrtErrorCode(xrtGetError()));
    testRequire(S==XMQTT_NEGOTIATION_RESTRICTED && I.Error==E && Bytes==123456,"outbound capability failure/atomicity");xrtClearError();
}
static void testNegotiationDefaultsAndWire(void)
{
    for(unsigned Version=4;Version<=5;++Version){xmqttpacket C,A;negPackets(&C,&A);xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=(xmqttversion)Version;
        xmqttnegotiated N;xmqttnegotiationissue I;memset(&I,0xa5,sizeof(I));
        testRequire(xmqttNegotiate(&C,&A,&K,&N,&I)==XMQTT_NEGOTIATION_DONE && N.Accepted && N.Version==(xmqttversion)Version && !N.SessionPresent && !N.HasAuthMethod && N.KeepAlive==60 &&
            N.MaximumQos==2 && N.RetainAvailable && N.SharedAvailable && N.WildcardAvailable && N.SubscriptionIdsAvailable && N.SendReceiveMaximum==65535 && N.ReceiveMaximum==65535 &&
            N.SendPacketBytes==XMQTT_PACKET_MAX_BYTES && N.ReceivePacketBytes==XMQTT_PACKET_MAX_BYTES && !N.SendAliasMaximum && !N.ReceiveAliasMaximum && N.ClientId.Data==C.Body.Connect.ClientId.Data && !I.Error,"negotiated protocol defaults");
        xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PINGREQ,&P);size_t Bytes=0;testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,&I)==XMQTT_NEGOTIATION_DONE && Bytes==2 && !I.Error,"negotiated PING size");
        for(unsigned Reason=1;Reason<=(Version==4?5u:1u);++Reason){A.Reason=Version==4?(uint8)Reason:0x87;
            testRequire(xmqttNegotiate(&C,&A,&K,&N,&I)==XMQTT_NEGOTIATION_REJECTED && !N.Accepted && N.Reason==A.Reason && !N.SendPacketBytes && !N.ClientId.Size && !I.Error,"ordinary connection refusal not protocol failure");}
    }
    const unsigned char CW[]={0x10,0x11,0,4,'M','Q','T','T',5,2,0,60,0,0,4,'d','e','m','o'};
    const unsigned char AW[]={0x20,0x13,0,0,0x10,0x13,0,30,0x21,0,4,0x24,1,0x27,0,0,4,0,0x22,0,8};
    xmqttpacket C,A;xmqttproperty Props[5];xmqttcodecbuffers B={Props,5,NULL,0};xmqttnegotiated N;
    testRequire(xmqttPacketRead5((xbytesview){CW,sizeof(CW)},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&C,NULL)==XMQTT_CODEC_DONE &&
        xmqttPacketRead5((xbytesview){AW,sizeof(AW)},XMQTT_SERVER_TO_CLIENT,NULL,&B,&A,NULL)==XMQTT_CODEC_DONE &&
        xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.KeepAlive==30 && N.SendReceiveMaximum==4 && N.MaximumQos==1 && N.SendPacketBytes==1024 && N.SendAliasMaximum==8,"independent literal CONNECT/CONNACK negotiation");
    memset(Props,0,sizeof(Props));testRequire(N.KeepAlive==30 && N.SendPacketBytes==1024 && !memcmp(N.ClientId.Data,"demo",4),"result retained property scratch");
}
static void testNegotiationSession(void)
{
    xmqttpacket C,A;negPackets(&C,&A);A.SessionPresent=true;negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION);
    C.Body.Connect.CleanStart=false;negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_STATE_ERROR,XMQTT_NEGOTIATION_SESSION);
    xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.HasSessionState=true;xmqttnegotiated N;
    testRequire(xmqttNegotiate(&C,&A,&K,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.SessionPresent,"matching resumed session");
    K.Version=XMQTT_VERSION_311;K.HasSessionState=false;testRequire(xmqttNegotiate(&C,&A,&K,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.SessionPresent,"3.1.1 caller mismatch policy replaced by 5.0 rule");
    K.Version=XMQTT_VERSION_5;A.SessionPresent=false;testRequire(xmqttNegotiate(&C,&A,&K,&N,NULL)==XMQTT_NEGOTIATION_DONE && !N.SessionPresent,"new session not accepted");
    C.Body.Connect.ClientId=(xstrview){0};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID);
    char Assigned[]="assigned";xmqttproperty P=negText(XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,(xstrview){Assigned,8});A.Properties=(xmqttpropertylist){&P,1};
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.ServerAssignedId && N.ClientId.Data==Assigned && N.ClientId.Size==8,"assigned identity borrowed bytes");
    P.Value.Text=(xstrview){0};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID);P.Value.Text=(xstrview){Assigned,8};
    A.SessionPresent=true;K.HasSessionState=true;negFailure(&C,&A,&K,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION);A.SessionPresent=false;
    C.Body.Connect.ClientId=XRT_STR_LITERAL("client");negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID);
    P.Value.Text=C.Body.Connect.ClientId;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && !N.ServerAssignedId,"redundant identical client id");
    A.Reason=0x85;C.Body.Connect.ClientId=(xstrview){0};A.Properties=(xmqttpropertylist){0};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_REJECTED,"empty ClientId refusal wrongly requires assignment");
}
static void testNegotiationProperties(void)
{
    xmqttpacket C,A;negPackets(&C,&A);xmqttproperty CP[]={negNumber(XMQTT_PROPERTY_RECEIVE_MAXIMUM,12),negNumber(XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,17),negNumber(XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,4096),negNumber(XMQTT_PROPERTY_SESSION_EXPIRY,100)};
    xmqttproperty AP[]={negNumber(XMQTT_PROPERTY_RECEIVE_MAXIMUM,7),negNumber(XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,11),negNumber(XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,UINT32_MAX),negNumber(XMQTT_PROPERTY_SESSION_EXPIRY,200),negNumber(XMQTT_PROPERTY_SERVER_KEEP_ALIVE,0)};
    C.Properties=(xmqttpropertylist){CP,4};A.Properties=(xmqttpropertylist){AP,5};xmqttnegotiated N;
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.ReceiveMaximum==12 && N.ReceiveAliasMaximum==17 && N.ReceivePacketBytes==4096 && N.SendReceiveMaximum==7 &&
        N.SendAliasMaximum==11 && N.SendPacketBytes==XMQTT_PACKET_MAX_BYTES && N.RequestedSessionExpiry==100 && N.SessionExpiry==200 && !N.KeepAlive,"directional limits and server overrides");
    AP[2].Value.Number=1;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.SendPacketBytes==1,"one-byte peer packet maximum rejected as descriptor");
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PINGREQ,&P);negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_PACKET_SIZE);
    AP[2].Value.Number=10;size_t AckBytes;testRequire(xmqttPacketSize5(&A,XMQTT_SERVER_TO_CLIENT,NULL,&AckBytes),"CONNACK cap fixture");CP[2].Value.Number=(uint32)AckBytes;
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"exact client receive packet cap");--CP[2].Value.Number;negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_PACKET_SIZE);
    negPackets(&C,&A);xmqttproperty R=negText(XMQTT_PROPERTY_RESPONSE_INFO,XRT_STR_LITERAL("reply/root"));A.Properties=(xmqttpropertylist){&R,1};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RESPONSE_INFO);
    xmqttproperty Q=negNumber(XMQTT_PROPERTY_REQUEST_RESPONSE_INFO,1);C.Properties=(xmqttpropertylist){&Q,1};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.ResponseInfo.Data==R.Value.Text.Data,"requested response information");
    A.Properties=(xmqttpropertylist){0};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"optional response information required");
    negPackets(&C,&A);xmqttproperty D=negNumber(XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,0),Reason=negText(XMQTT_PROPERTY_REASON_STRING,XRT_STR_LITERAL("public"));C.Properties=(xmqttpropertylist){&D,1};A.Properties=(xmqttpropertylist){&Reason,1};
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"problem-info zero incorrectly bans CONNACK reason");
    xmqttproperty Ref=negText(XMQTT_PROPERTY_SERVER_REFERENCE,XRT_STR_LITERAL("other.invalid:1883"));A.Reason=0x9c;A.Properties=(xmqttpropertylist){&Ref,1};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_REJECTED && N.ServerReference.Data==Ref.Value.Text.Data && !N.AuthMethod.Size,"explicit redirect refusal metadata");
}
static void testNegotiationAuthentication(void)
{
    xmqttpacket C,A;negPackets(&C,&A);xmqttproperty M=negText(XMQTT_PROPERTY_AUTH_METHOD,XRT_STR_LITERAL("fixture"));C.Properties=(xmqttpropertylist){&M,1};
    negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD);xmqttproperty AM=M;A.Properties=(xmqttpropertylist){&AM,1};xmqttnegotiated N;
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.HasAuthMethod && N.AuthMethod.Data==M.Value.Text.Data,"matching auth method");
    AM.Value.Text=XRT_STR_LITERAL("other");negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD);
    A.Reason=0x8c;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_REJECTED,"failed CONNACK unnecessarily requires matching method");A.Reason=0;AM=M;
    C.Properties=(xmqttpropertylist){0};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD);
    xmqttproperty AD={0};AD.Id=XMQTT_PROPERTY_AUTH_DATA;AD.Type=XMQTT_PROPERTY_BINARY;A.Properties=(xmqttpropertylist){&AD,1};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_DATA);
    C.Properties=(xmqttpropertylist){&M,1};A.Properties=(xmqttpropertylist){&AM,1};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"auth ready");
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_AUTH,&P);P.Reason=0x19;size_t Bytes=77;xmqttnegotiationissue Missing={0};
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,&Missing)==XMQTT_NEGOTIATION_LOCAL_ERROR && !Missing.Error && Bytes==77,"AUTH without required method is an invalid descriptor");xrtClearError();
    P.Properties=(xmqttpropertylist){&AM,1};Bytes=0;
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,NULL)==XMQTT_NEGOTIATION_DONE && Bytes>2,"reauth method constraint");
    AM.Value.Text=XRT_STR_LITERAL("different");negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_AUTH_METHOD);
    M.Value.Text=AM.Value.Text=(xstrview){0};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.HasAuthMethod && !N.AuthMethod.Size && xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==XMQTT_NEGOTIATION_DONE,"present empty auth method conflated with absent");
}
static void testNegotiationOutbound(void)
{
    xmqttpacket C,A,P;negPackets(&C,&A);xmqttproperty Cap[]={negNumber(XMQTT_PROPERTY_MAXIMUM_QOS,0),negNumber(XMQTT_PROPERTY_RETAIN_AVAILABLE,0),negNumber(XMQTT_PROPERTY_WILDCARD_AVAILABLE,0),negNumber(XMQTT_PROPERTY_SHARED_AVAILABLE,0),negNumber(XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE,0),negNumber(XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,2)};
    A.Properties=(xmqttpropertylist){Cap,6};xmqttnegotiated N;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"restricted peer");
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Identifier=1;P.Body.Publish.Topic=XRT_STR_LITERAL("topic");negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_QOS);
    P.Flags=1;P.Identifier=0;negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_RETAIN);P.Flags=0;
    xmqttproperty Alias=negNumber(XMQTT_PROPERTY_TOPIC_ALIAS,3);P.Properties=(xmqttpropertylist){&Alias,1};negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_TOPIC_ALIAS);Alias.Value.Number=2;
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==XMQTT_NEGOTIATION_DONE,"exact alias maximum");
    xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&P);P.Identifier=2;xmqttsubscription T={XRT_STR_LITERAL("plain"),2};P.Body.Topics=(xmqttsubscriptionlist){&T,1};
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==XMQTT_NEGOTIATION_DONE,"server maximum PUBLISH QoS restricted requested subscription QoS");
    T.Filter=XRT_STR_LITERAL("a/+");negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_WILDCARD);T.Filter=XRT_STR_LITERAL("$share/g/plain");negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_SHARED);
    T.Filter=XRT_STR_LITERAL("plain");xmqttproperty Sub=negNumber(XMQTT_PROPERTY_SUBSCRIPTION_ID,1);P.Properties=(xmqttpropertylist){&Sub,1};negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_SUBSCRIPTION_ID);
    xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&P);P.Identifier=3;T.Filter=XRT_STR_LITERAL("$share/g/a/+");T.Options=0;P.Body.Topics=(xmqttsubscriptionlist){&T,1};
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==XMQTT_NEGOTIATION_DONE,"disabled feature prevents old subscription removal");
    xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&P);xmqttproperty Expiry=negNumber(XMQTT_PROPERTY_SESSION_EXPIRY,1);P.Properties=(xmqttpropertylist){&Expiry,1};negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_SESSION_EXPIRY);
    xmqttproperty Override=negNumber(XMQTT_PROPERTY_SESSION_EXPIRY,999);A.Properties=(xmqttpropertylist){&Override,1};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.SessionExpiry==999,"server expiry override");negOutboundFailure(&N,&P,XMQTT_NEGOTIATION_SESSION_EXPIRY);
    C.Properties=(xmqttpropertylist){&Expiry,1};testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==XMQTT_NEGOTIATION_DONE,"positive CONNECT expiry permits DISCONNECT override");
    size_t Bytes=19;xmqttnegotiationissue I;testRequire(xmqttNegotiatedPacketCheck(&N,&C,NULL,&Bytes,&I)==XMQTT_NEGOTIATION_STATE_ERROR && Bytes==19,"second CONNECT accepted");xrtClearError();
    negPackets(&C,&A);C.Body.Connect.HasWill=true;C.Body.Connect.WillTopic=XRT_STR_LITERAL("last");C.Body.Connect.WillQos=1;A.Properties=(xmqttpropertylist){Cap,1};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_QOS);
    C.Body.Connect.WillQos=0;C.Body.Connect.WillRetain=true;A.Properties=(xmqttpropertylist){Cap+1,1};negFailure(&C,&A,NULL,XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RETAIN);A.Reason=0x9a;
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_REJECTED,"proper Will refusal treated as accepted capability violation");
}
static void testNegotiationAliases(void)
{
    xmqttpacket C,A;negPackets(&C,&A);xmqttnegotiated N,Before;xmqttnegotiationissue I,OldI;memset(&N,0xa5,sizeof(N));Before=N;memset(&I,0x5a,sizeof(I));OldI=I;
    testRequire(xmqttNegotiate(NULL,&A,NULL,&N,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(&N,&Before,sizeof(N)) && !memcmp(&I,&OldI,sizeof(I)),"bad negotiation API modified outputs");xrtClearError();
    xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);--K.Size;testRequire(xmqttNegotiate(&C,&A,&K,&N,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(&N,&Before,sizeof(N)) && !memcmp(&I,&OldI,sizeof(I)),"config size validation");xrtClearError();
    testRequire(xmqttNegotiate(&C,&A,NULL,(xmqttnegotiated*)&C,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(&I,&OldI,sizeof(I)),"result aliases input descriptor");xrtClearError();
    union {max_align_t Align;unsigned char Data[512];} Storage;memset(Storage.Data,'q',sizeof(Storage.Data));unsigned char Saved[512];memcpy(Saved,Storage.Data,512);
    C.Body.Connect.ClientId=(xstrview){(cstr)Storage.Data,32};testRequire(xmqttNegotiate(&C,&A,NULL,(xmqttnegotiated*)Storage.Data,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(Saved,Storage.Data,512) && !memcmp(&I,&OldI,sizeof(I)),"result aliases active client id");xrtClearError();
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,(xmqttnegotiationissue*)Storage.Data)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(Saved,Storage.Data,512),"issue aliases active client id");xrtClearError();
    negPackets(&C,&A);testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"alias fixture readiness");xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("topic");P.Body.Publish.Payload=(xbytesview){Storage.Data,128};
    testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,(size_t*)Storage.Data,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && !memcmp(Saved,Storage.Data,512),"required aliases payload");xrtClearError();
    P.Body.Publish.Payload=(xbytesview){NULL,1};size_t Bytes=987;testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,&I)==XMQTT_NEGOTIATION_LOCAL_ERROR && Bytes==987,"invalid descriptor publication");xrtClearError();
    testRequire(xmqttNegotiate(&C,&A,NULL,(xmqttnegotiated*)(uintptr_t)(UINTPTR_MAX-1u),NULL)==XMQTT_NEGOTIATION_LOCAL_ERROR,"wrapped result range");xrtClearError();
}
static void testNegotiationMatrices(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Clean=0;Clean<2;++Clean)for(unsigned Present=0;Present<2;++Present)for(unsigned Local=0;Local<2;++Local){
        xmqttpacket C,A;negPackets(&C,&A);C.Body.Connect.CleanStart=Clean!=0;A.SessionPresent=Present!=0;xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=(xmqttversion)V;K.HasSessionState=Local!=0;
        xmqttnegotiationstatus Expected=Present && Clean?XMQTT_NEGOTIATION_PROTOCOL_ERROR:V==5 && Present && !Local?XMQTT_NEGOTIATION_STATE_ERROR:XMQTT_NEGOTIATION_DONE;
        xmqttnegotiated N;testRequire(xmqttNegotiate(&C,&A,&K,&N,NULL)==Expected,"exhaustive version/clean/present/local-state matrix");xrtClearError();
    }
    const uint32 Limits[]={1,2,65535,268435460,UINT32_MAX};
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I)for(unsigned Small=0;Small<2;++Small){
        xmqttpacket C,A;negPackets(&C,&A);xmqttproperty P[]={negNumber(XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Limits[I]),negNumber(XMQTT_PROPERTY_RECEIVE_MAXIMUM,Small?1:65535),negNumber(XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,Small?0:65535),negNumber(XMQTT_PROPERTY_SESSION_EXPIRY,Small?0:UINT32_MAX),negNumber(XMQTT_PROPERTY_SERVER_KEEP_ALIVE,Small?0:65535)};
        A.Properties=(xmqttpropertylist){P,5};xmqttnegotiated N;
        testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && N.SendPacketBytes==(Limits[I]>268435460?268435460:Limits[I]) && N.SendReceiveMaximum==(Small?1:65535) &&
            N.SendAliasMaximum==(Small?0:65535) && N.SessionExpiry==(Small?0:UINT32_MAX) && N.KeepAlive==(Small?0:65535),"numeric negotiation boundaries/default independence");
    }
    /* Every server capability subset: the independent requested QoS remains 2.
     * Restrictions concern this exact operation, never a silent downgrade. */
    for(unsigned Mask=0;Mask<32;++Mask){xmqttpacket C,A,P;negPackets(&C,&A);xmqttproperty Caps[]={negNumber(XMQTT_PROPERTY_MAXIMUM_QOS,(Mask&1)?1:0),negNumber(XMQTT_PROPERTY_RETAIN_AVAILABLE,(Mask>>1)&1),negNumber(XMQTT_PROPERTY_WILDCARD_AVAILABLE,(Mask>>2)&1),negNumber(XMQTT_PROPERTY_SHARED_AVAILABLE,(Mask>>3)&1),negNumber(XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE,(Mask>>4)&1)};
        A.Properties=(xmqttpropertylist){Caps,5};xmqttnegotiated N;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE,"capability matrix negotiate");
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=1;P.Body.Publish.Topic=XRT_STR_LITERAL("topic");
        testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==((Mask&2)?XMQTT_NEGOTIATION_DONE:XMQTT_NEGOTIATION_RESTRICTED),"retain capability oracle");xrtClearError();
        xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&P);P.Identifier=1;xmqttsubscription Topic={XRT_STR_LITERAL("a/+"),2};P.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
        testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==((Mask&4)?XMQTT_NEGOTIATION_DONE:XMQTT_NEGOTIATION_RESTRICTED),"wildcard capability oracle");xrtClearError();
        Topic.Filter=XRT_STR_LITERAL("$share/g/plain");testRequire(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,NULL)==((Mask&8)?XMQTT_NEGOTIATION_DONE:XMQTT_NEGOTIATION_RESTRICTED),"shared capability oracle");xrtClearError();
    }
}
static void testNegotiationAllocation(void)
{
    xmqttpacket C,A,P;negPackets(&C,&A);xmqttPacketInit(XMQTT_PACKET_PINGREQ,&P);xmqttnegotiated N;size_t Bytes=0;
    xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
    #if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
    #endif
    for(unsigned I=0;I<10000;++I){
        A.Reason=0;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,NULL)==XMQTT_NEGOTIATION_DONE && Bytes==2,"warm valid negotiation/check");
        A.Reason=0x87;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_REJECTED && xrtGetError()==Ambient,"warm refusal/ambient error");
    }
    #if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(After.BlockAllocCalls==Before.BlockAllocCalls && After.MallocCalls==Before.MallocCalls && After.CallocCalls==Before.CallocCalls && After.ReallocCalls==Before.ReallocCalls,"successful/refused negotiation allocated");
    #endif
    xrtClearError();xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"negotiation quota enter");
    A.Reason=0;testRequire(xmqttNegotiate(&C,&A,NULL,&N,NULL)==XMQTT_NEGOTIATION_DONE && xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,NULL)==XMQTT_NEGOTIATION_DONE,"valid negotiation needs heap");
    A.SessionPresent=true;xmqttnegotiationissue Issue;xmqttnegotiated Saved=N;
    testRequire(xmqttNegotiate(&C,&A,NULL,&N,&Issue)==XMQTT_NEGOTIATION_PROTOCOL_ERROR && Issue.Error==XMQTT_NEGOTIATION_SESSION && !memcmp(&Saved,&N,sizeof(N)) && xrtErrorKind(xrtGetError())==XERR_MEMORY,"diagnostic OOM lost semantic failure");
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=2;Pub.Identifier=1;Pub.Body.Publish.Topic=XRT_STR_LITERAL("topic");N.MaximumQos=0;Bytes=99;
    testRequire(xmqttNegotiatedPacketCheck(&N,&Pub,NULL,&Bytes,&Issue)==XMQTT_NEGOTIATION_RESTRICTED && Issue.Error==XMQTT_NEGOTIATION_QOS && Bytes==99 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"diagnostic OOM lost restriction");
    testRequire(xrtAllocationBudgetLeave(&Scope),"negotiation quota leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);
    testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && Info.Rejections,"negotiation diagnostic budget leak");xrtClearError();xrtAllocationBudgetFree(Budget);
}
int main(void)
{testNegotiationDefaultsAndWire();testNegotiationSession();testNegotiationProperties();testNegotiationAuthentication();testNegotiationOutbound();testNegotiationAliases();testNegotiationMatrices();testNegotiationAllocation();puts("[PASS] xmqtt negotiated connection: defaults/session/identity/auth/capabilities/original-expiry/atomicity/aliases/matrices/zero-heap/OOM");return 0;}
