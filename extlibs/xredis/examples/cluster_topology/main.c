#include <xredis.h>
#include <stdio.h>
int main(void)
{
    static const char Wire[]="*1\r\n*3\r\n:0\r\n:16383\r\n*2\r\n+127.0.0.1\r\n:6379\r\n";
    xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;xredisclustertopology* Map=NULL;size_t Used=0;uint16 Slot;
    if(!xredisReplyReaderCreate(NULL,&Reader))return 1;
    if(xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,sizeof(Wire)-1},&Used,&Reply)!=XREDIS_REPLY_READY)return 2;
    xredisclusterlimits Limits;if(!xredisClusterLimitsInit(&Limits))return 3;Limits.RequireFullCoverage=true;
    if(xredisClusterTopologyCreate(Reply,XREDIS_CLUSTER_LEGACY_SLOTS,&Limits,&Map)!=XREDIS_CLUSTER_VALUE)return 3;
    xredisReplyRelease(Reply);
    static const char Redirect[]="-ASK 8000 :6380\r\n";
    if(xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Redirect,sizeof(Redirect)-1},&Used,&Reply)!=XREDIS_REPLY_READY)return 3;
    xredisclusterredirect Target;if(xredisClusterRedirectRead(Reply,1024,&Target)!=XREDIS_CLUSTER_VALUE || Target.Kind!=XREDIS_CLUSTER_ASK || Target.Endpoint!=XREDIS_CLUSTER_ENDPOINT_SOURCE)return 3;
    xredisReplyReaderDestroy(Reader);xredisReplyRelease(Reply);
    xredisclustertopologyinfo Info={0};Info.Size=sizeof(Info);xredisclusternode First;xredisclusterrange Range;
    if(!xredisClusterTopologyInfo(Map,&Info) || !xredisClusterTopologyNode(Map,0,&First) || !xredisClusterTopologyRange(Map,0,&Range) || Info.AssignedSlots!=16384 || Range.Last!=16383 || First.Port!=6379 || !xredisClusterTopologyRetain(Map))return 3;
    xredisClusterTopologyRelease(Map);
    if(!xredisClusterKeySlot(XRT_BYTES_LITERAL("orders:{42}"),&Slot))return 4;
    xredisclusternode Node;if(xredisClusterTopologyLookup(Map,Slot,&Node)!=XREDIS_CLUSTER_VALUE)return 5;
    printf("orders:{42}: slot=%u endpoint=%.*s:%u\n",(unsigned)Slot,(int)Node.Host.Size,(cstr)Node.Host.Data,(unsigned)Node.Port);
    xredisClusterTopologyRelease(Map);return 0;
}
