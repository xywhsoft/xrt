# SSH Transport State

`ssh_transport_state` 是 packet codec 与 KEX/auth/connection 驱动之间的纯状态层。它不拥有
socket、密钥、KEXINIT payload、时钟或收发缓冲，也不执行密码操作。同步、future、协程和自定义
事件循环都可以在消息可靠入队或认证解包后提交同一套状态。

## 状态边界

- `xrtSshTransportStateInit` 从 identification 阶段开始；双方 identification 分别提交。
- `xrtSshTransportKexInitCheck/Commit` 记录方向性 KEXINIT 和 guessed-packet 声明。
- `xrtSshTransportKexConfigure` 借用双方 KEXINIT 与协商结果，复制本代 KEX 方法额度。
- `xrtSshTransportMessageCheck/Commit` 处理普通 transport、KEX 方法和应用消息。
- `xrtSshTransportNewKeysCheck/Commit` 在本方向方法额度归零后发布密钥切换动作。
- `xrtSshTransportCanApplication` 分方向判断 rekey 期间是否仍可收发应用消息。

本端发送必须先 `Check`，只有 packet 已经被发送队列可靠接受后才能 `Commit`。对端消息只有在 packet
完整认证并解析出消息号后才能提交。状态对象只能由一个执行流推进；网络层负责串行化同一连接。

## KEX 方法规则

方法消息号 `30..49` 由 `xsshtransportkexrules` 按方向记录精确次数。`xrtSshTransportKexRuleSet`
允许当前和未来 KEX 方法组合任意消息额度。Curve25519/ECDH 驱动分别把 INIT 与 REPLY 的单次额度
配置到 client/server 方向；状态核心不会仅为便利配置而强制依赖某个 KEX 方法模块。额度在提交时
递减，重复消息、错方向消息以及额度未归零时的 NEWKEYS 都会失败。

若 `first_kex_packet_follows` 为真，KEXINIT 后第一包方法消息可在协商提交前登记。配置阶段通过
`xrtSshKexGuessSkip` 判定：错误猜测只计入 packet 序列，不消费真实方法额度；正确猜测则消费一次。
状态层不保存 KEXINIT 视图，调用方仍负责保留初次报文以计算 exchange hash。

## Strict KEX

首次协商 strict-kex 后，状态机会追溯检查双方 KEXINIT 都是各自第一个 packet，并拒绝初始 KEX
期间的非 KEX 消息和 uint32 序列回绕。每次初始 KEX 或后续 rekey 的 NEWKEYS 都返回：

- `XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS`：切换对应 codec 方向的新密钥。
- `XSSH_TRANSPORT_ACTION_RESET_SEQUENCE`：strict-kex 要求重置对应方向序列号。
- `XSSH_TRANSPORT_ACTION_KEX_COMPLETE`：双方 NEWKEYS 均已提交，本代 KEX 完成。

NEWKEYS 本身必须用旧密钥处理。发送端在 NEWKEYS 可靠入队后提交状态，再切换 write codec；接收端
在旧 read codec 验证 NEWKEYS 后提交状态，再切换 read codec。动作所属方向由提交参数确定。

## Rekey 与 EXT_INFO

任一方可在 OPEN 状态提交 KEXINIT 发起 rekey。某方向提交 KEXINIT 后，该方向在 NEWKEYS 前只能
发送 RFC 4253 允许的 transport/KEX 消息；另一方向在收到自己的 KEXINIT 边界前仍可处理线路上
在途应用数据。角色在整个连接中不改变，strict 和首次 EXT_INFO 能力也不会重新协商。

RFC 8308 的第一次 EXT_INFO 只允许作为本方向首次 NEWKEYS 后的下一包。server 还可以发送第二次
EXT_INFO；状态随后只接受 `xrtSshTransportAuthSuccessCommit`，从而保证它紧邻在
USERAUTH_SUCCESS 前。client 方向不能使用第二次机会。消息内容仍由 `ssh_transport_message` 解析，
认证成功报文仍由 `ssh_auth_message` 构建。

```c
xsshtransportstate state;
xsshtransportkexrules rules;
uint32 actions;

xrtSshTransportStateInit(&state, XSSH_ROLE_CLIENT);
xrtSshTransportKexRulesInit(&rules);
xrtSshTransportKexRuleSet(
	&rules,
	XSSH_TRANSPORT_LOCAL,
	XSSH_MSG_KEX_ECDH_INIT,
	1u
);

/* packet 写入并被发送队列接受后再提交。 */
if ( xrtSshTransportNewKeysCheck(
	&state,
	XSSH_TRANSPORT_LOCAL
) == XSSH_OK ) {
	/* enqueue SSH_MSG_NEWKEYS with old write keys */
	xrtSshTransportNewKeysCommit(
		&state,
		XSSH_TRANSPORT_LOCAL,
		&actions
	);
}
```
