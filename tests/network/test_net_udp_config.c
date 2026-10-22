/* Offline only: no socket, Engine, DNS or network traffic. */
#include "xrt/udp.h"
#include "xrt/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
static unsigned rejected;
static void invalid(const xnetudpconfig* config)
{
    xrtClearError();CHECK(!xrtNetUdpConfigValid(config));
    CHECK(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();++rejected;
}
#define REJECT(Field,Value) do{xnetudpconfig value=defaults;value.Field=(Value);invalid(&value);}while(0)
int main(void)
{
    xnetudpconfig defaults;xrtNetUdpConfigInit(&defaults);xnetudpconfig original=defaults;
    xrtClearError();CHECK(xrtNetUdpConfigValid(&defaults)&&!xrtGetError()&&!memcmp(&defaults,&original,sizeof(defaults)));
    invalid(NULL);
    REJECT(ReceiveSize,0);REJECT(ReceiveSize,65536);REJECT(ReceiveConcurrency,0);REJECT(ReceiveConcurrency,65);
    REJECT(ReceiveBatch,0);REJECT(ReceiveBatch,1025);REJECT(ReceiveMeta,UINT32_MAX);
    REJECT(Overflow,-1);REJECT(Overflow,XNET_UDP_DROP_ERROR+1);
    REJECT(Truncation,-1);REJECT(Truncation,XNET_UDP_TRUNCATE_ERROR+1);
    REJECT(ErrorSize,0);REJECT(ErrorSize,65536);REJECT(ErrorOverflow,-1);REJECT(ErrorOverflow,XNET_UDP_DROP_ERROR+1);
    REJECT(SendLimit,0);REJECT(SendPacketLimit,0);REJECT(SendHighWater,0);
    REJECT(SendHighWater,defaults.SendLimit+1);REJECT(SendLowWater,defaults.SendHighWater+1);
    REJECT(SendConcurrency,0);REJECT(SendConcurrency,65);
    REJECT(ReceiveBuffer,-2);REJECT(SendBuffer,-2);REJECT(HopLimit,-2);REJECT(HopLimit,256);
    REJECT(TrafficClass,-2);REJECT(TrafficClass,256);REJECT(PathMtu,-1);REJECT(PathMtu,XNET_PMTU_PROBE+1);
    xnetudpconfig value=defaults;value.ExclusiveAddress=true;value.ReuseAddress=true;invalid(&value);
    value=defaults;value.ExclusiveAddress=true;value.ReusePort=true;invalid(&value);
    /* Queue zero has real native semantics and is not an invalid capacity.
     * Boundary checks remain platform independent: no socket is touched. */
    value=defaults;value.ReceiveSize=65535;value.ReceiveConcurrency=64;value.ReceiveBatch=1024;
    value.ReceiveMeta=XNET_DGRAM_META_DESTINATION|XNET_DGRAM_META_INTERFACE|XNET_DGRAM_META_HOP_LIMIT|
        XNET_DGRAM_META_TRAFFIC_CLASS|XNET_DGRAM_META_SEGMENT_SIZE;
    value.ReceiveQueueLimit=0;value.ReceiveQueueByteLimit=0;value.Overflow=XNET_UDP_DROP_ERROR;
    value.Truncation=XNET_UDP_TRUNCATE_ERROR;value.ErrorSize=65535;value.ErrorQueueLimit=0;
    value.ErrorQueueByteLimit=0;value.ErrorOverflow=XNET_UDP_DROP_OLDEST;value.SendHighWater=1;
    value.SendLowWater=0;value.SendLimit=1;value.SendPacketLimit=1;value.SendConcurrency=64;
    value.ReceiveBuffer=0;value.SendBuffer=0;value.HopLimit=0;value.TrafficClass=255;value.PathMtu=XNET_PMTU_PROBE;
    CHECK(xrtNetUdpConfigValid(&value)&&!xrtGetError());
    printf("UDP configuration: %u rejected boundaries, immutable valid snapshots, no network\n",rejected);return 0;
}
