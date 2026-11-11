/* Decode real/corpus wire bytes on stdin; no networking or business replay. */
#include <xredis/cluster_topology.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
static void ctHex(xbytesview V)
{putchar('"');for(size_t I=0;I<V.Size;++I)printf("%02x",(unsigned)V.Data[I]);putchar('"');}
static int ctKeys(void)
{
    unsigned char Length[4],Key[4096];bool First=true;putchar('[');
    for(;;){size_t N=fread(Length,1,4,stdin);if(!N)break;if(N!=4)return 2;size_t Bytes=(size_t)Length[0]+((size_t)Length[1]<<8)+((size_t)Length[2]<<16)+((size_t)Length[3]<<24);if(Bytes>sizeof(Key) || fread(Key,1,Bytes,stdin)!=Bytes)return 2;
        uint16 Slot;if(!xredisClusterKeySlot((xbytesview){Key,Bytes},&Slot))return 3;if(!First)putchar(',');First=false;printf("%u",(unsigned)Slot);}
    puts("]");return 0;
}
int main(int Argc,char** Argv)
{
#if defined(_WIN32)
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    if(Argc!=2)return 2;
    unsigned Mode=(unsigned)strtoul(Argv[1],NULL,10);if(!Mode)return ctKeys();if(Mode>3)return 2;
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;if(!xredisReplyReaderCreate(NULL,&Reader))return 3;
    unsigned char Input[4096];size_t Bytes;
    while((Bytes=fread(Input,1,sizeof(Input),stdin))){size_t Offset=0;while(Offset<Bytes){size_t Used=0;if(R)return 4;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Input+Offset,Bytes-Offset},&Used,&R);if(S==XREDIS_REPLY_ERROR)return 4;Offset+=Used;}}
    if(!R || !xredisReplyReaderFinish(Reader))return 4;
    xredisReplyReaderDestroy(Reader);
    if(Mode==3){xredisclusterredirect E;xredisclusterstatus S=xredisClusterRedirectRead(R,1024,&E);printf("{\"status\":%d",(int)S);if(S==XREDIS_CLUSTER_VALUE){printf(",\"kind\":%d,\"slot\":%u,\"port\":%u,\"endpoint\":%d,\"host\":",(int)E.Kind,(unsigned)E.Slot,(unsigned)E.Port,(int)E.Endpoint);ctHex(E.Host);}puts("}");xredisReplyRelease(R);return 0;}
    xredisclustertopology* T=NULL;xredisclusterstatus S=xredisClusterTopologyCreate(R,(xredisclusterformat)Mode,NULL,&T);xredisReplyRelease(R);
    printf("{\"status\":%d",(int)S);if(S!=XREDIS_CLUSTER_VALUE){puts("}");return 0;}
    xredisclustertopologyinfo I={0};I.Size=sizeof(I);if(!xredisClusterTopologyInfo(T,&I))return 5;
    printf(",\"shards\":%zu,\"assigned\":%zu,\"nodes\":[",I.Shards,I.AssignedSlots);
    for(size_t K=0;K<I.Nodes;++K){xredisclusternode N;if(!xredisClusterTopologyNode(T,K,&N))return 5;if(K)putchar(',');printf("{\"port\":%u,\"tls\":%u,\"endpoint\":%d,\"role\":%d,\"health\":%d,\"offset\":%" PRId64 ",\"host\":",(unsigned)N.Port,(unsigned)N.TlsPort,(int)N.Endpoint,(int)N.Role,(int)N.Health,N.ReplicationOffset);ctHex(N.Host);printf(",\"id\":");ctHex(N.Id);printf(",\"ip\":");ctHex(N.Ip);printf(",\"hostname\":");ctHex(N.Hostname);printf(",\"role_text\":");ctHex(N.RoleText);printf(",\"health_text\":");ctHex(N.HealthText);printf(",\"shard\":%zu",N.Shard);putchar('}');}
    printf("],\"ranges\":[");for(size_t K=0;K<I.Ranges;++K){xredisclusterrange N;if(!xredisClusterTopologyRange(T,K,&N))return 5;if(K)putchar(',');printf("[%u,%u,%zu,%zu]",(unsigned)N.First,(unsigned)N.Last,N.Master,N.Shard);}
    uint64 Checksum=0;for(uint16 Slot=0;Slot<16384;++Slot){xredisclusternode N;if(xredisClusterTopologyLookup(T,Slot,&N)==XREDIS_CLUSTER_VALUE)Checksum+=(uint64)(Slot+1)*((uint64)N.Port+((uint64)N.TlsPort<<16)+1);}
    printf("],\"checksum\":%" PRIu64 "}\n",Checksum);xredisClusterTopologyRelease(T);return 0;
}
