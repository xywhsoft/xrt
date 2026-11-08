#include <xmqtt.h>
#include <stdio.h>
int main(void)
{
    /* Independent literal peers; no network, credentials, files or cleanup. */
    const unsigned char Connect[]={0x10,0x11,0,4,'M','Q','T','T',5,2,0,60,0,0,4,'d','e','m','o'};
    const unsigned char Connack[]={0x20,0x13,0,0,0x10,0x13,0,30,0x21,0,4,0x24,1,0x27,0,0,4,0,0x22,0,8};
    xmqttproperty Properties[5];xmqttcodecbuffers Buffers={Properties,5,NULL,0};xmqttpacket C,A;xmqttnegotiated N;
    if(xmqttPacketRead5((xbytesview){Connect,sizeof(Connect)},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&C,NULL)!=XMQTT_CODEC_DONE ||
       xmqttPacketRead5((xbytesview){Connack,sizeof(Connack)},XMQTT_SERVER_TO_CLIENT,NULL,&Buffers,&A,NULL)!=XMQTT_CODEC_DONE ||
       xmqttNegotiate(&C,&A,NULL,&N,NULL)!=XMQTT_NEGOTIATION_DONE)return 1;
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Identifier=1;P.Body.Publish.Topic=XRT_STR_LITERAL("sensor");size_t Bytes=0;
    if(xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,NULL)!=XMQTT_NEGOTIATION_DONE)return 2;
    printf("keep-alive=%u max-qos=%u receive-max=%u alias-max=%u packet-max=%u publish-bytes=%zu\n",N.KeepAlive,N.MaximumQos,N.SendReceiveMaximum,N.SendAliasMaximum,N.SendPacketBytes,Bytes);
    /* No automatic downgrade: this QoS-2 request violates the broker's QoS-1 cap. */
    P.Flags=4;xmqttnegotiationissue Issue;
    if(xmqttNegotiatedPacketCheck(&N,&P,NULL,NULL,&Issue)!=XMQTT_NEGOTIATION_RESTRICTED || Issue.Error!=XMQTT_NEGOTIATION_QOS)return 3;
    xrtClearError();return 0;
}
