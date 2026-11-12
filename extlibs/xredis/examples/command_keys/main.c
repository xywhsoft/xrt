#include <xredis/command_keys.h>
#include <stdio.h>
int main(void)
{
    /* A bounded COMMAND INFO GET reply; applications obtain real metadata using
     * an authorized ordinary client, then publish a retained catalogue snapshot. */
    static const char Wire[]="*1\r\n*6\r\n+get\r\n:2\r\n*1\r\n+readonly\r\n:1\r\n:1\r\n:1\r\n";
    xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;xrediscommandcatalog* Catalog=NULL;size_t Used=0;
    if(!xredisReplyReaderCreate(NULL,&Reader))return 1;
    xredisreplystatus Parsed=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,sizeof(Wire)-1},&Used,&Reply);xredisReplyReaderDestroy(Reader);
    if(Parsed!=XREDIS_REPLY_READY)return 2;
    xrediskeysstatus Built=xredisCommandCatalogCreate(Reply,NULL,&Catalog);xredisReplyRelease(Reply);if(Built!=XREDIS_KEYS_OK)return 3;
    xbytesview Args[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("customer:{123456789}\0order")};xrediscommandkeyresult Result={0};Result.Size=sizeof(Result);
    xrediskeysstatus Status=xredisCommandCatalogEvaluate(Catalog,Args,2,NULL,0,&Result);
    if(Status==XREDIS_KEYS_OK)printf("GET uses %zu key(s), slot %u\n",Result.Keys,(unsigned)Result.Slot);
    xredisCommandCatalogRelease(Catalog);return Status==XREDIS_KEYS_OK && Result.Slot==12739?0:4;
}
