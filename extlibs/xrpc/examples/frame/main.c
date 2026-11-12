#include <xrpc/frame.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    const unsigned char Input[]="Content-Length: 7\r\n\r\n{\"a\":1}Content-Length: 2\r\n\r\n{}";
    xrpcframereader* R=NULL;if(!xrpcFrameReaderCreate(NULL,&R))return 1;size_t At=0,Frames=0;
    while(At<sizeof(Input)-1){size_t N=sizeof(Input)-1-At;if(N>5)N=5;size_t Used=0;xrpcframeview V;xrpcframestatus S=xrpcFrameReaderFeed(R,(xbytesview){Input+At,N},&Used,&V);At+=Used;
        if(S==XRPC_FRAME_READY){const char* Expected=Frames?"{}":"{\"a\":1}";if(V.Body.Size!=strlen(Expected) || memcmp(V.Body.Data,Expected,V.Body.Size)){xrpcFrameReaderDestroy(R);return 2;}++Frames;}
        else if(S!=XRPC_FRAME_MORE || !Used){xrpcFrameReaderDestroy(R);return 3;}}
    bool OK=Frames==2 && xrpcFrameReaderEnd(R)==XRPC_FRAME_END;xrpcFrameReaderDestroy(R);if(!OK)return 4;puts("[PASS] incremental stream frame example: two coalesced frames, five-byte input chunks");return 0;
}
