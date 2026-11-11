# xredis public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xredis/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `163` 个函数、`269` 个常量或宏、
`122` 个公共类型。

## `extlibs/xredis/include/xredis/client.h`

[查看带契约注释的公共头](../../include/xredis/client.h)

### 函数 (13)

- `xredisClientCall`
- `xredisClientCallAsync`
- `xredisClientClose`
- `xredisClientClosed`
- `xredisClientConfigInit`
- `xredisClientConnect`
- `xredisClientConnectInit`
- `xredisClientInfo`
- `xredisClientInitConfigInit`
- `xredisClientReady`
- `xredisClientRelease`
- `xredisClientRetain`
- `xredisClientTakePush`

### 常量与宏 (54)

- `XREDIS_ADMISSION_ACCEPTED`
- `XREDIS_ADMISSION_CLOSED`
- `XREDIS_ADMISSION_ERROR`
- `XREDIS_ADMISSION_OVERLOADED`
- `XREDIS_CLIENT_ARGUMENT`
- `XREDIS_CLIENT_CLOSED`
- `XREDIS_CLIENT_CLOSING`
- `XREDIS_CLIENT_COMMAND_MODE`
- `XREDIS_CLIENT_CONNECTING`
- `XREDIS_CLIENT_INITIALIZATION_FAILED`
- `XREDIS_CLIENT_OPEN`
- `XREDIS_CLIENT_STATE`
- `XREDIS_FEATURE_XREDIS_CLIENT`
- `XREDIS_FEATURE_XREDIS_COMMAND`
- `XREDIS_FEATURE_XREDIS_REPLY`
- `XREDIS_INIT_AUTH`
- `XREDIS_INIT_AUTHENTICATE`
- `XREDIS_INIT_AUTO`
- `XREDIS_INIT_DATABASE`
- `XREDIS_INIT_DONE`
- `XREDIS_INIT_HELLO`
- `XREDIS_INIT_NAME`
- `XREDIS_INIT_NONE`
- `XREDIS_INIT_RESP2`
- `XREDIS_INIT_RESP3`
- `XREDIS_INIT_ROLE`
- `XREDIS_INIT_SELECT`
- `XREDIS_INIT_SETNAME`
- `XREDIS_INIT_USERNAME`
- `XREDIS_MAY_HAVE_EXECUTED`
- `XREDIS_NOT_SENT`
- `XREDIS_PROTOCOL_UNKNOWN`
- `XREDIS_READY_AUTH_FAILED`
- `XREDIS_READY_CLOSED`
- `XREDIS_READY_DISCONNECTED`
- `XREDIS_READY_LIMIT`
- `XREDIS_READY_MEMORY`
- `XREDIS_READY_OK`
- `XREDIS_READY_PROTOCOL`
- `XREDIS_READY_ROLE_MISMATCH`
- `XREDIS_READY_SERVER_ERROR`
- `XREDIS_READY_TIMEOUT`
- `XREDIS_READY_UNSUPPORTED`
- `XREDIS_REPLIED`
- `XREDIS_REQUEST_CANCELLED`
- `XREDIS_REQUEST_CLOSED`
- `XREDIS_REQUEST_DISCONNECTED`
- `XREDIS_REQUEST_INITIALIZATION`
- `XREDIS_REQUEST_LIMIT`
- `XREDIS_REQUEST_MEMORY`
- `XREDIS_REQUEST_OK`
- `XREDIS_REQUEST_PROTOCOL`
- `XREDIS_REQUEST_SERVER_ERROR`
- `XREDIS_REQUEST_TIMEOUT`

### 类型 (17)

- `xredisadmission`
- `xredisclient`
- `xredisclientconfig`
- `xredisclienterror`
- `xredisclientinfo`
- `xredisclientinitconfig`
- `xredisclientstate`
- `xredisdelivery`
- `xredisinitmode`
- `xredisinitstep`
- `xredisprotocol`
- `xredisreadyresult`
- `xredisreadystatus`
- `xredisreply`
- `xredisreplyreaderconfig`
- `xredisrequestresult`
- `xredisrequeststatus`

## `extlibs/xredis/include/xredis/cluster_topology.h`

[查看带契约注释的公共头](../../include/xredis/cluster_topology.h)

### 函数 (10)

- `xredisClusterKeySlot`
- `xredisClusterLimitsInit`
- `xredisClusterRedirectRead`
- `xredisClusterTopologyCreate`
- `xredisClusterTopologyInfo`
- `xredisClusterTopologyLookup`
- `xredisClusterTopologyNode`
- `xredisClusterTopologyRange`
- `xredisClusterTopologyRelease`
- `xredisClusterTopologyRetain`

### 常量与宏 (24)

- `XREDIS_CLUSTER_ABSENT`
- `XREDIS_CLUSTER_ASK`
- `XREDIS_CLUSTER_ENDPOINT_HOST`
- `XREDIS_CLUSTER_ENDPOINT_SOURCE`
- `XREDIS_CLUSTER_ENDPOINT_UNKNOWN`
- `XREDIS_CLUSTER_ERROR`
- `XREDIS_CLUSTER_FAILED`
- `XREDIS_CLUSTER_HEALTH_OTHER`
- `XREDIS_CLUSTER_LEGACY_SLOTS`
- `XREDIS_CLUSTER_LIMIT`
- `XREDIS_CLUSTER_LOADING`
- `XREDIS_CLUSTER_MALFORMED`
- `XREDIS_CLUSTER_MASTER`
- `XREDIS_CLUSTER_MEMORY`
- `XREDIS_CLUSTER_MOVED`
- `XREDIS_CLUSTER_ONLINE`
- `XREDIS_CLUSTER_REPLICA`
- `XREDIS_CLUSTER_ROLE_OTHER`
- `XREDIS_CLUSTER_SERVER_ERROR`
- `XREDIS_CLUSTER_SHARDS`
- `XREDIS_CLUSTER_SLOTS`
- `XREDIS_CLUSTER_UNREPORTED`
- `XREDIS_CLUSTER_VALUE`
- `XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY`

### 类型 (12)

- `xredisclusterendpoint`
- `xredisclusterformat`
- `xredisclusterhealth`
- `xredisclusterlimits`
- `xredisclusternode`
- `xredisclusterrange`
- `xredisclusterredirect`
- `xredisclusterredirectkind`
- `xredisclusterrole`
- `xredisclusterstatus`
- `xredisclustertopology`
- `xredisclustertopologyinfo`

## `extlibs/xredis/include/xredis/command.h`

[查看带契约注释的公共头](../../include/xredis/command.h)

### 函数 (2)

- `xredisCommandSize`
- `xredisCommandWrite`

### 常量与宏 (4)

- `XREDIS_COMMAND_ARGUMENT`
- `XREDIS_COMMAND_CAPACITY`
- `XREDIS_COMMAND_OVERFLOW`
- `XREDIS_COMMAND_OVERLAP`

### 类型 (1)

- `xrediscommanderror`

## `extlibs/xredis/include/xredis/dial.h`

[查看带契约注释的公共头](../../include/xredis/dial.h)

### 函数 (3)

- `xredisClientDial`
- `xredisClientDialAddresses`
- `xredisClientDialConfigInit`

### 常量与宏 (3)

- `XREDIS_DIAL_ADDRESSES_MAX`
- `XREDIS_DIAL_HOST_MAX`
- `XREDIS_FEATURE_XREDIS_DIAL`

### 类型 (1)

- `xredisclientdialconfig`

## `extlibs/xredis/include/xredis/export.h`

[查看带契约注释的公共头](../../include/xredis/export.h)

### 常量与宏 (2)

- `XREDIS_BUILD_SHARED`
- `XREDIS_USE_SHARED`

## `extlibs/xredis/include/xredis/pool.h`

[查看带契约注释的公共头](../../include/xredis/pool.h)

### 函数 (16)

- `xredisLeaseAbort`
- `xredisLeaseCall`
- `xredisLeaseCallAsync`
- `xredisLeaseClosed`
- `xredisLeaseRelease`
- `xredisLeaseRetain`
- `xredisLeaseReturn`
- `xredisPoolAcquire`
- `xredisPoolAcquireAsync`
- `xredisPoolClose`
- `xredisPoolClosed`
- `xredisPoolConfigInit`
- `xredisPoolCreate`
- `xredisPoolInfo`
- `xredisPoolRelease`
- `xredisPoolRetain`

### 常量与宏 (4)

- `XREDIS_FEATURE_XREDIS_POOL`
- `XREDIS_POOL_CLOSED`
- `XREDIS_POOL_CLOSING`
- `XREDIS_POOL_OPEN`

### 类型 (7)

- `xredislease`
- `xredisleaseclosedresult`
- `xredisleaseresult`
- `xredispool`
- `xredispoolconfig`
- `xredispoolinfo`
- `xredispoolstate`

## `extlibs/xredis/include/xredis/pool_asking.h`

[查看带契约注释的公共头](../../include/xredis/pool_asking.h)

### 函数 (2)

- `xredisLeaseAskingCall`
- `xredisLeaseAskingCallAsync`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_POOL_ASKING`

### 类型 (1)

- `xredisaskingresult`

## `extlibs/xredis/include/xredis/pool_connect.h`

[查看带契约注释的公共头](../../include/xredis/pool_connect.h)

### 函数 (3)

- `xredisPoolConnect`
- `xredisPoolConnectConfigInit`
- `xredisPoolConnectInfo`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_POOL_CONNECT`

### 类型 (4)

- `xredispoolconnectconfig`
- `xredispoolconnectdrop`
- `xredispoolconnectinfo`
- `xredispoolconnectproc`

## `extlibs/xredis/include/xredis/pool_shared.h`

[查看带契约注释的公共头](../../include/xredis/pool_shared.h)

### 函数 (3)

- `xredisSharedPoolCall`
- `xredisSharedPoolCallAsync`
- `xredisSharedPoolCreate`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_POOL_SHARED`

## `extlibs/xredis/include/xredis/pool_shared_asking.h`

[查看带契约注释的公共头](../../include/xredis/pool_shared_asking.h)

### 函数 (2)

- `xredisSharedPoolAskingCall`
- `xredisSharedPoolAskingCallAsync`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING`

## `extlibs/xredis/include/xredis/pool_shared_connect.h`

[查看带契约注释的公共头](../../include/xredis/pool_shared_connect.h)

### 函数 (1)

- `xredisSharedPoolConnect`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT`

## `extlibs/xredis/include/xredis/pool_transaction.h`

[查看带契约注释的公共头](../../include/xredis/pool_transaction.h)

### 函数 (2)

- `xredisLeaseTransaction`
- `xredisLeaseTransactionAsync`

### 常量与宏 (2)

- `XREDIS_FEATURE_XREDIS_POOL_TRANSACTION`
- `XREDIS_FEATURE_XREDIS_TRANSACTION`

### 类型 (2)

- `xredistransactioncommand`
- `xredistransactionconfig`

## `extlibs/xredis/include/xredis/pool_watch.h`

[查看带契约注释的公共头](../../include/xredis/pool_watch.h)

### 函数 (2)

- `xredisLeaseWatch`
- `xredisLeaseWatchAsync`

### 常量与宏 (2)

- `XREDIS_FEATURE_XREDIS_POOL_WATCH`
- `XREDIS_FEATURE_XREDIS_WATCH`

### 类型 (1)

- `xrediswatchconfig`

## `extlibs/xredis/include/xredis/pubsub.h`

[查看带契约注释的公共头](../../include/xredis/pubsub.h)

### 函数 (4)

- `xredisPubSubCommandSize`
- `xredisPubSubCommandWrite`
- `xredisPubSubEventRead`
- `xredisPubSubPongRead`

### 常量与宏 (29)

- `XREDIS_FEATURE_XREDIS_PUBSUB`
- `XREDIS_PUBSUB_ARGUMENT`
- `XREDIS_PUBSUB_CAPACITY`
- `XREDIS_PUBSUB_CMD_PING`
- `XREDIS_PUBSUB_CMD_PSUBSCRIBE`
- `XREDIS_PUBSUB_CMD_PUNSUBSCRIBE`
- `XREDIS_PUBSUB_CMD_SSUBSCRIBE`
- `XREDIS_PUBSUB_CMD_SUBSCRIBE`
- `XREDIS_PUBSUB_CMD_SUNSUBSCRIBE`
- `XREDIS_PUBSUB_CMD_UNSUBSCRIBE`
- `XREDIS_PUBSUB_ERROR`
- `XREDIS_PUBSUB_EVENT`
- `XREDIS_PUBSUB_LIMIT`
- `XREDIS_PUBSUB_MALFORMED`
- `XREDIS_PUBSUB_MESSAGE`
- `XREDIS_PUBSUB_NAMES_MAX`
- `XREDIS_PUBSUB_NAME_ABSENT`
- `XREDIS_PUBSUB_OTHER`
- `XREDIS_PUBSUB_OVERLAP`
- `XREDIS_PUBSUB_PMESSAGE`
- `XREDIS_PUBSUB_PONG`
- `XREDIS_PUBSUB_PSUBSCRIBE`
- `XREDIS_PUBSUB_PUNSUBSCRIBE`
- `XREDIS_PUBSUB_SMESSAGE`
- `XREDIS_PUBSUB_SSUBSCRIBE`
- `XREDIS_PUBSUB_STRUCTURED`
- `XREDIS_PUBSUB_SUBSCRIBE`
- `XREDIS_PUBSUB_SUNSUBSCRIBE`
- `XREDIS_PUBSUB_UNSUBSCRIBE`

### 类型 (6)

- `xredispubsubcommand`
- `xredispubsuberror`
- `xredispubsubevent`
- `xredispubsubkind`
- `xredispubsubstatus`
- `xredisreplytype`

## `extlibs/xredis/include/xredis/reader.h`

[查看带契约注释的公共头](../../include/xredis/reader.h)

### 函数 (7)

- `xredisReaderConfigInit`
- `xredisReaderCreate`
- `xredisReaderDestroy`
- `xredisReaderFeed`
- `xredisReaderFinish`
- `xredisReaderInfo`
- `xredisReaderReset`

### 常量与宏 (40)

- `XREDIS_FEATURE_XREDIS_READER`
- `XREDIS_LENGTH_UNKNOWN`
- `XREDIS_READER_ARGUMENT`
- `XREDIS_READER_CALLBACK`
- `XREDIS_READER_ERROR`
- `XREDIS_READER_LEGACY_NAN`
- `XREDIS_READER_LIMIT`
- `XREDIS_READER_MORE`
- `XREDIS_READER_OK`
- `XREDIS_READER_PAUSED`
- `XREDIS_READER_PROTOCOL`
- `XREDIS_READER_READY`
- `XREDIS_READER_STATE`
- `XREDIS_READER_TRUNCATED`
- `XREDIS_READER_YIELD`
- `XREDIS_READ_BEGIN`
- `XREDIS_READ_DATA`
- `XREDIS_READ_END`
- `XREDIS_READ_FAIL`
- `XREDIS_READ_NEXT`
- `XREDIS_READ_PAUSE`
- `XREDIS_REPLY_ARRAY`
- `XREDIS_REPLY_ATTRIBUTE`
- `XREDIS_REPLY_BIG_NUMBER`
- `XREDIS_REPLY_BOOLEAN`
- `XREDIS_REPLY_BULK_ERROR`
- `XREDIS_REPLY_BULK_STRING`
- `XREDIS_REPLY_DOUBLE`
- `XREDIS_REPLY_INTEGER`
- `XREDIS_REPLY_MAP`
- `XREDIS_REPLY_NULL`
- `XREDIS_REPLY_NULL_VALUE`
- `XREDIS_REPLY_PUSH`
- `XREDIS_REPLY_SET`
- `XREDIS_REPLY_SIMPLE_ERROR`
- `XREDIS_REPLY_SIMPLE_STRING`
- `XREDIS_REPLY_STREAMED`
- `XREDIS_REPLY_VERBATIM`
- `XREDIS_RESP2`
- `XREDIS_RESP3`

### 类型 (9)

- `xredisreadaction`
- `xredisreader`
- `xredisreaderconfig`
- `xredisreadererror`
- `xredisreaderinfo`
- `xredisreadevent`
- `xredisreadkind`
- `xredisreadproc`
- `xredisreadstatus`

## `extlibs/xredis/include/xredis/reply.h`

[查看带契约注释的公共头](../../include/xredis/reply.h)

### 函数 (12)

- `xredisReplyDouble`
- `xredisReplyGet`
- `xredisReplyInfo`
- `xredisReplyReaderConfigInit`
- `xredisReplyReaderCreate`
- `xredisReplyReaderDestroy`
- `xredisReplyReaderFeed`
- `xredisReplyReaderFinish`
- `xredisReplyReaderInfo`
- `xredisReplyReaderReset`
- `xredisReplyRelease`
- `xredisReplyRetain`

### 常量与宏 (12)

- `XREDIS_REPLY_ARGUMENT`
- `XREDIS_REPLY_ERROR`
- `XREDIS_REPLY_LIMIT`
- `XREDIS_REPLY_MEMORY`
- `XREDIS_REPLY_MORE`
- `XREDIS_REPLY_NONE`
- `XREDIS_REPLY_OK`
- `XREDIS_REPLY_PROTOCOL`
- `XREDIS_REPLY_READY`
- `XREDIS_REPLY_STATE`
- `XREDIS_REPLY_TRUNCATED`
- `XREDIS_REPLY_YIELD`

### 类型 (6)

- `xredisreplyerror`
- `xredisreplyinfo`
- `xredisreplyreader`
- `xredisreplyreaderinfo`
- `xredisreplystatus`
- `xredisreplyvalue`

## `extlibs/xredis/include/xredis/role.h`

[查看带契约注释的公共头](../../include/xredis/role.h)

### 函数 (4)

- `xredisRoleLimitsInit`
- `xredisRoleMasterNext`
- `xredisRoleRead`
- `xredisRoleReplicaNext`

### 常量与宏 (15)

- `XREDIS_FEATURE_XREDIS_ROLE`
- `XREDIS_ROLE_ERROR`
- `XREDIS_ROLE_LIMIT`
- `XREDIS_ROLE_LINK_CONNECT`
- `XREDIS_ROLE_LINK_CONNECTED`
- `XREDIS_ROLE_LINK_CONNECTING`
- `XREDIS_ROLE_LINK_OTHER`
- `XREDIS_ROLE_LINK_SYNC`
- `XREDIS_ROLE_MALFORMED`
- `XREDIS_ROLE_MASTER`
- `XREDIS_ROLE_OTHER`
- `XREDIS_ROLE_REPLICA`
- `XREDIS_ROLE_SENTINEL`
- `XREDIS_ROLE_SERVER_ERROR`
- `XREDIS_ROLE_VALUE`

### 类型 (7)

- `xredisrolecursor`
- `xredisroleinfo`
- `xredisrolekind`
- `xredisrolelimits`
- `xredisrolelink`
- `xredisrolereplica`
- `xredisrolestatus`

## `extlibs/xredis/include/xredis/sentinel.h`

[查看带契约注释的公共头](../../include/xredis/sentinel.h)

### 函数 (9)

- `xredisSentinelConfigInit`
- `xredisSentinelDiscoveryCancel`
- `xredisSentinelDiscoveryCreate`
- `xredisSentinelDiscoveryDestroy`
- `xredisSentinelDiscoveryNextWake`
- `xredisSentinelDiscoveryPending`
- `xredisSentinelDiscoveryResult`
- `xredisSentinelDiscoveryStep`
- `xredisSentinelDiscoveryTakeClient`

### 常量与宏 (18)

- `XREDIS_FEATURE_XREDIS_SENTINEL`
- `XREDIS_SENTINEL_AUTH_FAILED`
- `XREDIS_SENTINEL_CANCELLED`
- `XREDIS_SENTINEL_DONE`
- `XREDIS_SENTINEL_FACTORY`
- `XREDIS_SENTINEL_LIMIT`
- `XREDIS_SENTINEL_MEMORY`
- `XREDIS_SENTINEL_MORE`
- `XREDIS_SENTINEL_OK`
- `XREDIS_SENTINEL_PENDING`
- `XREDIS_SENTINEL_PROTOCOL`
- `XREDIS_SENTINEL_SERVER_ERROR`
- `XREDIS_SENTINEL_STALE_ROLE`
- `XREDIS_SENTINEL_STEP_ERROR`
- `XREDIS_SENTINEL_TIMEOUT`
- `XREDIS_SENTINEL_UNKNOWN_SERVICE`
- `XREDIS_SENTINEL_UNREACHABLE`
- `XREDIS_SENTINEL_WAIT`

### 类型 (8)

- `xredissentinelconfig`
- `xredissentineldiscovery`
- `xredissentinelendpoint`
- `xredissentinelmasterconnect`
- `xredissentinelresult`
- `xredissentinelseedconnect`
- `xredissentinelstatus`
- `xredissentinelstep`

## `extlibs/xredis/include/xredis/sentinel_pool.h`

[查看带契约注释的公共头](../../include/xredis/sentinel_pool.h)

### 函数 (3)

- `xredisSentinelPoolConfigInit`
- `xredisSentinelPoolCreate`
- `xredisSentinelPoolInfo`

### 常量与宏 (6)

- `XREDIS_FEATURE_XREDIS_SENTINEL_POOL`
- `XREDIS_SENTINEL_POOL_CLOSED`
- `XREDIS_SENTINEL_POOL_CLOSING`
- `XREDIS_SENTINEL_POOL_DISCOVERING`
- `XREDIS_SENTINEL_POOL_ONLINE`
- `XREDIS_SENTINEL_POOL_RECONFIGURING`

### 类型 (4)

- `xredissentinelpoolconfig`
- `xredissentinelpoolinfo`
- `xredissentinelpoolnotify`
- `xredissentinelpoolphase`

## `extlibs/xredis/include/xredis/subscriber.h`

[查看带契约注释的公共头](../../include/xredis/subscriber.h)

### 函数 (5)

- `xredisSubscriberCommand`
- `xredisSubscriberCommandAsync`
- `xredisSubscriberConfigInit`
- `xredisSubscriberInfo`
- `xredisSubscriberStart`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_SUBSCRIBER`

### 类型 (3)

- `xredissubscriberconfig`
- `xredissubscriberinfo`
- `xredissubscriberresult`

## `extlibs/xredis/include/xredis/subscriber_notify.h`

[查看带契约注释的公共头](../../include/xredis/subscriber_notify.h)

### 函数 (2)

- `xredisSubscriberReadable`
- `xredisSubscriberTakeBatch`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY`

## `extlibs/xredis/include/xredis/subscription_session.h`

[查看带契约注释的公共头](../../include/xredis/subscription_session.h)

### 函数 (9)

- `xredisSubscriptionSessionApply`
- `xredisSubscriptionSessionClose`
- `xredisSubscriptionSessionClosed`
- `xredisSubscriptionSessionConfigInit`
- `xredisSubscriptionSessionCreate`
- `xredisSubscriptionSessionCurrent`
- `xredisSubscriptionSessionInfo`
- `xredisSubscriptionSessionRelease`
- `xredisSubscriptionSessionRetain`

### 常量与宏 (10)

- `XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS`
- `XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION`
- `XREDIS_SESSION_BACKOFF`
- `XREDIS_SESSION_CLOSED`
- `XREDIS_SESSION_CLOSING`
- `XREDIS_SESSION_CONNECTING`
- `XREDIS_SESSION_DRAINING`
- `XREDIS_SESSION_ONLINE`
- `XREDIS_SESSION_RESTORING`
- `XREDIS_SESSION_UPDATING`

### 类型 (11)

- `xredissubscriptionreplayconfig`
- `xredissubscriptions`
- `xredissubscriptionsession`
- `xredissubscriptionsessionclosed`
- `xredissubscriptionsessionconfig`
- `xredissubscriptionsessionconnect`
- `xredissubscriptionsessioncurrent`
- `xredissubscriptionsessiondrop`
- `xredissubscriptionsessioninfo`
- `xredissubscriptionsessionnotify`
- `xredissubscriptionsessionstate`

## `extlibs/xredis/include/xredis/subscriptions.h`

[查看带契约注释的公共头](../../include/xredis/subscriptions.h)

### 函数 (18)

- `xredisSubscriptionReplayCancel`
- `xredisSubscriptionReplayConfigInit`
- `xredisSubscriptionReplayCreate`
- `xredisSubscriptionReplayDestroy`
- `xredisSubscriptionReplayPending`
- `xredisSubscriptionReplayResult`
- `xredisSubscriptionReplayStep`
- `xredisSubscriptionSnapshotAt`
- `xredisSubscriptionSnapshotInfo`
- `xredisSubscriptionSnapshotRelease`
- `xredisSubscriptionSnapshotRetain`
- `xredisSubscriptionsApply`
- `xredisSubscriptionsConfigInit`
- `xredisSubscriptionsCreate`
- `xredisSubscriptionsInfo`
- `xredisSubscriptionsRelease`
- `xredisSubscriptionsRetain`
- `xredisSubscriptionsSnapshot`

### 常量与宏 (14)

- `XREDIS_SUBSCRIPTIONS_CANCELLED`
- `XREDIS_SUBSCRIPTIONS_CLOSED`
- `XREDIS_SUBSCRIPTIONS_DONE`
- `XREDIS_SUBSCRIPTIONS_LIMIT`
- `XREDIS_SUBSCRIPTIONS_MEMORY`
- `XREDIS_SUBSCRIPTIONS_MORE`
- `XREDIS_SUBSCRIPTIONS_OK`
- `XREDIS_SUBSCRIPTIONS_PENDING`
- `XREDIS_SUBSCRIPTIONS_PROTOCOL`
- `XREDIS_SUBSCRIPTIONS_SERVER_ERROR`
- `XREDIS_SUBSCRIPTIONS_STALE`
- `XREDIS_SUBSCRIPTIONS_STEP_ERROR`
- `XREDIS_SUBSCRIPTIONS_TIMEOUT`
- `XREDIS_SUBSCRIPTIONS_WAIT`

### 类型 (8)

- `xredissubscriptionreplay`
- `xredissubscriptionreplayresult`
- `xredissubscriptionsconfig`
- `xredissubscriptionsinfo`
- `xredissubscriptionsnapshot`
- `xredissubscriptionstatus`
- `xredissubscriptionstep`
- `xredissubscriptionview`

## `extlibs/xredis/include/xredis/tls.h`

[查看带契约注释的公共头](../../include/xredis/tls.h)

### 函数 (4)

- `xredisClientTlsConfigInit`
- `xredisClientTlsConnect`
- `xredisClientTlsDial`
- `xredisClientTlsDialAddresses`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_TLS`

### 类型 (1)

- `xredisclienttlsconfig`

## `extlibs/xredis/include/xredis/tls_auth.h`

[查看带契约注释的公共头](../../include/xredis/tls_auth.h)

### 函数 (3)

- `xredisClientTlsAuthConnectV1`
- `xredisClientTlsAuthDialAddressesV1`
- `xredisClientTlsAuthDialV1`

### 常量与宏 (1)

- `XREDIS_FEATURE_XREDIS_TLS_AUTH`

## `extlibs/xredis/include/xredis/tracking.h`

[查看带契约注释的公共头](../../include/xredis/tracking.h)

### 函数 (10)

- `xredisClientCachingCall`
- `xredisClientCachingCallAsync`
- `xredisClientTrackingCall`
- `xredisClientTrackingCallAsync`
- `xredisClientTrackingConfigureAsync`
- `xredisClientTrackingInfo`
- `xredisClientTrackingObserve`
- `xredisClientTrackingTake`
- `xredisTrackingConfigInit`
- `xredisTrackingEventRead`

### 常量与宏 (15)

- `XREDIS_FEATURE_XREDIS_TRACKING`
- `XREDIS_TRACKING_BCAST`
- `XREDIS_TRACKING_ERROR`
- `XREDIS_TRACKING_EVENT`
- `XREDIS_TRACKING_FLUSH`
- `XREDIS_TRACKING_KEYS`
- `XREDIS_TRACKING_MALFORMED`
- `XREDIS_TRACKING_NOLOOP`
- `XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT`
- `XREDIS_TRACKING_OPTIN`
- `XREDIS_TRACKING_OPTOUT`
- `XREDIS_TRACKING_OTHER`
- `XREDIS_TRACKING_PREFIXES_MAX`
- `XREDIS_TRACKING_REDIRECT`
- `XREDIS_TRACKING_REDIRECT_BROKEN`

### 类型 (8)

- `xrediscachingresult`
- `xredistrackingconfig`
- `xredistrackingevent`
- `xredistrackingeventkind`
- `xredistrackinginfo`
- `xredistrackingpush`
- `xredistrackingresult`
- `xredistrackingstatus`

## `extlibs/xredis/include/xredis/transaction.h`

[查看带契约注释的公共头](../../include/xredis/transaction.h)

### 函数 (3)

- `xredisClientTransaction`
- `xredisClientTransactionAsync`
- `xredisClientTransactionConfigInit`

### 常量与宏 (6)

- `XREDIS_TRANSACTION_ABORTED`
- `XREDIS_TRANSACTION_COMMANDS_DEFAULT`
- `XREDIS_TRANSACTION_COMMANDS_MAX`
- `XREDIS_TRANSACTION_EXECUTED`
- `XREDIS_TRANSACTION_MAY_HAVE_EXECUTED`
- `XREDIS_TRANSACTION_NOT_EXECUTED`

### 类型 (2)

- `xredistransactionexecution`
- `xredistransactionresult`

## `extlibs/xredis/include/xredis/watch.h`

[查看带契约注释的公共头](../../include/xredis/watch.h)

### 函数 (11)

- `xredisClientWatch`
- `xredisClientWatchAsync`
- `xredisWatchAbort`
- `xredisWatchCall`
- `xredisWatchCallAsync`
- `xredisWatchClosed`
- `xredisWatchCommit`
- `xredisWatchCommitAsync`
- `xredisWatchConfigInit`
- `xredisWatchRelease`
- `xredisWatchRetain`

### 类型 (3)

- `xrediswatch`
- `xrediswatchclosedresult`
- `xrediswatchresult`
