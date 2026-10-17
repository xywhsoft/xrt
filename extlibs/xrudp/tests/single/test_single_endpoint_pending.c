/* Exercise a retained real HELLO after local UDP admission stalls. Private
 * single-header state is used only to inject this otherwise nondeterministic
 * capacity condition; no socket or synthetic allocation failure is required. */
#define XRUDP_MODULE_RUDP_ENDPOINT
#define XRT_MODULE_MEMORY_DEBUG
#include "../../include/xrudp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#include <stdio.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
static xrudpresult observed;
static unsigned int closes;
static void peer_closed(xrudpendpoint* e, xrudppeer* p, xrudpresult reason, void* context)
{
	(void)e; (void)context;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) { observed=reason; ++closes; }
}
static int stalled(bool fail)
{
	unsigned char key[32]={7}, remoteKey[32]={9}, public[32], scope[16]={23};
	CHECK(xrtX25519Public(remoteKey,public)); xrudphandshakeconfig config={0};
	config.StaticPrivate=key; config.PeerPublic=public; config.Scope=scope; xrtRudpParametersDefault(&config.Parameters);
	xrudphandshake* h=NULL; CHECK(xrtRudpHandshakeCreate(&config,0,&h)==XRUDP_OK);
	size_t allocation=sizeof(xrudpendpoint)+sizeof(xrudppeer*);
	xrudpendpoint* e=(xrudpendpoint*)xrtMalloc(allocation); CHECK(e!=NULL); memset(e,0,allocation);
	e->Allocation=allocation; xrtAtomic32Init(&e->References,1); xrtAtomic32Init(&e->State,XRUDP_ENDPOINT_OPEN);
	xrtRudpEndpointConfigInit(&e->Config); e->Config.MaxPeers=1;
	size_t runtime=xrtRudpHandshakeAllocationSize();
	xrtAtomic64Init(&e->Reserved,allocation+sizeof(xrudppeer)+runtime); e->Events.PeerClose=peer_closed;
	xnetaddr address; CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,42000));
	xrudppeer* p=(xrudppeer*)xrtMalloc(sizeof(*p)); CHECK(p!=NULL); memset(p,0,sizeof(*p));
	xrtAtomic32Init(&p->References,2); xrtAtomic32Init(&p->State,XRUDP_PEER_NEGOTIATING);
	p->Endpoint=xrtRudpEndpointRef(e); p->Address=address; p->Handshake=h; p->RuntimeBytes=runtime;
	e->Peers=e->HalfOpen=1; e->Slots[0]=p;
	uint64_t due; CHECK(xrtRudpHandshakeNext(h,0,p->Pending,sizeof(p->Pending),&p->PendingSize,&due)==XRUDP_OK && p->PendingSize!=0);
	(void)xrtRudpHandshakeInfo(h,&p->Info); observed=XRUDP_OK; closes=0;
	if (fail) CHECK(xrtMemDebugFailAfter(0));
	/* This must terminate before touching UDP (the fake endpoint has none),
	 * even though PendingSize would normally skip HandshakeNext's deadline. */
	xrudp__peer_pump(p,p->Info.DeadlineUs);
	CHECK(observed==XRUDP_TIMEOUT && closes==1 && xrtRudpPeerState(p)==XRUDP_PEER_CLOSED);
	if (fail) { CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); }
	xrtRudpPeerDestroy(p); CHECK(xrtAtomic64Load(&e->Reserved,XMEMORY_ACQUIRE)==allocation);
	xrtRudpEndpointDestroy(e); return 0;
}
int main(void)
{
	CHECK(stalled(false)==0); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
	CHECK(stalled(true)==0); xrtMemDebugSnapshot(&after);
	CHECK(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
	puts("xrudp pending handshake: UDP backpressure cannot bypass the absolute deadline; allocation-free cleanup passed"); return 0;
}
