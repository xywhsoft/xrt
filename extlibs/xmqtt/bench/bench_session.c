#include <xmqtt.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):100000;unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5,Qos=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):1;size_t Bytes=Argc>4?(size_t)strtoull(Argv[4],NULL,10):64;
    if(!Count || Count>1000000000u || (Version!=5 && Version!=4) || !Qos || Qos>2 || !Bytes || Bytes>65536 || Argc>5)return 1;
    unsigned char* Data=malloc(Bytes);if(!Data)return 2;memset(Data,0xa5,Bytes);xmqttsession* S=NULL;xmqttsessionconfig C;xmqttSessionConfigInit(&C);C.Version=(xmqttversion)Version;C.ClientId=XRT_STR_LITERAL("bench-session");if(!xmqttSessionCreate(&C,&S)){free(Data);return 3;}
    xmqttpacket Connect,Connack,Publish,Ack;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=C.ClientId;Connect.Body.Connect.CleanStart=true;xmqttPacketInit(XMQTT_PACKET_CONNACK,&Connack);xmqttnegotiated N;if(xmqttSessionOpen(S,&Connect,&Connack,0,&N)!=XMQTT_SESSION_DONE)return 4;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=(uint8)(Qos<<1);Publish.Body.Publish.Topic=XRT_STR_LITERAL("bench/session");Publish.Body.Publish.Payload=(xbytesview){Data,Bytes};uint64 Sum=0;xmqttsessioninfo I;xmqttSessionInfo(S,&I);size_t Base=I.BaseBytes,Peak=0;
    double Start=xrtTimer();
    for(uint64 At=0;At<Count;++At){const xmqttsessionpacket* P=NULL;xmqttsessionframe F;if(xmqttSessionPublish(S,&Publish,&P)!=XMQTT_SESSION_DONE || xmqttSessionNext(S,&F)!=XMQTT_SESSION_DONE)return 5;
        uint16 Id=F.Identifier;Sum+=Id+F.Prefix.Size+F.Tail.Size;if(!At){xmqttSessionInfo(S,&I);Peak=I.LiveBytes;}
        if(xmqttSessionSent(S,&F)!=XMQTT_SESSION_DONE)return 6;
        xmqttSessionPacketRelease(F.Packet);xmqttSessionPacketRelease(P);
        xmqttPacketInit(Qos==1?XMQTT_PACKET_PUBACK:XMQTT_PACKET_PUBREC,&Ack);Ack.Identifier=Id;if(xmqttSessionAcknowledge(S,&Ack)!=XMQTT_SESSION_DONE)return 7;
        if(Qos==2){if(xmqttSessionNext(S,&F)!=XMQTT_SESSION_DONE || xmqttSessionSent(S,&F)!=XMQTT_SESSION_DONE)return 8;Sum+=F.Prefix.Size;xmqttSessionPacketRelease(F.Packet);xmqttPacketInit(XMQTT_PACKET_PUBCOMP,&Ack);Ack.Identifier=Id;if(xmqttSessionAcknowledge(S,&Ack)!=XMQTT_SESSION_DONE)return 9;}
    }
    double Elapsed=xrtTimer()-Start;bool Closed=xmqttSessionEnd(S,NULL,1)==XMQTT_SESSION_DONE && xmqttSessionDestroy(S);free(Data);if(!Closed || !(Elapsed>0))return 10;
    printf("session_cycles_per_sec: %.3f\nsession_base_bytes: %zu\nsession_single_packet_peak_bytes: %zu\nsession_config_bytes: %zu\nsession_info_bytes: %zu\nsession_frame_bytes: %zu\nsession_packet_info_bytes: %zu\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Base,Peak,sizeof(xmqttsessionconfig),sizeof(xmqttsessioninfo),sizeof(xmqttsessionframe),sizeof(xmqttsessionpacketinfo),Sum);return 0;
}
