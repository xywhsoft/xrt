#include <xmqtt.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char** argv)
{
    uint64 Count=argc>1?strtoull(argv[1],NULL,10):1000000,Sum=0;
    unsigned Version=argc>2?(unsigned)strtoul(argv[2],NULL,10):5;if(!Count || Count>1000000000 || (Version!=4 && Version!=5) || argc>3)return 1;
    xmqttpacket C,A,P;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    C.Body.Connect.CleanStart=true;C.Body.Connect.ClientId=XRT_STR_LITERAL("bench");C.Body.Connect.KeepAlive=60;P.Body.Publish.Topic=XRT_STR_LITERAL("bench/data");
    const unsigned char Payload[]={42};P.Body.Publish.Payload=(xbytesview){Payload,1};xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=(xmqttversion)Version;
    double Start=xrtTimer();for(uint64 I=0;I<Count;++I){xmqttnegotiated N;size_t Bytes=0;
        C.Body.Connect.KeepAlive=(uint16)(I%65536u);
        if(xmqttNegotiate(&C,&A,&K,&N,NULL)!=XMQTT_NEGOTIATION_DONE || xmqttNegotiatedPacketCheck(&N,&P,NULL,&Bytes,NULL)!=XMQTT_NEGOTIATION_DONE)return 2;
        Sum+=N.KeepAlive+N.SendReceiveMaximum+Bytes;}
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))return 3;
    printf("negotiation_cycles_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Sum);return 0;
}
