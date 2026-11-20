#define XMQTT_MODULE_XMQTT_STORE_CHECKPOINT
#define XMQTT_MODULE_XMQTT_JOURNAL_SLOT
#define XMQTT_MODULE_XMQTT_STORE_WRITE
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_VFS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include <string.h>
static unsigned compactFault,compactHit,compactSelectorWrites;

static ptr compactAlloc(size_t Bytes)
{
    if(compactFault==1&&Bytes==84){
        ++compactHit;
        xrtSetErrorKind(XERR_MEMORY);
        return NULL;
    }
    return xrtMalloc(Bytes);
}
static bool compactWrite(xfile File,uint64 Offset,const void* Data,size_t Bytes,size_t* Written)
{
    if(compactFault==2&&Bytes>=8&&!memcmp(Data,"XMQCROW",8)){
        ++compactHit;
        xrtSetErrorKind(XERR_IO);
        return false;
    }
    if(compactFault>=3&&Bytes==4096&&!memcmp(Data,"XMQSEL1",8)){
        ++compactSelectorWrites;
        if(compactSelectorWrites==compactFault-2){
            ++compactHit;
            if(compactFault==4&&!xrtFlush(File))return false;
            xrtSetErrorKind(XERR_IO);
            return false;
        }
    }
    return xrtWriteAtFull(File,Offset,Data,Bytes,Written);
}
#define XMQTT_IMPLEMENTATION
#define xrtMalloc compactAlloc
#define xrtWriteAtFull compactWrite
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtMalloc
#undef xrtWriteAtFull
#define TEST_COMPACT_FAULTS
#include "../test_store_compact.c"
