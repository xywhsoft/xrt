# XRT 缺陷修补交付记录

日期：2026-09-05。基线：`9bfc3c0f82e91dacc93d654f27fc72a21f1e784f`。

本次只修补 XRT 本身及其测试、仓库 API 文档、生成单头和 CI。未修改官网、`extlibs/` 或工作区已有的 `examples/` 改动；未提交 Git commit。

## 已实施

| 项目 | 最小修补及边界 |
| --- | --- |
| 协程投递预算 | 默认最多排队 1024 个用户投递；新增 `xrtCoSchedCreateLimit(n)`，0 使用默认值，`SIZE_MAX` 可显式解除实际限额。队满返回 `XERR_AGAIN`，拒绝时不接管 Owned 数据；内部无分配唤醒链不受此限额影响。 |
| TLS 签名声明 | ClientHello 按已编入的验签后端过滤签名列表。不再声明 Ed448；TLS 1.3 不声明缺少后端的 P-521。TLS 1.2-only 保留已有后端可执行的 SHA-512/ECDSA 组合。没有新增算法或后端抽象。 |
| TLS 监听器预算 | 默认同时握手上限从 1024 调整为 128；已完成连接的 accept 队列默认仍为 1024。已有显式配置仍有效。 |
| TLS 1.3 自动 KeyUpdate | 客户端和服务端应用写入达到边界时，预留旧 epoch 最后一条记录给 KeyUpdate，再用新密钥发送数据。复用原有事务实现；无空间不切换密钥，只有更新记录能排队时允许先切换并对应用数据返回 AGAIN。TLS 1.2 和底层记录 API 保留硬上限拒绝；纯控制消息发送仍需显式更新。 |
| RSA 私钥盲化 | CRT 和完整指数路径均使用新的随机可逆基底，解除盲化后再次公钥复核。拒绝 0、1、越界和不可逆因子；最多尝试 64 次，随机失败不回退到未盲化计算，不发布输出。只增加私钥模块的安全随机依赖，不影响纯公钥模块。 |
| 票据 OOM 回归 | 实际票据事务实现无需改动。修正测试对分配调用数、底层保留 slab 与逻辑存活块的混淆；分配器状态改为静态寿命，覆盖进程退出后的线程缓存析构。只在服务端测试聚合中启用 memory_debug。 |
| 有状态 fuzz | 新增真实 PSK+DHE 握手后的操作序列目标，覆盖分片、背压、KeyUpdate、票据、有效 AEAD 内的畸形握手、密钥边界、关闭与 EOF；检查终态、队列预算和逐轮逻辑泄漏。代码仅在测试目标中。 |
| 独立互操作 | 新增本地 OpenSSL/Python ssl 双向矩阵与 Python 大整数 RSA 差分。证书和测试密钥临时生成，不访问公网。工具和 Python 依赖不进入运行库。 |

互操作另外发现并修复了三个小型协议缺陷：

- TLS 1.2 首次握手缺少安全重协商的空绑定标记，导致 OpenSSL 默认拒绝 XRT 服务端。现在处理空 RI 扩展及 `0x00FF` SCSV，拒绝非空初始绑定；仍不实现重新协商。同时修正最大 ALPN 的扩展缓冲预算，并增加 255 字节 ALPN 回归。依据：[RFC 5746](https://www.rfc-editor.org/rfc/rfc5746.html)。
- TLS 1.3 客户端误拒绝 HRR 与最终 ServerHello 之间的兼容 CCS，两端也误拒绝重复 CCS。现在仅在允许的握手窗口丢弃合法明文单字节 CCS，仍拒绝畸形或受保护的 CCS。依据：[RFC 8446 第 5 节](https://www.rfc-editor.org/rfc/rfc8446.html#section-5)。
- 服务端误要求 HRR 后第二次 ClientHello 的 padding 保持原样。只对 padding 增删和长度变化作例外，其余固定字段及稳定扩展仍严格检查。依据：[RFC 8446 第 4.1.2 节](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.1.2)。

## 体积实测

源码口径为 `src/**/*.c,h` 与 `include/xrt/**/*.c,h` 的物理行，包含空行和注释，不重复计入生成的 single 文件。

| 运行库源码 | 净增行数 |
| --- | ---: |
| 协程预算及公开声明 | 33 |
| RSA 加固及内部声明 | 624 |
| TLS 修补及内部声明 | 118 |
| 生成的 feature 选择头 | 3 |
| 合计 | **778** |

总行数从 306,790 增至 307,568，约 **0.254%**。RSA 新文件占 606 行，其中包括带完整 MIT 声明的 BearSSL 固定迭代模除法移植。生成的 `single/xrt.h` 文件增加约 22.9 KiB；这是源码文本增量，不是程序运行体积。

二进制口径：Windows x64，GCC 16.1.0，`-O2`，同一组公开模块选择，编译生成单头为单个对象，比较 `.text + .data`。不是最终应用 EXE 的通用增量，也不是所有可选模块的完整组合。

| 配置 | 修补前代码/数据 | 修补后代码/数据 | 增量 |
| --- | ---: | ---: | ---: |
| `core` | 13,812 B | 13,812 B | **0 B** |
| `concurrency` | 105,716 B | 105,924 B | **208 B** |
| `security_transport` | 468,696 B | 468,760 B | **64 B** |
| 下述含 RSA 认证的 TLS 组合 | 807,420 B | 812,728 B | **5,308 B（5.18 KiB，0.66%）** |

最后一行的精确模块选择：

```text
tls_stream,tls_client_resume,tls_server_resume,tls_schedule_sha256,tls_key_exchange_x25519,tls_record_aes,tls_identity_rsa
```

`security_transport` 是仓库已有的基础安全传输 profile，不包含完整客户端/服务端证书握手，不能拿它的 64 B 代表完整 TLS 修补。以上四组 `.data`、`.bss` 均未增加。最后一组对象文件的磁盘大小为 1,020,678 → 1,026,479 B，增加 5,801 B，包含对象格式等非运行开销。

内存边界也有变化：

- 调度器新增一个 `size_t` 预算字段，x64 为 8 B。
- 公共 TLS 会话本体新增一个函数指针，x64 为 8 B；角色结构删除不再需要的 CCS 已见标志。
- RSA 盲化无新堆分配，无共享因子缓存；工作数组共 9,568 B，返回前清零。GCC x64 `-O2 -fstack-usage` 测得新增包装函数栈帧 **9,728 B（9.5 KiB）**，不包含它调用的原有私钥核心栈帧。小栈调用方应把这部分新增临时栈计入预算。

实测低于此前约定的 1,500 行运行库代码、16 KiB 代码体积预算。测试、脚本、文档及 CI 的增长不进入该运行库组合；没有新增生产功能模块。

## 验证记录

本地已通过：

- GCC `-Wall -Wextra -Werror -O2`：TLS 客户端/服务端 9 个模块化测试，包括 TLS 1.2、HRR/CCS、自动更新、票据、限额及 OOM。
- 流适配、监听器、Select/IOCP 共 25 个模块化测试，包括背压、慢读、截断、超时、恢复和中止。
- TLS、协程、RSA 相关 8 个单头测试；有状态 fuzz 另外以模块化和单头两种方式各执行 200 条确定性随机操作轨迹。
- 协程预算、多生产者、内部唤醒和所有权回归；RSA 私钥、PSS、PKCS#1、盲化拒绝采样、随机失败和小模数穷举回归。
- OpenSSL 3.0.16 互操作 46 组：TLS 1.3 的 RSA/P-256/P-384/Ed25519，TLS 1.2 的 RSA/P-256/P-384，各三个 AEAD 套件、两种角色；覆盖 ALPN、分片、双向 20 KB 数据、TLS 1.3 KeyUpdate、close_notify，以及双向 HRR/错误域名拒绝。
- RSA Python 大整数差分 180 次私钥运算：1024/2048/3072/4096/8192 位，公开指数 3/65537，CRT/完整指数、输入输出别名、拒绝越界及失败原子性。
- RSA 私钥、协程、TLS session/client resume/server resume 的裁剪依赖正反检查。
- 57 个构建/文档/发布状态/ABI 解析/体积工具单元测试；生成单头与 API 参考一致性、协程/TLS API 文档符号覆盖、发布成熟度检查及 diff 空白检查。

主要复验命令：

```powershell
python tools/build.py --compiler gcc --suite tls_client_tests,tls_server_tests --no-single --no-examples --jobs 4 --cflag=-O2
python tools/build.py --compiler gcc --suite tls_stream_tests,tls_stream_listener_tests,tls_stream_iocp_tests --no-single --no-examples --jobs 4 --cflag=-O2
python tools/build.py --compiler gcc --suite tls_state_fuzz_tests,coroutine_scheduler,crypto_rsa_private,crypto_rsa_pss_sign,crypto_rsa_pkcs1_sign --no-examples --jobs 4 --cflag=-O2
python tools/test_tls_interop.py --compiler gcc
python tools/test_rsa_blinding.py --compiler gcc
python tools/measure_size.py --compiler gcc --arch x64 --profiles core,concurrency,security_transport --kind single --report out/size-audit-after.json
```

体积和测试日志保留在 `out/size-audit-9bfc3c0f.json`、`out/size-audit-after.json`、`out/audit-full-tls-size.log`、`out/audit-*.log`，属于未跟踪的本地验证产物。

## 未宣称已验证的部分

本机未找到 Clang，因此没有在本地执行 libFuzzer + ASan/UBSan；Linux/macOS CI 也尚未由本次本地操作触发。新的有状态目标已接入现有 fuzz 门禁，独立 TLS/RSA 工具已加入 native CI；`cryptography==50.0.0` 仅是测试依赖。

本次不引入新的密码算法、TLS 后端框架、PKI 管理系统或线上吊销服务，不把这些扩展当作缺陷补丁。RSA 随机盲化和差分测试也不等于独立侧信道审计或安全认证。
