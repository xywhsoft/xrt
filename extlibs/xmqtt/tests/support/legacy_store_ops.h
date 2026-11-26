#ifndef XMQTT_TEST_LEGACY_STORE_OPS_H
#define XMQTT_TEST_LEGACY_STORE_OPS_H
#include "../test.h"
#include <xmqtt/store_write.h>

static inline xmqttjournaladmission swLegacyAck(xmqttstorewrite* Writer,
    const xmqttstoredeltaconfig* Delta,uint64 Epoch,uint64 Key,int64 Timeout,xfuture** Future)
{
    xmqttstoredelta Op={.Type=XMQTT_STORE_INBOX_ACK,.Epoch=Epoch,.Key=Key};
    unsigned char Batch[128];
    size_t Bytes=0;
    testRequire(xmqttStoreDeltaWrite(Delta,&Op,1,NULL,0,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE,
        "encode legacy Inbox ACK fixture");
    return xmqttStoreWriteAppendAsync(Writer,(xbytesview){Batch,Bytes},Timeout,Future);
}
#endif
