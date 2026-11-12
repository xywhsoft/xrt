#ifndef XREDIS_TEST_CLUSTER_COMMAND_PEER_H
#define XREDIS_TEST_CLUSTER_COMMAND_PEER_H
#include "cluster_client_peer.h"
#include "command_keys_fixture.h"
/* The literal peer returns the requested GET key as its value, making copied
 * command bytes observable. Its metadata still uses the actual GET key rule. */
static bool qpCommand(redispeer* P,xredisreply* Reply,ptr Data)
{
    if(rfCommand(P,Reply,Data))return true;
    xredisreplyvalue Root,Name,Key;redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"command fixture name");
    if(!redisPeerName(Name,"GET"))return false;
    redisPeerRequire(xredisReplyGet(Reply,Name.Next,&Key),"command fixture key");char Head[40];int N=snprintf(Head,sizeof(Head),"$%zu\r\n",Key.Data.Size);redisPeerSend(P,Head,(size_t)N);if(Key.Data.Size)redisPeerSend(P,Key.Data.Data,Key.Data.Size);redisPeerSend(P,"\r\n",2);return true;
}
static void qpInstall(xnetworker* Worker,ptr Data)
{(void)Worker;rfnode* N=Data;N->Owner->Base->Peers[N->Index].Hook=(redispeerhook){qpCommand,N};xrtAtomic32Store(&N->Installed,1,XMEMORY_RELEASE);}
static inline rffixture* qpStart(void)
{
    rffixture* F=rfStart(3);for(size_t I=0;I<3;++I){xrtAtomic32Store(&F->Nodes[I].Installed,0,XMEMORY_RELEASE);redisPeerRequire(xrtNetEnginePost(F->Base->Peers[I].Engine,1,qpInstall,&F->Nodes[I]),"command fixture hook");redisPeerWait(&F->Nodes[I].Installed,1);}return F;
}
static inline xrediscommandcatalog* qpCatalogueWire(const char* Wire,const xrediscommandkeylimits* Limits)
{
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;size_t Used=0;redisPeerRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,strlen(Wire)},&Used,&R)==XREDIS_REPLY_READY,"command catalogue reply");xredisReplyReaderDestroy(Reader);xrediscommandcatalog* C=NULL;redisPeerRequire(xredisCommandCatalogCreate(R,Limits,&C)==XREDIS_KEYS_OK,"command catalogue create");xredisReplyRelease(R);return C;
}
static inline xrediscommandcatalog* qpCatalogue(unsigned Version,const xrediscommandkeylimits* Limits)
{return qpCatalogueWire(Version?CK_METADATA3:CK_METADATA2,Limits);}
static inline xfuture* qpGet(xredisclusterclient* C,const xrediscommandcatalog* Catalogue,int64 Timeout)
{xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};xfuture* F=NULL;redisPeerRequire(xredisClusterClientCallCommand(C,Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,Timeout,&F)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"metadata command admission");return F;}
static inline void qpValue(xfuture* F)
{const xredisclusterclientresult* R=cfWait(F);xredisreplyvalue V;redisPeerRequire(R->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && R->RouteObserved && R->Route.Query.Status==XREDIS_REQUEST_OK && xredisReplyGet(R->Route.Query.Reply,0,&V) && V.Data.Size==13 && !memcmp(V.Data.Data,"owned\0{22179}",13),"metadata command binary result");}
#endif
