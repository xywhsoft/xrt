/* Ordinary owned loopback with every native attach allocation prefix. */
#define XRT_MODULE_WEBSOCKET_STREAM_REF
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#include <stdio.h>

static xwsstream* attachWithFailures(xnetstream* tcp,xnetbuf* initial,
    const xwsstreamconfig* config,void* data,const xwsstreamownershipv1* policy)
{
    if(!config->SendLimit)return xrtWsStreamAttachOwnedV1(tcp,initial,config,data,policy);
    const size_t size=initial?initial->Size:0;
    for(size_t point=0;point<64;point++){
        assert(xrtMemDebugFailAfter(point));
        xwsstream* result=xrtWsStreamAttachOwnedV1(tcp,initial,config,data,policy);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!hit){
            assert(result && !xrtGetError());
            printf("native WS attach role=%d: %zu failed prefixes preserve source\n",(int)config->Role,point);
            return result;
        }
        assert(!result && xrtGetError());
        assert((initial?initial->Size:0)==size);
        assert(xrtNetStreamState(tcp)==XNET_STREAM_OPEN);
        xrtClearError();
    }
    assert(false);return NULL;
}
#define xrtWsStreamAttachOwnedV1 attachWithFailures
#define TEST_SINGLE_WS_BUFFERED 1
#define TEST_SINGLE_WS_OWNED 1
#include "test_single_websocket_stream.c"
