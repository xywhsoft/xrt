/* Reads independent, bounded real-server wire records. No network runtime. */
#include <xredis/pubsub.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void pubsubRequire(bool OK,const char* What)
{if(!OK){fprintf(stderr,"[FAIL] pubsub corpus: %s\n",What);exit(1);}}
static uint32 pubsubU32(const unsigned char* P)
{return (uint32)P[0]|(uint32)P[1]<<8|(uint32)P[2]<<16|(uint32)P[3]<<24;}
static uint64 pubsubU64(const unsigned char* P)
{return (uint64)pubsubU32(P)|(uint64)pubsubU32(P+4)<<32;}
static bool pubsubSame(xbytesview A,xbytesview B)
{return A.Size==B.Size && (!A.Size || !memcmp(A.Data,B.Data,A.Size));}
int main(int Argc,char** Argv)
{
    if(Argc!=3)return 2;
    size_t Fragment=(size_t)strtoull(Argv[2],NULL,10);if(!Fragment)return 2;
    FILE* Input=fopen(Argv[1],"rb");pubsubRequire(Input!=NULL,"open corpus");unsigned char Magic[4];pubsubRequire(fread(Magic,1,4,Input)==4 && !memcmp(Magic,"XPS1",4),"corpus version");
    xredisreplyreader* Reader=NULL;pubsubRequire(xredisReplyReaderCreate(NULL,&Reader),"bounded reader");size_t Records=0,Commands=0,Events=0;
    unsigned char Header[28];
    for(;;){size_t Got=fread(Header,1,sizeof(Header),Input);if(!Got)break;pubsubRequire(Got==sizeof(Header),"record header");
        size_t Bytes=pubsubU32(Header),C=pubsubU32(Header+16),P=pubsubU32(Header+20),V=pubsubU32(Header+24);
        pubsubRequire(Bytes && Bytes<=16u*1024u*1024u && C<=16u*1024u*1024u && P<=16u*1024u*1024u && V<=16u*1024u*1024u,"bounded record fields");
        size_t Meta=C+P+V;pubsubRequire(Meta<=16u*1024u*1024u,"bounded corpus record");
        unsigned char* Expected=malloc(Meta?Meta:1);unsigned char* Wire=malloc(Bytes);pubsubRequire(Expected && Wire,"caller record buffers");
        pubsubRequire(fread(Expected,1,Meta,Input)==Meta && fread(Wire,1,Bytes,Input)==Bytes,"complete record");
        xredisreply* R=NULL;size_t Offset=0;
        while(Offset<Bytes){size_t N=Bytes-Offset;if(N>Fragment)N=Fragment;size_t Used=0;
            xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Wire+Offset,N},&Used,&R);pubsubRequire(S!=XREDIS_REPLY_ERROR && Used<=N && (Used || S==XREDIS_REPLY_YIELD),"feed frame");Offset+=Used;
            if(S==XREDIS_REPLY_READY){pubsubRequire(Offset==Bytes,"one record per frame");break;}
        }
        pubsubRequire(R!=NULL,"complete semantic frame");
        if(Header[4]>=0x80 && Header[4]<=0x86){
            xredisreplyinfo I={0};I.Size=sizeof(I);pubsubRequire(xredisReplyInfo(R,&I),"command root info");xredisreplyvalue Root,Name;
            pubsubRequire(xredisReplyGet(R,I.Root,&Root) && Root.Type==XREDIS_REPLY_ARRAY && Root.ChildCount && Root.ChildCount<=XREDIS_PUBSUB_NAMES_MAX+1,"command argument count");
            xbytesview Names[XREDIS_PUBSUB_NAMES_MAX];xredisReplyGet(R,Root.FirstChild,&Name);size_t Node=Name.Next;
            for(size_t N=1;N<Root.ChildCount;++N){xredisreplyvalue A;pubsubRequire(xredisReplyGet(R,Node,&A),"command name view");Names[N-1]=A.Data;Node=A.Next;}
            unsigned char* Encoded=malloc(Bytes);pubsubRequire(Encoded!=NULL,"caller command buffer");size_t Need=0,Written=0;
            pubsubRequire(xredisPubSubCommandSize((xredispubsubcommand)(Header[4]-0x80),Names,Root.ChildCount-1,&Need) && Need==Bytes && xredisPubSubCommandWrite(Encoded,Bytes,(xredispubsubcommand)(Header[4]-0x80),Names,Root.ChildCount-1,&Written) && Written==Bytes && !memcmp(Encoded,Wire,Bytes),"typed encoder matches independent request bytes");free(Encoded);++Commands;
        }else{
            free(Wire);Wire=NULL;xredispubsubevent E;memset(&E,0x45,sizeof(E));xredispubsubevent Saved=E;
            xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();xredispubsubstatus S;
            xbytesview Payload={Expected+C+P,V};
            if(Header[6])S=xredisPubSubPongRead(R,Header[6]==1?NULL:&Payload,&E);else S=xredisPubSubEventRead(R,&E);
            pubsubRequire(xrtGetError()==Error,"classification preserves ambient error");
            if(Header[4]==255)pubsubRequire(S==XREDIS_PUBSUB_OTHER && !memcmp(&E,&Saved,sizeof(E)),"unrelated reply preserved");
            else {
                pubsubRequire(S==XREDIS_PUBSUB_EVENT && (unsigned)E.Kind==Header[4] && E.Flags==Header[5] && E.Subscriptions==pubsubU64(Header+8),"exact semantic kind/flags/count");
                pubsubRequire(pubsubSame(E.Channel,(xbytesview){Expected,C}) && pubsubSame(E.Pattern,(xbytesview){Expected+C,P}) && pubsubSame(E.Payload,Payload),"exact binary channel/pattern/payload");
                if(Header[7])pubsubRequire(E.Envelope==(Header[7]==1?XREDIS_REPLY_ARRAY:Header[7]==2?XREDIS_REPLY_PUSH:Header[7]==3?XREDIS_REPLY_SIMPLE_STRING:XREDIS_REPLY_BULK_STRING),"actual envelope preserved");
                ++Events;
            }
            xrtClearError();
        }
        xredisReplyRelease(R);free(Wire);free(Expected);++Records;
    }
    pubsubRequire(!ferror(Input) && xredisReplyReaderFinish(Reader),"clean complete corpus EOF");fclose(Input);xredisReplyReaderDestroy(Reader);
    printf("[PASS] independent Pub/Sub corpus: records=%zu commands=%zu events=%zu fragment=%zu\n",Records,Commands,Events,Fragment);return 0;
}
