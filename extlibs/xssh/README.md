# xssh

`xssh` 是 XRT 的原生 SSH-2 client 扩展库，目标是提供可嵌入的 SSH 连接、认证、channel、端口转发和 XUI Terminal 前端能力。

当前实现仍在按 `xssh-spec.md` 推进，已完成 wire/packet/KEX/auth/channel/known_hosts 的核心消息层、纯内存 mock transcript、mock runtime、端口转发消息层、xnetstream TCP + SSH identification exchange + curve25519/Ed25519/AES-GCM KEX/NEWKEYS、加密后的 password USERAUTH、signer callback publickey USERAUTH、OpenSSH v1 未加密 Ed25519 私钥认证、keyboard-interactive USERAUTH、基础 session/exec/subsystem channel transport、direct-tcpip/forwarded-tcpip/tcpip-forward 数据与控制面、`xssh_forward` 单连接本地监听数据泵、基础 CLI mock 示例和 XUI2 Terminal mock 示例。OpenSSH 互操作、加密私钥/RSA/ECDSA/agent、交互式 shell 验证和多连接转发管理仍在开发中。

## 构建与测试

在 Windows PowerShell 中执行：

```bat
cd /d D:\Git\xrt\extlibs\xssh
build_test.bat
```

当前 `build_test.bat` 会编译并运行：

- `xssh_wire_test`
- `xssh_packet_test`
- `xssh_crypto_test`
- `xssh_kex_test`
- `xssh_transport_test`
- `xssh_auth_test`
- `xssh_channel_test`
- `xssh_known_hosts_test`
- `xssh_mock_server_test`
- `xssh_runtime_test`
- `xssh_interop_test`，默认无 `XSSH_LIVE_HOST` 时跳过
- `xssh_exec --mock`
- `xssh_shell --mock`
- `xssh_forward --mock`
- `xssh_forward --mock --self-test`，验证本地 listener、客户端 socket、direct-tcpip mock channel 和双向数据泵

默认测试只使用本地内存夹具、mock runtime 和本地 SSH socket server，不依赖外部 SSH 服务；本地 socket server 覆盖 KEX/NEWKEYS、first_kex_packet_follows 错误猜测跳过、AES-128-GCM/AES-256-GCM 加密 packet 读写、password success/failure、AES-GCM bad tag/坏 packet_length/半包关闭/乱序 channel data 异常注入、keepalive/ignore/debug、服务端主动 global request fallback、session channel open、多 session channel 事件分发、env success/failure 与 `CHANNEL_FAILURE` result、PTY/shell echo、subsystem raw byte channel、exec stdout/stderr/exit-status/exit-signal/EOF/CLOSE、主动 DISCONNECT、tcpip-forward/direct-tcpip/cancel-tcpip-forward 控制面、direct-tcpip reject，以及 direct-tcpip 加密数据面 roundtrip。

`xssh_interop_test` 和 CLI/XUI 示例在 SSH connect、channel、pty、shell、exec、forward 失败时会打印 `stage/error/disconnect/banner/kex/hostkey/c2s/s2c/mac/auth/fingerprint/text`，用于定位真实服务端互操作问题；密码和私钥内容不会写入日志。

XUI2 Terminal 示例依赖 `D:\git\xge\dev\xui2`，可单独构建和 smoke：

```bat
cd /d D:\Git\xrt\extlibs\xssh\examples\xssh_terminal_xui2
build.bat
cd /d D:\Git\xrt\extlibs\xssh
bin\xssh_terminal_xui2.exe --mock --frames 5
```

如果 XUI2 checkout 不在默认路径，可先设置 `XUI2_DIR` 环境变量。该示例直接使用 `xuiTerminalWrite`、`xuiTerminalSetInputCallback` 和 `xuiTerminalSetResizeCallback` 接入原生 `xssh` channel，不调用 XUI2 现有 OpenSSH-backed adapter。

live 示例建议使用 `--known-hosts PATH --accept-new` 指向临时 known_hosts 文件做首次验证，避免污染用户默认 SSH 配置。

## 已支持的协议层能力

- SSH wire 类型：`byte`、`boolean`、`uint32`、`uint64`、`string`、`name-list`、signed `mpint`、banner。
- packet codec：plain packet、AES-GCM AEAD packet、plain 到 AES-GCM 的 codec 切换。
- KEX：KEXINIT 构建/解析、算法协商、first_kex_packet_follows 错误猜测跳过、curve25519 shared secret、curve25519 exchange hash、SHA-256 key derivation、真实 NEWKEYS key switch；AES-128-GCM 和 AES-256-GCM 均有真实 transport 协商覆盖。
- Host key：`ssh-ed25519` key blob、signature blob、签名验证。
- UserAuth 消息：service、none、password、publickey probe、publickey signed request、keyboard-interactive request/challenge/response、failure/success/banner/PK_OK；真实 transport 已接入 password success/failure、signer callback publickey success、OpenSSH v1 未加密 Ed25519 私钥 success 和 keyboard-interactive success。
- `xsshresult.sAuthMethods` 会在 USERAUTH_FAILURE 时保存服务端 allowed methods，例如 `publickey,password`。
- Channel 消息：session open、direct-tcpip、forwarded-tcpip、shell、exec、subsystem、PTY、PTY terminal modes、env、window-change、signal、break、data、extended-data、EOF、close、exit-status、exit-signal；runtime 已保存 exit-status/exit-signal 并提供查询，且已覆盖同一连接上的多 session channel 数据分发。
- `xsshChannelWrite` 在真实 transport 下支持按 remote window / max packet 分包；窗口耗尽时会把剩余数据放入固定容量队列，并在收到 WINDOW_ADJUST 后自动 flush，`pWritten` 返回本次已接受字节数。
- `xsshChannelClose` 在真实 transport 下采用非阻塞双向 close：本端 close 先发送 `CHANNEL_CLOSE`，收到远端 `CHANNEL_CLOSE` 后才释放 channel slot 并产生最终 close event；远端先 close 时会自动 best-effort 回包。
- Global request：`tcpip-forward`、`cancel-tcpip-forward`、`keepalive@openssh.com`、request success/failure；keepalive 会把 REQUEST_FAILURE 视为有效存活回复，服务端主动 `GLOBAL_REQUEST` 若要求回复会返回 `REQUEST_FAILURE` 以保持连接兼容。
- forwarded-tcpip runtime：服务端主动 `CHANNEL_OPEN` 接收、open confirmation、channel metadata 保存、`XSSH_EVENT_CHANNEL_OPEN` 事件和后续数据读取。
- known_hosts：普通 host、`[host]:port`、OpenSSH `|1|salt|hash` hashed host、同 host 多 key type 筛选、strict unknown 拒绝、accept-new 追加记录、mismatch 拒绝、host key SHA256/base64 指纹展示和兼容用 MD5 hex 指纹展示。
- mock runtime：`xsshConnect`、`xsshDisconnect`、`xsshPoll`、`xsshNextEvent`、session channel、exec/shell mock 数据流。
- real transport seed：xnetstream TCP connect、client/server SSH identification exchange、curve25519/Ed25519/AES-GCM KEX/NEWKEYS、加密 USERAUTH service request、password auth、keyboard-interactive auth、keepalive/ignore/debug、session channel open、PTY/shell request、exec request、subsystem request/raw byte channel、channel data pump、direct-tcpip 控制面与数据面、tcpip-forward/cancel-tcpip-forward 控制面、真实 server banner 与协商算法 result 填充。
- forwarding example：`xssh_forward` live 模式会监听本地端口，accept 一个本地 TCP client，打开 direct-tcpip channel，并在同一线程内双向搬运数据；mock `--self-test` 会启动本地客户端线程验证这条路径。
- XUI2 mock 示例：XGE + XUI Terminal 渲染、输入回调、resize 回调和 xssh event drain。

## 安全默认值

- 默认 host key policy 是 `strict`，unknown host 会拒绝。
- `accept-new` 必须显式配置，只追加新主机记录，不静默覆盖 mismatch；写入前会创建 known_hosts 父目录，并用短时 `.lock` 独占写保护和锁内重读避免并发重复追加，成功追加后 `xsshresult.bHostKeyAcceptedNew` 会置位。
- 同一个 host 可以同时存在多种 host key 类型；`xssh` 只把同 host 且同算法的记录用于 mismatch 判断，不同算法记录不会阻止当前算法的 strict/accept-new 流程。
- `insecure accept any` 必须显式配置，`xsshresult.bHostKeyInsecure` 会标记该风险。
- `xsshresult.sHostKeyFingerprint` 使用 OpenSSH 风格 `SHA256:<base64-no-padding>`，认证失败前也会尽量填充，便于排查 host key 问题；MD5 hex 指纹仅用于兼容展示，不参与安全决策。
- 默认禁用 `ssh-rsa`、`hmac-sha1`、CBC/3DES。
- `xsshConnect` 会钳制超大的 packet/window 配置，认证阶段有服务端包数和 keyboard-interactive 轮数上限。
- 密码、私钥、session key、签名中间数据不得写入日志。
- 密钥和签名中间缓冲在释放或测试结束前使用 `xsshSecureZero` 清理。

## 最小消息层示例

下面示例演示如何构建一个 `exec` channel request。真实 session/exec runtime 已接入，转发、rekey 和更多认证方式仍在开发中。

```c
#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

int main(void)
{
	uint8 buf[256];
	xsshwriter wr;
	xsshchannelrequestmsg req;

	xsshWriterInit(&wr, buf, sizeof(buf));
	if ( xsshChannelRequestExecWrite(&wr, 7u, TRUE, "uname -a") != XSSH_OK ) {
		return 1;
	}
	if ( xsshChannelRequestRead(buf, wr.iLen, &req) != XSSH_OK ) {
		return 1;
	}
	return 0;
}
```

## 当前限制

- `xsshConnect` 已支持真实 xnetstream TCP、SSH identification exchange、curve25519/Ed25519/AES-GCM KEX/NEWKEYS、known_hosts、password USERAUTH、signer callback publickey USERAUTH、OpenSSH v1 未加密 Ed25519 私钥和 keyboard-interactive；加密私钥、RSA/ECDSA 私钥、agent 仍在开发中。
- `xsshOpenSessionChannel`、`xsshChannelRequestPty`、`xsshChannelRequestShell`、`xsshChannelRequestExec`、`xsshChannelRequestSubsystem`、`xsshPoll` / `xsshNextEvent` 已接入真实 session shell/exec/subsystem 基础路径，`xsshPtyModeSet` 可显式编码 RFC 4254 terminal modes，并已覆盖多 session channel 分发；多连接转发管理仍在开发中。
- 尚未实现加密 OpenSSH private key、RSA/ECDSA private key signing、ssh-agent/Pageant、OpenSSH remote forwarding live 验证、多连接转发管理、rekey。
- 已提供 `xssh_exec` / `xssh_shell` / `xssh_forward` / `xssh_terminal_xui2` mock 示例；`xssh_terminal_xui2` 的 live shell 仍待真实 OpenSSH 环境手工验证。
