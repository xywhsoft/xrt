#define XRT_MODULE_WEBSOCKET_STREAM_REF
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

static xnetresult testOwnedBytesText(xwsstream* s,xstrview text)
{
    size_t required=0;
    if(!xrtWsStreamSendSize(s,text.Size,&required)||required> xrtWsStreamWriteLimit(s))return XNET_RESULT_ERROR;
    xnetbytes* bytes=xrtNetBytesCopy(text.Data,text.Size);
    if(!bytes)return XNET_RESULT_ERROR;
    xnetresult result=xrtWsStreamSendBytes(s,XWS_OPCODE_TEXT,bytes,0,text.Size);
    xrtNetBytesDestroy(bytes);return result;
}
static xnetresult testOwnedBytesTextRef(xwsstream* s,const xnetref* ref)
{
    xnetresult result=testOwnedBytesText(s,(xstrview){(const char*)ref->Data,ref->Size});
    if(result==XNET_RESULT_OK)ref->Release(ref->Context,ref->Data,ref->Size);
    return result;
}
#define xrtWsStreamText testOwnedBytesText
#define xrtWsStreamTextRef testOwnedBytesTextRef
#define TEST_SINGLE_WS_BUFFERED 1
#define TEST_SINGLE_WS_OWNED 1
#include "test_single_websocket_stream.c"
