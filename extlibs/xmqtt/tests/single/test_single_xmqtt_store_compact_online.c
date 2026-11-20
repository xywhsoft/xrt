#define XMQTT_MODULE_XMQTT_STORE_COMPACT_ONLINE
#define XMQTT_MODULE_XMQTT_CLIENT_STORE
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_LOCK
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include <string.h>

static xatomic32 onlineHook,onlineEntered,onlineReleased,onlineWrites;
static bool onlineWrite(xfile File,uint64 Offset,const void* Data,size_t Bytes,size_t* Written)
{
    uint32 Mode=xrtAtomic32Load(&onlineHook,XMEMORY_ACQUIRE);
    if((Mode==1||Mode==2)&&Bytes>=8&&!memcmp(Data,"XMQCROW",8)){
        if(Mode==2){xrtSetErrorKind(XERR_IO);return false;}
        xrtAtomic32Store(&onlineEntered,1,XMEMORY_RELEASE);
        double End=xrtTimer()+10;
        while(!xrtAtomic32Load(&onlineReleased,XMEMORY_ACQUIRE)){
            if(xrtTimer()>=End){xrtSetErrorKind(XERR_TIMEOUT);return false;}
            xrtSleep(1);
        }
    }
    if(Mode==3&&Bytes==4096&&!memcmp(Data,"XMQSEL1",8)&&
       xrtAtomic32FetchAdd(&onlineWrites,1,XMEMORY_ACQ_REL)==1){
        if(!xrtFlush(File))return false;
        xrtSetErrorKind(XERR_IO);
        return false;
    }
    return xrtWriteAtFull(File,Offset,Data,Bytes,Written);
}
#define XMQTT_IMPLEMENTATION
#define xrtWriteAtFull onlineWrite
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtWriteAtFull
#define TEST_ONLINE_FAULTS
#include "../test_store_compact_online.c"
