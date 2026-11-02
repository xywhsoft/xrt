#include <xmqtt/negotiation.h>
#include <xrt/memory.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
static bool xmqttNegSpan(const void* P,size_t N){return (P || !N) && xrtMemRangeValid(P,N);}
static bool xmqttNegDisjoint(const void* O,size_t N,const void* P,size_t B)
{return !N || !B || !xrtMemRangesOverlap(O,N,P,B);}
static bool xmqttNegView(const void* O,size_t N,xstrview V)
{return xmqttNegDisjoint(O,N,V.Data,V.Size);}
static bool xmqttNegPropertiesOutput(const void* O,size_t N,xmqttpropertylist L)
{
    if(!xmqttNegDisjoint(O,N,L.Data,L.Count*sizeof(*L.Data)))return false;
    for(size_t K=0;K<L.Count;++K){const xmqttproperty* P=&L.Data[K];
        if(P->Type==XMQTT_PROPERTY_STRING && !xmqttNegView(O,N,P->Value.Text))return false;
        if(P->Type==XMQTT_PROPERTY_BINARY && !xmqttNegDisjoint(O,N,P->Value.Binary.Data,P->Value.Binary.Size))return false;
        if(P->Type==XMQTT_PROPERTY_STRING_PAIR && (!xmqttNegView(O,N,P->Value.Pair.Name) || !xmqttNegView(O,N,P->Value.Pair.Value)))return false;
    }return true;
}
/* Called only after the public codec has validated every descriptor/span. */
static bool xmqttNegPacketOutput(const void* O,size_t N,const xmqttpacket* P)
{
    if(!N)return true;
    if(!xmqttNegDisjoint(O,N,P,sizeof(*P)) || !xmqttNegPropertiesOutput(O,N,P->Properties))return false;
    if(P->Type==XMQTT_PACKET_CONNECT){const xmqttconnectpacket* C=&P->Body.Connect;
        if(!xmqttNegView(O,N,C->ClientId))return false;
        if(C->HasWill && (!xmqttNegPropertiesOutput(O,N,C->WillProperties) || !xmqttNegView(O,N,C->WillTopic) || !xmqttNegDisjoint(O,N,C->WillPayload.Data,C->WillPayload.Size)))return false;
        if(C->HasUsername && !xmqttNegView(O,N,C->Username))return false;
        if(C->HasPassword && !xmqttNegDisjoint(O,N,C->Password.Data,C->Password.Size))return false;
    }else if(P->Type==XMQTT_PACKET_PUBLISH){if(!xmqttNegView(O,N,P->Body.Publish.Topic) || !xmqttNegDisjoint(O,N,P->Body.Publish.Payload.Data,P->Body.Publish.Payload.Size))return false;
    }else if(P->Type==XMQTT_PACKET_SUBSCRIBE || P->Type==XMQTT_PACKET_UNSUBSCRIBE){
        if(!xmqttNegDisjoint(O,N,P->Body.Topics.Data,P->Body.Topics.Count*sizeof(*P->Body.Topics.Data)))return false;
        for(size_t K=0;K<P->Body.Topics.Count;++K)if(!xmqttNegView(O,N,P->Body.Topics.Data[K].Filter))return false;
    }else if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK){if(!xmqttNegDisjoint(O,N,P->Body.Reasons.Data,P->Body.Reasons.Size))return false;}
    return true;
}
static xmqttnegotiationstatus xmqttNegArgument(void)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_NEGOTIATION_ARGUMENT,"invalid MQTT negotiation input/output");return XMQTT_NEGOTIATION_LOCAL_ERROR;}
static xmqttnegotiationstatus xmqttNegFail(xmqttnegotiationstatus S,xmqttnegotiationerror E,xmqttpropertyid P,xmqttnegotiationissue* I)
{
    if(I)*I=(xmqttnegotiationissue){S,E,P};
    xerrkind Kind=S==XMQTT_NEGOTIATION_RESTRICTED?XERR_RANGE:S==XMQTT_NEGOTIATION_PROTOCOL_ERROR?XERR_PROTOCOL:XERR_STATE;
    xrtSetErrorInfo(Kind,"xrt.mqtt",(int32)E,"MQTT negotiation constraint failed");return S;
}
static bool xmqttNegSize(const xmqttpacket* P,xmqttversion V,xmqttdirection D,const xmqttcodecconfig* C,size_t* N)
{return V==XMQTT_VERSION_5?xmqttPacketSize5(P,D,C,N):xmqttPacketSize311(P,D,C,N);}
static bool xmqttNegEqual(xstrview A,xstrview B)
{return A.Size==B.Size && (!A.Size || !memcmp(A.Data,B.Data,A.Size));}
static uint32 xmqttNegPacketLimit(uint32 V){return V>XMQTT_PACKET_MAX_BYTES?XMQTT_PACKET_MAX_BYTES:V;}
XMQTT_API bool xmqttNegotiationConfigInit(xmqttnegotiationconfig* C)
{
    if(!C || !xmqttNegSpan(C,sizeof(*C))){xmqttNegArgument();return false;}
    *C=(xmqttnegotiationconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;xmqttCodecConfigInit(&C->Codec);return true;
}
XMQTT_API xmqttnegotiationstatus xmqttNegotiate(const xmqttpacket* Connect,const xmqttpacket* Connack,
    const xmqttnegotiationconfig* Config,xmqttnegotiated* Result,xmqttnegotiationissue* Issue)
{
    if(!Connect || !Connack || !Result || !xmqttNegSpan(Connect,sizeof(*Connect)) || !xmqttNegSpan(Connack,sizeof(*Connack)) ||
        !xmqttNegSpan(Result,sizeof(*Result)) || (Issue && !xmqttNegSpan(Issue,sizeof(*Issue))) || (Config && !xmqttNegSpan(Config,sizeof(*Config))) ||
        !xmqttNegDisjoint(Result,sizeof(*Result),Connect,sizeof(*Connect)) || !xmqttNegDisjoint(Result,sizeof(*Result),Connack,sizeof(*Connack)) ||
        !xmqttNegDisjoint(Result,sizeof(*Result),Config,Config?sizeof(*Config):0) || !xmqttNegDisjoint(Result,sizeof(*Result),Issue,Issue?sizeof(*Issue):0) ||
        !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Connect,sizeof(*Connect)) || !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Connack,sizeof(*Connack)) ||
        !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Config,Config?sizeof(*Config):0))return xmqttNegArgument();
    xmqttnegotiationconfig C;if(Config)C=*Config;else xmqttNegotiationConfigInit(&C);
    if(C.Size!=sizeof(C) || (C.Version!=XMQTT_VERSION_5 && C.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT || Connack->Type!=XMQTT_PACKET_CONNACK)return xmqttNegArgument();
    size_t ConnectBytes=0,ConnackBytes=0;
    if(!xmqttNegSize(Connect,C.Version,XMQTT_CLIENT_TO_SERVER,&C.Codec,&ConnectBytes) || !xmqttNegSize(Connack,C.Version,XMQTT_SERVER_TO_CLIENT,&C.Codec,&ConnackBytes))return XMQTT_NEGOTIATION_LOCAL_ERROR;
    if(!xmqttNegPacketOutput(Result,sizeof(*Result),Connect) || !xmqttNegPacketOutput(Result,sizeof(*Result),Connack) ||
        !xmqttNegPacketOutput(Issue,Issue?sizeof(*Issue):0,Connect) || !xmqttNegPacketOutput(Issue,Issue?sizeof(*Issue):0,Connack))return xmqttNegArgument();
    (void)ConnectBytes;xmqttnegotiated N={0};N.Size=sizeof(N);N.Version=C.Version;N.Reason=Connack->Reason;
    N.ClientId=Connect->Body.Connect.ClientId;N.KeepAlive=Connect->Body.Connect.KeepAlive;
    N.MaximumQos=2;N.RetainAvailable=N.WildcardAvailable=N.SharedAvailable=N.SubscriptionIdsAvailable=true;
    N.SendReceiveMaximum=N.ReceiveMaximum=UINT16_MAX;N.SendPacketBytes=N.ReceivePacketBytes=XMQTT_PACKET_MAX_BYTES;
    bool RequestedResponse=false;const xmqttproperty *Method=NULL,*AckMethod=NULL,*Assigned=NULL,*AuthData=NULL,*Response=NULL;
    if(C.Version==XMQTT_VERSION_5){
        for(size_t K=0;K<Connect->Properties.Count;++K){const xmqttproperty* P=&Connect->Properties.Data[K];switch(P->Id){
            case XMQTT_PROPERTY_SESSION_EXPIRY:N.RequestedSessionExpiry=N.SessionExpiry=P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE:N.ReceivePacketBytes=xmqttNegPacketLimit(P->Value.Number);break;
            case XMQTT_PROPERTY_RECEIVE_MAXIMUM:N.ReceiveMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM:N.ReceiveAliasMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_REQUEST_RESPONSE_INFO:RequestedResponse=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_AUTH_METHOD:Method=P;N.HasAuthMethod=true;N.AuthMethod=P->Value.Text;break;
            default:break;}}
        for(size_t K=0;K<Connack->Properties.Count;++K){const xmqttproperty* P=&Connack->Properties.Data[K];switch(P->Id){
            case XMQTT_PROPERTY_SESSION_EXPIRY:N.SessionExpiry=P->Value.Number;break;
            case XMQTT_PROPERTY_SERVER_KEEP_ALIVE:N.KeepAlive=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE:N.SendPacketBytes=xmqttNegPacketLimit(P->Value.Number);break;
            case XMQTT_PROPERTY_RECEIVE_MAXIMUM:N.SendReceiveMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM:N.SendAliasMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_QOS:N.MaximumQos=(uint8)P->Value.Number;break;
            case XMQTT_PROPERTY_RETAIN_AVAILABLE:N.RetainAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_WILDCARD_AVAILABLE:N.WildcardAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_SHARED_AVAILABLE:N.SharedAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE:N.SubscriptionIdsAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_ASSIGNED_CLIENT_ID:Assigned=P;break;
            case XMQTT_PROPERTY_AUTH_METHOD:AckMethod=P;break;
            case XMQTT_PROPERTY_AUTH_DATA:AuthData=P;break;
            case XMQTT_PROPERTY_RESPONSE_INFO:Response=P;N.ResponseInfo=P->Value.Text;break;
            case XMQTT_PROPERTY_SERVER_REFERENCE:N.ServerReference=P->Value.Text;break;
            default:break;}}
        if(ConnackBytes>N.ReceivePacketBytes)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_PACKET_SIZE,XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Issue);
        if(!Method && AckMethod)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(!Method && AuthData)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_DATA,XMQTT_PROPERTY_AUTH_DATA,Issue);
        if(!RequestedResponse && Response)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RESPONSE_INFO,XMQTT_PROPERTY_RESPONSE_INFO,Issue);
    }
    if(Connack->Reason){xmqttnegotiated R={0};R.Size=sizeof(R);R.Version=C.Version;R.Reason=Connack->Reason;R.ServerReference=N.ServerReference;*Result=R;if(Issue)*Issue=(xmqttnegotiationissue){0};return XMQTT_NEGOTIATION_REJECTED;}
    if(Connack->SessionPresent && Connect->Body.Connect.CleanStart)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
    if(C.Version==XMQTT_VERSION_5){
        if(Method && (!AckMethod || !xmqttNegEqual(Method->Value.Text,AckMethod->Value.Text)))return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(!N.ClientId.Size){
            if(!Assigned || !Assigned->Value.Text.Size)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID,XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,Issue);
            if(Connack->SessionPresent)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
            N.ClientId=Assigned->Value.Text;N.ServerAssignedId=true;
        }else if(Assigned && !xmqttNegEqual(N.ClientId,Assigned->Value.Text))return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID,XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,Issue);
        if(Connack->SessionPresent && !C.HasSessionState)return xmqttNegFail(XMQTT_NEGOTIATION_STATE_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
        if(Connect->Body.Connect.HasWill){
            if(Connect->Body.Connect.WillQos>N.MaximumQos)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_QOS,XMQTT_PROPERTY_MAXIMUM_QOS,Issue);
            if(Connect->Body.Connect.WillRetain && !N.RetainAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RETAIN,XMQTT_PROPERTY_RETAIN_AVAILABLE,Issue);
        }
    }
    N.Accepted=true;N.SessionPresent=Connack->SessionPresent;*Result=N;if(Issue)*Issue=(xmqttnegotiationissue){0};return XMQTT_NEGOTIATION_DONE;
}
XMQTT_API xmqttnegotiationstatus xmqttNegotiatedPacketCheck(const xmqttnegotiated* Peer,const xmqttpacket* Packet,
    const xmqttcodecconfig* Codec,size_t* Required,xmqttnegotiationissue* Issue)
{
    size_t RequiredBytes=Required?sizeof(*Required):0,IssueBytes=Issue?sizeof(*Issue):0;
    if(!Peer || !Packet || !xmqttNegSpan(Peer,sizeof(*Peer)) || !xmqttNegSpan(Packet,sizeof(*Packet)) || (Required && !xmqttNegSpan(Required,RequiredBytes)) ||
        (Issue && !xmqttNegSpan(Issue,IssueBytes)) || (Codec && !xmqttNegSpan(Codec,sizeof(*Codec))) || !xmqttNegDisjoint(Required,RequiredBytes,Issue,IssueBytes) ||
        !xmqttNegDisjoint(Required,RequiredBytes,Peer,sizeof(*Peer)) || !xmqttNegDisjoint(Issue,IssueBytes,Peer,sizeof(*Peer)) ||
        !xmqttNegDisjoint(Required,RequiredBytes,Codec,Codec?sizeof(*Codec):0) || !xmqttNegDisjoint(Issue,IssueBytes,Codec,Codec?sizeof(*Codec):0))return xmqttNegArgument();
    if(Peer->Size!=sizeof(*Peer) || !Peer->Accepted || (Peer->Version!=XMQTT_VERSION_5 && Peer->Version!=XMQTT_VERSION_311) ||
        !Peer->SendPacketBytes || Peer->SendPacketBytes>XMQTT_PACKET_MAX_BYTES || Peer->MaximumQos>2 || !Peer->SendReceiveMaximum || !Peer->ReceiveMaximum ||
        !xmqttNegSpan(Peer->ClientId.Data,Peer->ClientId.Size) || !xmqttNegSpan(Peer->AuthMethod.Data,Peer->AuthMethod.Size) ||
        !xmqttNegSpan(Peer->ServerReference.Data,Peer->ServerReference.Size) || !xmqttNegSpan(Peer->ResponseInfo.Data,Peer->ResponseInfo.Size))return xmqttNegArgument();
    const xstrview Views[]={Peer->ClientId,Peer->AuthMethod,Peer->ServerReference,Peer->ResponseInfo};
    for(size_t K=0;K<4;++K)if(!xmqttNegView(Required,RequiredBytes,Views[K]) || !xmqttNegView(Issue,IssueBytes,Views[K]))return xmqttNegArgument();
    size_t Bytes=0;if(!xmqttNegSize(Packet,Peer->Version,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes))return XMQTT_NEGOTIATION_LOCAL_ERROR;
    if(!xmqttNegPacketOutput(Required,RequiredBytes,Packet) || !xmqttNegPacketOutput(Issue,IssueBytes,Packet))return xmqttNegArgument();
    if(Packet->Type==XMQTT_PACKET_CONNECT)return xmqttNegFail(XMQTT_NEGOTIATION_STATE_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
    if(Bytes>Peer->SendPacketBytes)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_PACKET_SIZE,XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Issue);
    if(Peer->Version==XMQTT_VERSION_5){
        if(Packet->Type==XMQTT_PACKET_PUBLISH){
            if(((Packet->Flags&XMQTT_PUBLISH_QOS_MASK)>>1)>Peer->MaximumQos)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_QOS,XMQTT_PROPERTY_MAXIMUM_QOS,Issue);
            if((Packet->Flags&XMQTT_PUBLISH_RETAIN) && !Peer->RetainAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_RETAIN,XMQTT_PROPERTY_RETAIN_AVAILABLE,Issue);
        }
        const xmqttproperty* Method=NULL;
        for(size_t K=0;K<Packet->Properties.Count;++K){const xmqttproperty* P=&Packet->Properties.Data[K];
            if(P->Id==XMQTT_PROPERTY_TOPIC_ALIAS && P->Value.Number>Peer->SendAliasMaximum)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_TOPIC_ALIAS,P->Id,Issue);
            if(Packet->Type==XMQTT_PACKET_SUBSCRIBE && P->Id==XMQTT_PROPERTY_SUBSCRIPTION_ID && !Peer->SubscriptionIdsAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SUBSCRIPTION_ID,P->Id,Issue);
            if(Packet->Type==XMQTT_PACKET_DISCONNECT && P->Id==XMQTT_PROPERTY_SESSION_EXPIRY && !Peer->RequestedSessionExpiry && P->Value.Number)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SESSION_EXPIRY,P->Id,Issue);
            if(P->Id==XMQTT_PROPERTY_AUTH_METHOD)Method=P;
        }
        if(Packet->Type==XMQTT_PACKET_AUTH && (!Peer->HasAuthMethod || !Method || !xmqttNegEqual(Peer->AuthMethod,Method->Value.Text)))return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(Packet->Type==XMQTT_PACKET_SUBSCRIBE){
            for(size_t K=0;K<Packet->Body.Topics.Count;++K){xstrview F=Packet->Body.Topics.Data[K].Filter;
                if(!Peer->SharedAvailable && F.Size>=7 && !memcmp(F.Data,"$share/",7))return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SHARED,XMQTT_PROPERTY_SHARED_AVAILABLE,Issue);
                if(!Peer->WildcardAvailable)for(size_t I=0;I<F.Size;++I)if(F.Data[I]=='+' || F.Data[I]=='#')return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_WILDCARD,XMQTT_PROPERTY_WILDCARD_AVAILABLE,Issue);
            }
        }
    }
    if(Required)*Required=Bytes;
    if(Issue)*Issue=(xmqttnegotiationissue){0};
    return XMQTT_NEGOTIATION_DONE;
}
#endif
