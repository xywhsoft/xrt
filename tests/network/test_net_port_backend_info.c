/* Compiled-driver metadata only: no sockets, ports, external or loopback I/O. */
#include <xrt.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int compiled(xnetportbackend backend)
{
	switch (backend) {
#if defined(XRT_FEATURE_NET_PORT_SELECT)
	case XNET_PORT_SELECT: return 1;
#endif
#if defined(XRT_FEATURE_NET_PORT_IOCP) && defined(_WIN32)
	case XNET_PORT_IOCP: return 1;
#endif
#if defined(XRT_FEATURE_NET_PORT_EPOLL) && defined(__linux__)
	case XNET_PORT_EPOLL: return 1;
#endif
#if defined(XRT_FEATURE_NET_PORT_URING) && defined(__linux__)
	case XNET_PORT_URING: return 1;
#endif
#if defined(XRT_FEATURE_NET_PORT_KQUEUE) && (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__))
	case XNET_PORT_KQUEUE: return 1;
#endif
	default: return 0;
	}
}
int main(void)
{
	xnetportbackendinfo sentinel={XNET_PORT_AUTO,"unchanged",UINT32_MAX};
	xrtSetErrorKind(XERR_ARGUMENT);
	const xerror* prior=xrtGetError();
	for (int value=-1;value<=XNET_PORT_SELECT+1;++value) {
		xnetportbackendinfo info=sentinel;
		bool found=xrtNetPortBackendInfo((xnetportbackend)value,&info);
		if (value==XNET_PORT_AUTO) {
			int expected=0;
			for (int b=1;b<=XNET_PORT_SELECT;++b)expected|=compiled((xnetportbackend)b);
			if (found!=(expected!=0))return 1;
		} else if (found!=(compiled((xnetportbackend)value)!=0))return 2;
		if (xrtGetError()!=prior)return 3;
		if (found) {
			if (info.Backend<=XNET_PORT_AUTO||info.Backend>XNET_PORT_SELECT||!compiled(info.Backend)||
				!info.Name||!info.Name[0]||!info.Capabilities)return 4;
			if (value!=XNET_PORT_AUTO&&info.Backend!=(xnetportbackend)value)return 5;
			printf("backend %d => %d %s 0x%08x\n",value,(int)info.Backend,info.Name,(unsigned)info.Capabilities);
		} else if (info.Backend!=sentinel.Backend||info.Name!=sentinel.Name||info.Capabilities!=sentinel.Capabilities)return 6;
	}
	if (xrtNetPortBackendInfo((xnetportbackend)INT_MAX,&sentinel)||
		xrtNetPortBackendInfo(XNET_PORT_AUTO,NULL)||xrtGetError()!=prior)return 7;
	xrtClearError();
	puts("backend metadata: exact compiled set, output/error preservation; no OS initialization");
	return 0;
}
