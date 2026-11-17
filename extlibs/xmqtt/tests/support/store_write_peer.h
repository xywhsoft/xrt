#ifndef XMQTT_TEST_STORE_WRITE_PEER_H
#define XMQTT_TEST_STORE_WRITE_PEER_H
#include "store_state_peer.h"
#include "journal_worker_peer.h"
#include <xmqtt/store_write.h>
static inline xmqttstorewriteinfo swInfo(xmqttstorewrite* W)
{xmqttstorewriteinfo I;testRequire(xmqttStoreWriteInfo(W,&I),"writer snapshot");return I;}
static inline xfuture* swAppend(xmqttstorewrite* W,xbytesview Data)
{
    double End=xrtTimer()+10;xfuture* F=NULL;xmqttjournaladmission A;
    do{testRequire(xrtTimer()<End,"finite native scheduling backpressure");A=xmqttStoreWriteAppendAsync(W,Data,(int64)((End-xrtTimer())*1000),&F);if(A==XMQTT_JWORK_OVERLOADED)xrtSleep(1);}while(A==XMQTT_JWORK_OVERLOADED);
    testRequire(A==XMQTT_JWORK_ACCEPTED&&F,"semantic immutable FIFO admission");return F;
}
static inline xfuture* swOps(xmqttstorewrite* W,const xmqttstoredeltaconfig* C,const xmqttstoredelta* Ops,size_t Count)
{unsigned char B[SD_BUFFER];xmqttproperty P[4];size_t N=0;sdStatus(xmqttStoreDeltaWrite(C,Ops,Count,P,4,B,sizeof(B),&N),XMQTT_STORE_DELTA_DONE,"encode valid transaction representation");xfuture* F=swAppend(W,(xbytesview){B,N});memset(B,0xcc,N);return F;}
#endif
