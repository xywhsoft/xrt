#include <xmqtt/client_ws_uri.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
int main(int Count,char** Args)
{
    unsigned N=Count>1?(unsigned)strtoul(Args[1],NULL,10):1000000;if(!N||N>10000000||Count>2)return 2;uint64 Sum=0;double Start=xrtTimer();
    for(unsigned I=0;I<N;++I){xmqttwsuri U={.Size=sizeof(U)};if(!xmqttWsUriParse(XRT_STR_LITERAL("wss://ex%61mple.com:443/mqtt%2Fa?x=%00"),&U)||!U.Secure||U.Port!=443||U.RouteBytes!=11||U.TargetBytes!=15)return 1;Sum+=U.Port+U.RouteBytes+U.TargetBytes;}
    double Elapsed=xrtTimer()-Start;if(Sum!=(uint64)N*469u)return 1;printf("parses_per_sec: %.3f\nchecksum: %" PRIu64 "\n",N/Elapsed,Sum);return 0;
}
