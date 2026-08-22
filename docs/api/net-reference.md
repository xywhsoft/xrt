# 网络公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[net.md](net.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `371` 个函数、`281` 个常量或宏、
`123` 个公共类型。

## `include/xrt/net.h`

[查看带契约注释的公共头](../../include/xrt/net.h)

### 函数 (172)

- `xrtNetAddrAny`
- `xrtNetAddrCompare`
- `xrtNetAddrEndpointString`
- `xrtNetAddrEndpointText`
- `xrtNetAddrEqual`
- `xrtNetAddrFromNative`
- `xrtNetAddrIsLinkLocal`
- `xrtNetAddrIsLoopback`
- `xrtNetAddrIsMapped`
- `xrtNetAddrIsMulticast`
- `xrtNetAddrIsPrivate`
- `xrtNetAddrIsUnspecified`
- `xrtNetAddrListCount`
- `xrtNetAddrListCreate`
- `xrtNetAddrListDestroy`
- `xrtNetAddrListGet`
- `xrtNetAddrListRef`
- `xrtNetAddrListWithPort`
- `xrtNetAddrLoopback`
- `xrtNetAddrParse`
- `xrtNetAddrParseEndpoint`
- `xrtNetAddrSameIP`
- `xrtNetAddrString`
- `xrtNetAddrText`
- `xrtNetAddrToNative`
- `xrtNetAddrUnmap`
- `xrtNetBufAppend`
- `xrtNetBufAppendBorrow`
- `xrtNetBufAppendRef`
- `xrtNetBufAppendTake`
- `xrtNetBufCancel`
- `xrtNetBufClear`
- `xrtNetBufCommit`
- `xrtNetBufConsume`
- `xrtNetBufEmpty`
- `xrtNetBufFind`
- `xrtNetBufFront`
- `xrtNetBufInit`
- `xrtNetBufMove`
- `xrtNetBufPeek`
- `xrtNetBufPoolConfigInit`
- `xrtNetBufPoolCreate`
- `xrtNetBufPoolDestroy`
- `xrtNetBufPoolGet`
- `xrtNetBufPoolTrim`
- `xrtNetBufPrepend`
- `xrtNetBufPullup`
- `xrtNetBufRead`
- `xrtNetBufReserve`
- `xrtNetBufSize`
- `xrtNetBufSpanCount`
- `xrtNetBufSpans`
- `xrtNetBytesDestroy`
- `xrtNetBytesRef`
- `xrtNetBytesView`
- `xrtNetCompletionInit`
- `xrtNetEngineAfter`
- `xrtNetEngineConfigInit`
- `xrtNetEngineCreate`
- `xrtNetEngineCurrent`
- `xrtNetEngineDestroy`
- `xrtNetEnginePin`
- `xrtNetEnginePost`
- `xrtNetEngineSchedule`
- `xrtNetEngineStart`
- `xrtNetEngineState`
- `xrtNetEngineStats`
- `xrtNetEngineStop`
- `xrtNetEngineTimerCancel`
- `xrtNetEngineTimerCancelCurrent`
- `xrtNetEngineUnpin`
- `xrtNetEngineWorker`
- `xrtNetEngineWorkerCount`
- `xrtNetLookup`
- `xrtNetPortAccept`
- `xrtNetPortBackend`
- `xrtNetPortCancel`
- `xrtNetPortCapabilities`
- `xrtNetPortConfigInit`
- `xrtNetPortConnect`
- `xrtNetPortCreate`
- `xrtNetPortDestroy`
- `xrtNetPortGetConfig`
- `xrtNetPortName`
- `xrtNetPortPost`
- `xrtNetPortReadProbe`
- `xrtNetPortRecv`
- `xrtNetPortRecvError`
- `xrtNetPortRecvFrom`
- `xrtNetPortRecvFromVec`
- `xrtNetPortRecvMsg`
- `xrtNetPortRecvMsgVec`
- `xrtNetPortRecvVec`
- `xrtNetPortSend`
- `xrtNetPortSendMsg`
- `xrtNetPortSendMsgVec`
- `xrtNetPortSendTo`
- `xrtNetPortSendToVec`
- `xrtNetPortSendVec`
- `xrtNetPortUnwatch`
- `xrtNetPortWait`
- `xrtNetPortWake`
- `xrtNetPortWatch`
- `xrtNetPost`
- `xrtNetPostInit`
- `xrtNetPostPending`
- `xrtNetResolve`
- `xrtNetResolveAsync`
- `xrtNetResolveOne`
- `xrtNetResolveOpCancel`
- `xrtNetResolveOpDestroy`
- `xrtNetResolveOpError`
- `xrtNetResolveOpRef`
- `xrtNetResolveOpResult`
- `xrtNetResolveOpState`
- `xrtNetResolverClear`
- `xrtNetResolverConfigInit`
- `xrtNetResolverCreate`
- `xrtNetResolverDestroy`
- `xrtNetResolverResolve`
- `xrtNetResolverStats`
- `xrtNetReverse`
- `xrtNetSocketAccept`
- `xrtNetSocketAvailable`
- `xrtNetSocketBind`
- `xrtNetSocketClose`
- `xrtNetSocketConnect`
- `xrtNetSocketDgramCapabilities`
- `xrtNetSocketDgramControlAvailable`
- `xrtNetSocketDgramMetaAvailable`
- `xrtNetSocketDgramMetaEnabled`
- `xrtNetSocketDgramMetaSet`
- `xrtNetSocketDgramRecvError`
- `xrtNetSocketFamily`
- `xrtNetSocketFinishConnect`
- `xrtNetSocketGet`
- `xrtNetSocketListen`
- `xrtNetSocketLocal`
- `xrtNetSocketMulticastHopLimit`
- `xrtNetSocketMulticastInterface`
- `xrtNetSocketMulticastJoin`
- `xrtNetSocketMulticastLeave`
- `xrtNetSocketMulticastLoop`
- `xrtNetSocketNative`
- `xrtNetSocketOpen`
- `xrtNetSocketRecv`
- `xrtNetSocketRecvBatch`
- `xrtNetSocketRecvFrom`
- `xrtNetSocketRecvFromVec`
- `xrtNetSocketRecvMsg`
- `xrtNetSocketRecvMsgVec`
- `xrtNetSocketRecvVec`
- `xrtNetSocketRemote`
- `xrtNetSocketSend`
- `xrtNetSocketSendBatch`
- `xrtNetSocketSendMsg`
- `xrtNetSocketSendMsgVec`
- `xrtNetSocketSendTo`
- `xrtNetSocketSendToVec`
- `xrtNetSocketSendVec`
- `xrtNetSocketSet`
- `xrtNetSocketShutdown`
- `xrtNetSocketType`
- `xrtNetWorkerAlloc`
- `xrtNetWorkerBufPool`
- `xrtNetWorkerEngine`
- `xrtNetWorkerFree`
- `xrtNetWorkerIndex`
- `xrtNetWorkerIsCurrent`
- `xrtNetWorkerOperationId`
- `xrtNetWorkerPort`
- `xrtNetWorkerStats`

### 常量与宏 (199)

- `XNET_BUFFER_CLASS_COUNT`
- `XNET_DGRAM_BATCH_MAX`
- `XNET_DGRAM_CAP_ERROR_QUEUE`
- `XNET_DGRAM_CAP_PATH_MTU_MODE`
- `XNET_DGRAM_CAP_PATH_MTU_QUERY`
- `XNET_DGRAM_CAP_SEGMENT_RECEIVE`
- `XNET_DGRAM_CAP_SEGMENT_SEND`
- `XNET_DGRAM_CONTROL_HOP_LIMIT`
- `XNET_DGRAM_CONTROL_INTERFACE`
- `XNET_DGRAM_CONTROL_SEGMENT_SIZE`
- `XNET_DGRAM_CONTROL_SOURCE`
- `XNET_DGRAM_CONTROL_TRAFFIC_CLASS`
- `XNET_DGRAM_ERROR_ICMP`
- `XNET_DGRAM_ERROR_ICMP6`
- `XNET_DGRAM_ERROR_LOCAL`
- `XNET_DGRAM_ERROR_META_TRUNCATED`
- `XNET_DGRAM_ERROR_OFFENDER`
- `XNET_DGRAM_ERROR_PATH_MTU`
- `XNET_DGRAM_ERROR_PAYLOAD_TRUNCATED`
- `XNET_DGRAM_ERROR_REMOTE`
- `XNET_DGRAM_ERROR_UNKNOWN`
- `XNET_DGRAM_META_DESTINATION`
- `XNET_DGRAM_META_HOP_LIMIT`
- `XNET_DGRAM_META_INTERFACE`
- `XNET_DGRAM_META_SEGMENT_SIZE`
- `XNET_DGRAM_META_TRAFFIC_CLASS`
- `XNET_DGRAM_META_TRUNCATED`
- `XNET_ENGINE_DESTROYING`
- `XNET_ENGINE_RUNNING`
- `XNET_ENGINE_STARTING`
- `XNET_ENGINE_STOPPED`
- `XNET_ENGINE_STOPPING`
- `XNET_ERROR_BUFFER`
- `XNET_ERROR_BUFFER_STATE`
- `XNET_ERROR_DIAL_CONFIG`
- `XNET_ERROR_DIAL_CONNECT`
- `XNET_ERROR_DIAL_CREATE`
- `XNET_ERROR_DIAL_RESOLVE`
- `XNET_ERROR_DNS_RESOLVE`
- `XNET_ERROR_DNS_RESULT`
- `XNET_ERROR_DNS_REVERSE`
- `XNET_ERROR_ENGINE_CREATE`
- `XNET_ERROR_ENGINE_POST`
- `XNET_ERROR_ENGINE_START`
- `XNET_ERROR_ENGINE_STOP`
- `XNET_ERROR_ENGINE_TIMER`
- `XNET_ERROR_FAMILY`
- `XNET_ERROR_FORMAT`
- `XNET_ERROR_FRAME_CONFIG`
- `XNET_ERROR_FRAME_LENGTH`
- `XNET_ERROR_FRAME_LIMIT`
- `XNET_ERROR_FRAME_STATE`
- `XNET_ERROR_HOST_NAME`
- `XNET_ERROR_INTERFACE_ADDRESS`
- `XNET_ERROR_INTERFACE_HARDWARE`
- `XNET_ERROR_INTERFACE_INDEX`
- `XNET_ERROR_INTERFACE_NAME`
- `XNET_ERROR_INTERFACE_QUERY`
- `XNET_ERROR_LISTENER_ACCEPT`
- `XNET_ERROR_LISTENER_CLOSE`
- `XNET_ERROR_LISTENER_CREATE`
- `XNET_ERROR_NATIVE`
- `XNET_ERROR_NONE`
- `XNET_ERROR_POOL_BUSY`
- `XNET_ERROR_PORT`
- `XNET_ERROR_PORT_CANCEL`
- `XNET_ERROR_PORT_CLOSE`
- `XNET_ERROR_PORT_CREATE`
- `XNET_ERROR_PORT_POST`
- `XNET_ERROR_PORT_SUBMIT`
- `XNET_ERROR_PORT_WAIT`
- `XNET_ERROR_PORT_WATCH`
- `XNET_ERROR_PROXY_AUTH`
- `XNET_ERROR_PROXY_CONFIG`
- `XNET_ERROR_PROXY_CONNECT`
- `XNET_ERROR_PROXY_CREATE`
- `XNET_ERROR_PROXY_LIMIT`
- `XNET_ERROR_PROXY_PROTOCOL`
- `XNET_ERROR_PROXY_UNSUPPORTED`
- `XNET_ERROR_RESOLVER_CLOSED`
- `XNET_ERROR_RESOLVER_CREATE`
- `XNET_ERROR_RESOLVER_QUERY`
- `XNET_ERROR_RESOLVER_SUBMIT`
- `XNET_ERROR_SCOPE`
- `XNET_ERROR_SERVER_ACCEPT`
- `XNET_ERROR_SERVER_CONFIG`
- `XNET_ERROR_SERVER_START`
- `XNET_ERROR_SOCKET_ACCEPT`
- `XNET_ERROR_SOCKET_BIND`
- `XNET_ERROR_SOCKET_CLOSE`
- `XNET_ERROR_SOCKET_CONNECT`
- `XNET_ERROR_SOCKET_DGRAM_ERROR`
- `XNET_ERROR_SOCKET_LISTEN`
- `XNET_ERROR_SOCKET_OPEN`
- `XNET_ERROR_SOCKET_OPTION`
- `XNET_ERROR_SOCKET_READ`
- `XNET_ERROR_SOCKET_SHUTDOWN`
- `XNET_ERROR_SOCKET_WRITE`
- `XNET_ERROR_STREAM_CLOSE`
- `XNET_ERROR_STREAM_CONFIG`
- `XNET_ERROR_STREAM_CONNECT`
- `XNET_ERROR_STREAM_CREATE`
- `XNET_ERROR_STREAM_READ`
- `XNET_ERROR_STREAM_WRITE`
- `XNET_ERROR_SYSTEM`
- `XNET_ERROR_UDP_CLOSE`
- `XNET_ERROR_UDP_CONFIG`
- `XNET_ERROR_UDP_CREATE`
- `XNET_ERROR_UDP_RECEIVE`
- `XNET_ERROR_UDP_RECEIVE_QUEUE`
- `XNET_ERROR_UDP_SEND`
- `XNET_FAMILY_IPV4`
- `XNET_FAMILY_IPV6`
- `XNET_FAMILY_UNSPEC`
- `XNET_OPTION_BROADCAST`
- `XNET_OPTION_DGRAM_ERRORS`
- `XNET_OPTION_ERROR`
- `XNET_OPTION_EXCLUSIVE_ADDRESS`
- `XNET_OPTION_HOP_LIMIT`
- `XNET_OPTION_IPV6_ONLY`
- `XNET_OPTION_KEEP_ALIVE`
- `XNET_OPTION_LINGER`
- `XNET_OPTION_NONBLOCK`
- `XNET_OPTION_NO_DELAY`
- `XNET_OPTION_PATH_MTU`
- `XNET_OPTION_PATH_MTU_MODE`
- `XNET_OPTION_RECEIVE_BUFFER`
- `XNET_OPTION_REUSE_ADDRESS`
- `XNET_OPTION_REUSE_PORT`
- `XNET_OPTION_SEND_BUFFER`
- `XNET_OPTION_TRAFFIC_CLASS`
- `XNET_PMTU_DISCOVER`
- `XNET_PMTU_FRAGMENT`
- `XNET_PMTU_PROBE`
- `XNET_PMTU_SYSTEM`
- `XNET_POLL_READ`
- `XNET_POLL_WRITE`
- `XNET_PORT_AUTO`
- `XNET_PORT_CAP_BATCH`
- `XNET_PORT_CAP_CANCEL`
- `XNET_PORT_CAP_COMPLETION`
- `XNET_PORT_CAP_DGRAM_ERROR`
- `XNET_PORT_CAP_EDGE`
- `XNET_PORT_CAP_FILE_IO`
- `XNET_PORT_CAP_ONESHOT`
- `XNET_PORT_CAP_POST`
- `XNET_PORT_CAP_READINESS`
- `XNET_PORT_CAP_READ_PROBE`
- `XNET_PORT_CAP_SEND_FILE`
- `XNET_PORT_CAP_WAKE`
- `XNET_PORT_EPOLL`
- `XNET_PORT_EVENT_ACCEPT`
- `XNET_PORT_EVENT_CONNECT`
- `XNET_PORT_EVENT_EOF`
- `XNET_PORT_EVENT_ERROR`
- `XNET_PORT_EVENT_FILE_READ`
- `XNET_PORT_EVENT_FILE_WRITE`
- `XNET_PORT_EVENT_HANGUP`
- `XNET_PORT_EVENT_MORE`
- `XNET_PORT_EVENT_READ`
- `XNET_PORT_EVENT_READY`
- `XNET_PORT_EVENT_READ_PROBE`
- `XNET_PORT_EVENT_RECV`
- `XNET_PORT_EVENT_RECV_ERROR`
- `XNET_PORT_EVENT_RECV_FROM`
- `XNET_PORT_EVENT_RECV_MSG`
- `XNET_PORT_EVENT_SEND`
- `XNET_PORT_EVENT_SEND_FILE`
- `XNET_PORT_EVENT_SEND_MSG`
- `XNET_PORT_EVENT_SEND_TO`
- `XNET_PORT_EVENT_USER`
- `XNET_PORT_EVENT_WAKE`
- `XNET_PORT_EVENT_WRITE`
- `XNET_PORT_IOCP`
- `XNET_PORT_KQUEUE`
- `XNET_PORT_SELECT`
- `XNET_PORT_URING`
- `XNET_POST_STORAGE_SIZE`
- `XNET_RESOLVE_CANCELLED`
- `XNET_RESOLVE_FAILED`
- `XNET_RESOLVE_PENDING`
- `XNET_RESOLVE_RESOLVED`
- `XNET_RESOLVE_RUNNING`
- `XNET_RESULT_AGAIN`
- `XNET_RESULT_CANCELLED`
- `XNET_RESULT_CLOSED`
- `XNET_RESULT_ERROR`
- `XNET_RESULT_OK`
- `XNET_RESULT_TIMEOUT`
- `XNET_RESULT_TRUNCATED`
- `XNET_SHUTDOWN_BOTH`
- `XNET_SHUTDOWN_READ`
- `XNET_SHUTDOWN_WRITE`
- `XNET_SOCKET_DGRAM`
- `XNET_SOCKET_NONBLOCK`
- `XNET_SOCKET_STREAM`
- `XNET_STATS_BASIC`
- `XNET_STATS_FULL`
- `XNET_STATS_OFF`

### 类型 (59)

- `xnetaddr`
- `xnetaddrlist`
- `xnetblock`
- `xnetbuf`
- `xnetbufpool`
- `xnetbufpoolconfig`
- `xnetbufpoolinfo`
- `xnetbytes`
- `xnetcompletion`
- `xnetcompletionproc`
- `xnetdgramcap`
- `xnetdgramcontrol`
- `xnetdgramcontrolflag`
- `xnetdgramerror`
- `xnetdgramerrorflag`
- `xnetdgramerrororigin`
- `xnetdgrammeta`
- `xnetdgrammetaflag`
- `xnetdgramrecv`
- `xnetdgramsend`
- `xnetengine`
- `xnetengineconfig`
- `xnetenginestate`
- `xnetenginestats`
- `xneterror`
- `xnetfamily`
- `xnetoption`
- `xnetpmtumode`
- `xnetpoll`
- `xnetport`
- `xnetport_impl`
- `xnetportbackend`
- `xnetportcap`
- `xnetportconfig`
- `xnetportevent`
- `xnetporteventflag`
- `xnetporteventtype`
- `xnetpost`
- `xnetref`
- `xnetreleaseproc`
- `xnetresolveop`
- `xnetresolveopstate`
- `xnetresolveproc`
- `xnetresolver`
- `xnetresolverconfig`
- `xnetresolverlookup`
- `xnetresolverstats`
- `xnetresult`
- `xnetshutdown`
- `xnetsocket`
- `xnetsocket_impl`
- `xnetsocketflag`
- `xnetsockettype`
- `xnetspan`
- `xnettaskproc`
- `xnettimerproc`
- `xnetworker`
- `xnetworkerstats`
- `xnetwspan`

## `include/xrt/net_file.h`

[查看带契约注释的公共头](../../include/xrt/net_file.h)

### 函数 (4)

- `xrtNetFileCancel`
- `xrtNetFileOpen`
- `xrtNetFileRead`
- `xrtNetFileWrite`

## `include/xrt/net_frame.h`

[查看带契约注释的公共头](../../include/xrt/net_frame.h)

### 函数 (9)

- `xrtNetFrameConsume`
- `xrtNetFrameCopy`
- `xrtNetLengthConfigInit`
- `xrtNetLengthInit`
- `xrtNetLengthNext`
- `xrtNetLineConfigInit`
- `xrtNetLineInit`
- `xrtNetLineNext`
- `xrtNetLineReset`

### 常量与宏 (5)

- `XNET_FRAME_BIG_ENDIAN`
- `XNET_FRAME_ERROR`
- `XNET_FRAME_LITTLE_ENDIAN`
- `XNET_FRAME_MORE`
- `XNET_FRAME_READY`

### 类型 (7)

- `xnetframe`
- `xnetframeorder`
- `xnetframestatus`
- `xnetlengthconfig`
- `xnetlengthframer`
- `xnetlineconfig`
- `xnetlineframer`

## `include/xrt/net_interface.h`

[查看带契约注释的公共头](../../include/xrt/net_interface.h)

### 函数 (12)

- `xrtNetHostName`
- `xrtNetHostNameString`
- `xrtNetInterfaceIndex`
- `xrtNetInterfaceName`
- `xrtNetInterfaces`
- `xrtNetInterfacesFree`
- `xrtNetLocalAddress`
- `xrtNetLocalAddressString`
- `xrtNetLocalAddressText`
- `xrtNetLocalHardware`
- `xrtNetLocalHardwareString`
- `xrtNetLocalHardwareText`

### 常量与宏 (7)

- `XNET_INTERFACE_BROADCAST`
- `XNET_INTERFACE_LOOPBACK`
- `XNET_INTERFACE_MULTICAST`
- `XNET_INTERFACE_POINT_TO_POINT`
- `XNET_INTERFACE_PREFIX_UNKNOWN`
- `XNET_INTERFACE_RUNNING`
- `XNET_INTERFACE_UP`

### 类型 (4)

- `xnetinterface`
- `xnetinterfaceaddress`
- `xnetinterfaceflag`
- `xnetinterfacelist`

## `include/xrt/proxy.h`

[查看带契约注释的公共头](../../include/xrt/proxy.h)

### 函数 (23)

- `xrtNetProxyConfigInit`
- `xrtNetProxyCreate`
- `xrtNetProxyDial`
- `xrtNetProxyDialCancel`
- `xrtNetProxyDialConfigInit`
- `xrtNetProxyDialDestroy`
- `xrtNetProxyDialError`
- `xrtNetProxyDialRef`
- `xrtNetProxyDialState`
- `xrtNetProxyDialStats`
- `xrtNetProxyHandshakeBound`
- `xrtNetProxyHandshakeCode`
- `xrtNetProxyHandshakeConfigInit`
- `xrtNetProxyHandshakeCreate`
- `xrtNetProxyHandshakeDestroy`
- `xrtNetProxyHandshakeError`
- `xrtNetProxyHandshakeOutput`
- `xrtNetProxyHandshakeSent`
- `xrtNetProxyHandshakeState`
- `xrtNetProxyHandshakeStep`
- `xrtNetProxyInfo`
- `xrtNetProxyRelease`
- `xrtNetProxyRetain`

### 常量与宏 (25)

- `XNET_PROXY_AUTH_AUTO`
- `XNET_PROXY_AUTH_NONE`
- `XNET_PROXY_AUTH_OPTIONAL`
- `XNET_PROXY_AUTH_REQUIRED`
- `XNET_PROXY_DIAL_CANCELLED`
- `XNET_PROXY_DIAL_CONNECTED`
- `XNET_PROXY_DIAL_CONNECTING`
- `XNET_PROXY_DIAL_FAILED`
- `XNET_PROXY_DIAL_HANDSHAKE`
- `XNET_PROXY_DIAL_RESOLVING`
- `XNET_PROXY_HANDSHAKE_ERROR`
- `XNET_PROXY_HANDSHAKE_READ`
- `XNET_PROXY_HANDSHAKE_READY`
- `XNET_PROXY_HANDSHAKE_WRITE`
- `XNET_PROXY_HTTP_CONNECT`
- `XNET_PROXY_SOCKS5`
- `XNET_SOCKS5_ADDRESS_UNSUPPORTED`
- `XNET_SOCKS5_COMMAND_UNSUPPORTED`
- `XNET_SOCKS5_CONNECTION_REFUSED`
- `XNET_SOCKS5_GENERAL_FAILURE`
- `XNET_SOCKS5_HOST_UNREACHABLE`
- `XNET_SOCKS5_NETWORK_UNREACHABLE`
- `XNET_SOCKS5_RULESET_DENIED`
- `XNET_SOCKS5_SUCCEEDED`
- `XNET_SOCKS5_TTL_EXPIRED`

### 类型 (19)

- `xnetdialconfig`
- `xnetdialstats`
- `xnetproxy`
- `xnetproxyauth`
- `xnetproxyconfig`
- `xnetproxydial`
- `xnetproxydialconfig`
- `xnetproxydialproc`
- `xnetproxydialstate`
- `xnetproxydialstats`
- `xnetproxyendpoint`
- `xnetproxyhandshake`
- `xnetproxyhandshakeconfig`
- `xnetproxyhandshakestate`
- `xnetproxyinfo`
- `xnetproxytype`
- `xnetsocks5reply`
- `xnetstream`
- `xnetstreamevents`

## `include/xrt/tcp.h`

[查看带契约注释的公共头](../../include/xrt/tcp.h)

### 函数 (63)

- `xrtNetConnect`
- `xrtNetDial`
- `xrtNetDialAsync`
- `xrtNetDialCancel`
- `xrtNetDialConfigInit`
- `xrtNetDialConfigValid`
- `xrtNetDialDestroy`
- `xrtNetDialError`
- `xrtNetDialRef`
- `xrtNetDialState`
- `xrtNetDialStats`
- `xrtNetListen`
- `xrtNetListenConfigInit`
- `xrtNetListenerAccept`
- `xrtNetListenerAcceptAsync`
- `xrtNetListenerAcceptWait`
- `xrtNetListenerClose`
- `xrtNetListenerData`
- `xrtNetListenerDestroy`
- `xrtNetListenerLocal`
- `xrtNetListenerRef`
- `xrtNetListenerState`
- `xrtNetListenerStats`
- `xrtNetListenerWorker`
- `xrtNetStreamAbort`
- `xrtNetStreamAvailable`
- `xrtNetStreamBuffer`
- `xrtNetStreamClose`
- `xrtNetStreamConfigInit`
- `xrtNetStreamConnect`
- `xrtNetStreamConsume`
- `xrtNetStreamData`
- `xrtNetStreamDestroy`
- `xrtNetStreamError`
- `xrtNetStreamLocal`
- `xrtNetStreamPause`
- `xrtNetStreamPending`
- `xrtNetStreamRead`
- `xrtNetStreamRecv`
- `xrtNetStreamRecvAsync`
- `xrtNetStreamRef`
- `xrtNetStreamRemote`
- `xrtNetStreamResume`
- `xrtNetStreamSend`
- `xrtNetStreamSendBuffer`
- `xrtNetStreamSendFile`
- `xrtNetStreamSendRef`
- `xrtNetStreamSendRefs`
- `xrtNetStreamSendTake`
- `xrtNetStreamSendVec`
- `xrtNetStreamSetData`
- `xrtNetStreamSetEvents`
- `xrtNetStreamShutdownWrite`
- `xrtNetStreamSocket`
- `xrtNetStreamState`
- `xrtNetStreamStats`
- `xrtNetStreamWait`
- `xrtNetStreamWaitAsync`
- `xrtNetStreamWaitAvailable`
- `xrtNetStreamWaitAvailableAsync`
- `xrtNetStreamWorker`
- `xrtNetStreamWritable`
- `xrtNetStreamWriteLimit`

### 常量与宏 (22)

- `XNET_ACCEPT_LOCAL`
- `XNET_ACCEPT_ROUND_ROBIN`
- `XNET_DIAL_CANCELLED`
- `XNET_DIAL_CONNECTED`
- `XNET_DIAL_CONNECTING`
- `XNET_DIAL_FAILED`
- `XNET_DIAL_RESOLVING`
- `XNET_LISTENER_CLOSED`
- `XNET_LISTENER_CLOSING`
- `XNET_LISTENER_OPEN`
- `XNET_STREAM_CLOSED`
- `XNET_STREAM_CLOSING`
- `XNET_STREAM_CONNECTING`
- `XNET_STREAM_OPEN`
- `XNET_STREAM_READ_ADAPTIVE`
- `XNET_STREAM_READ_DIRECT`
- `XNET_STREAM_READ_PROBE`
- `XNET_STREAM_WAIT_CLOSE`
- `XNET_STREAM_WAIT_DRAIN`
- `XNET_STREAM_WAIT_OPEN`
- `XNET_STREAM_WAIT_READ`
- `XNET_STREAM_WAIT_WRITE`

### 类型 (14)

- `xnetacceptdistribution`
- `xnetdial`
- `xnetdialproc`
- `xnetdialstate`
- `xnetlistenconfig`
- `xnetlistener`
- `xnetlistenerevents`
- `xnetlistenerstate`
- `xnetlistenerstats`
- `xnetstreamconfig`
- `xnetstreamreadmode`
- `xnetstreamstate`
- `xnetstreamstats`
- `xnetstreamwait`

## `include/xrt/tcp_server.h`

[查看带契约注释的公共头](../../include/xrt/tcp_server.h)

### 函数 (15)

- `xrtNetServerAccept`
- `xrtNetServerAcceptAsync`
- `xrtNetServerAcceptWait`
- `xrtNetServerClose`
- `xrtNetServerConfigInit`
- `xrtNetServerData`
- `xrtNetServerDestroy`
- `xrtNetServerEndpointCount`
- `xrtNetServerListener`
- `xrtNetServerListenerCount`
- `xrtNetServerLocal`
- `xrtNetServerRef`
- `xrtNetServerStart`
- `xrtNetServerState`
- `xrtNetServerStats`

### 常量与宏 (6)

- `XNET_SERVER_CLOSED`
- `XNET_SERVER_CLOSING`
- `XNET_SERVER_OPEN`
- `XNET_SERVER_REUSE_PORT`
- `XNET_SERVER_SHARED`
- `XNET_SERVER_STARTING`

### 类型 (6)

- `xnetserver`
- `xnetserverconfig`
- `xnetserverevents`
- `xnetservermode`
- `xnetserverstate`
- `xnetserverstats`

## `include/xrt/udp.h`

[查看带契约注释的公共头](../../include/xrt/udp.h)

### 函数 (73)

- `xrtNetUdpAbort`
- `xrtNetUdpBatchCount`
- `xrtNetUdpBatchDestroy`
- `xrtNetUdpBatchPacket`
- `xrtNetUdpBatchRef`
- `xrtNetUdpBatchTake`
- `xrtNetUdpBind`
- `xrtNetUdpClose`
- `xrtNetUdpConfigInit`
- `xrtNetUdpConnect`
- `xrtNetUdpConnected`
- `xrtNetUdpData`
- `xrtNetUdpDestroy`
- `xrtNetUdpError`
- `xrtNetUdpErrorPacketData`
- `xrtNetUdpErrorPacketDestroy`
- `xrtNetUdpErrorPacketInfo`
- `xrtNetUdpErrorPacketRef`
- `xrtNetUdpErrorPacketSize`
- `xrtNetUdpJoin`
- `xrtNetUdpLeave`
- `xrtNetUdpLocal`
- `xrtNetUdpMulticastHopLimit`
- `xrtNetUdpMulticastInterface`
- `xrtNetUdpMulticastLoop`
- `xrtNetUdpOpen`
- `xrtNetUdpPacketData`
- `xrtNetUdpPacketDestroy`
- `xrtNetUdpPacketMeta`
- `xrtNetUdpPacketRef`
- `xrtNetUdpPacketRemote`
- `xrtNetUdpPacketSize`
- `xrtNetUdpPacketTruncated`
- `xrtNetUdpPathMtu`
- `xrtNetUdpPeer`
- `xrtNetUdpPending`
- `xrtNetUdpQueued`
- `xrtNetUdpQueuedBytes`
- `xrtNetUdpQueuedErrorBytes`
- `xrtNetUdpQueuedErrors`
- `xrtNetUdpReceive`
- `xrtNetUdpReceiveAsync`
- `xrtNetUdpReceiveBatch`
- `xrtNetUdpReceiveBatchAsync`
- `xrtNetUdpReceiveBatchWait`
- `xrtNetUdpReceiveError`
- `xrtNetUdpReceiveErrorAsync`
- `xrtNetUdpReceiveErrorBatch`
- `xrtNetUdpReceiveErrorWait`
- `xrtNetUdpReceiveWait`
- `xrtNetUdpRef`
- `xrtNetUdpSend`
- `xrtNetUdpSendBatch`
- `xrtNetUdpSendControlAvailable`
- `xrtNetUdpSendMsg`
- `xrtNetUdpSendMsgRef`
- `xrtNetUdpSendMsgTake`
- `xrtNetUdpSendRef`
- `xrtNetUdpSendRefTo`
- `xrtNetUdpSendTake`
- `xrtNetUdpSendTakeTo`
- `xrtNetUdpSendTo`
- `xrtNetUdpSendVec`
- `xrtNetUdpSendVecTo`
- `xrtNetUdpSetData`
- `xrtNetUdpSocket`
- `xrtNetUdpState`
- `xrtNetUdpStats`
- `xrtNetUdpWait`
- `xrtNetUdpWaitAsync`
- `xrtNetUdpWorker`
- `xrtNetUdpWritable`
- `xrtNetUdpWritableAsync`

### 常量与宏 (17)

- `XNET_UDP_CLOSED`
- `XNET_UDP_CLOSING`
- `XNET_UDP_DROP_ERROR`
- `XNET_UDP_DROP_NEWEST`
- `XNET_UDP_DROP_OLDEST`
- `XNET_UDP_MESSAGE_TRUNCATED`
- `XNET_UDP_OPEN`
- `XNET_UDP_OPENING`
- `XNET_UDP_PAYLOAD_MAX`
- `XNET_UDP_TRUNCATE_DELIVER`
- `XNET_UDP_TRUNCATE_DROP`
- `XNET_UDP_TRUNCATE_ERROR`
- `XNET_UDP_WAIT_CLOSE`
- `XNET_UDP_WAIT_DRAIN`
- `XNET_UDP_WAIT_ERROR`
- `XNET_UDP_WAIT_OPEN`
- `XNET_UDP_WAIT_RECEIVE`

### 类型 (14)

- `xnetudp`
- `xnetudpbatch`
- `xnetudpconfig`
- `xnetudperrormessage`
- `xnetudperrorpacket`
- `xnetudpevents`
- `xnetudpflag`
- `xnetudpmessage`
- `xnetudpoverflow`
- `xnetudppacket`
- `xnetudpstate`
- `xnetudpstats`
- `xnetudptruncation`
- `xnetudpwait`
