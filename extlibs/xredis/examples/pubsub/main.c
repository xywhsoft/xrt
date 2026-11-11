#include <xredis/pubsub.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    xbytesview Names[]={XRT_BYTES_LITERAL("binary\0channel"),{NULL,0}};unsigned char Command[128];size_t Written=0;
    if(!xredisPubSubCommandWrite(Command,sizeof(Command),XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,2,&Written))return 1;
    static const char Wire[]="|1\r\n+source\r\n+example\r\n>4\r\n+pmessage\r\n+p*\r\n$3\r\np\0x\r\n$4\r\nv\0\r\n\r\n";
    xredisreplyreader* Reader=NULL;if(!xredisReplyReaderCreate(NULL,&Reader))return 2;xredisreply* Reply=NULL;size_t Offset=0;
    while(Offset<sizeof(Wire)-1){size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire+Offset,1},&Used,&Reply);if(S==XREDIS_REPLY_ERROR)return 3;Offset+=Used;}
    xredisReplyReaderDestroy(Reader);xredispubsubevent E;
    if(!Reply || xredisPubSubEventRead(Reply,&E)!=XREDIS_PUBSUB_EVENT || E.Kind!=XREDIS_PUBSUB_PMESSAGE || E.Channel.Size!=3 || E.Payload.Size!=4 || memcmp(E.Payload.Data,"v\0\r\n",4))return 4;
    printf("subscribe_bytes=%zu pattern_bytes=%zu channel_bytes=%zu payload_bytes=%zu\n",Written,E.Pattern.Size,E.Channel.Size,E.Payload.Size);
    xredisReplyRelease(Reply);return 0;
}
