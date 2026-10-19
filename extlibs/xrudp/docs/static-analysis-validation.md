# 当前运行时的静态分析复核

2026-10-19：从 Git `3d55ec41e13f0884ffe840d44a57a0189d9310f3` 归档 XRUDP 的 `src`、`include`，隔离分析全部 20 个 C 翻译单元。XRT 声明及 shim 来自已验证产品 SDK，核对其单头与该提交一致，未使用工作区其他任务的核心文件。

Linux Clang 21 的实际命令逐项保存在 `out/xrudp-static-audit/3d55-clang21-r1/analysis.json`。主要选项为 `--analyze -std=gnu11 -DXRT_MODULE_ALL=1 -DXRUDP_MODULE_ALL=1 -Xanalyzer -analyzer-output=plist -Xanalyzer -analyzer-checker=core,unix,deadcode,security`，加产品 SDK 与隔离 XRUDP 的 include 路径。每个单元保留完整日志和 plist；源文件、声明、日志和报告的 SHA256 均记录。

20 个分析进程均正常退出。结果并非“零告警”：包含两条需要状态不变量复核的候选路径，以及 236 条通用安全 API 建议。原报告未屏蔽、删除或改写；复核说明和报告哈希在同目录 `review.json`。

| 候选路径 | 当前源码的实际约束 | 结论边界 |
| --- | --- | --- |
| `rudp_session.c:1527`，空 `m` 访问 Deficit | emit 在外部 Send 返回后初始化 `ref->Stream=NO_STREAM`；只有 data/open/reset/stop/streamGrant 会改为 selected，这些条件均要求非空 m。末尾 `ref->Stream!=NO_STREAM` 因而要求非空 m。opaque 元数据不对调用者暴露，Send 契约禁止重入或销毁 Session | 报告路径丢失 emit 后置条件；当前合法 API 状态不能沿该路径产生空引用，未确认运行时缺陷 |
| `rudp_sent.c:302 -> 46`，NULL events 传给 memset | Pending 始终等于有效窗口中 Status==0 的数量；Record/finish/trim 同步维护。events=NULL 要求 capacity=0，Pending>capacity 会提前背压；Pending=0 时不会进入 finish。窗口是 opaque 且要求串行调用 | 报告假设的 Pending=0、仍有 Status==0 条目相互矛盾；`test_sent.c` 已有 NULL/zero 空窗口、pending 背压及独立窗口数量不变量回归 |

236 条 API 建议包括 memcpy 119、memset 106、memmove 11，去重后为 179 个位置（核心声明内联函数会在各翻译单元重复报告）。这些报告建议使用其他带边界参数的 API，本身没有提供越界路径。复核结合固定包缓冲、输入长度检查、ACK/控制帧预留，以及既有恶意线路、MTU 回退和状态模型证据；没有为消除此类建议而替换运行时 API。

本次没有修改运行时、SDK 或任何长稳来源。该分析按翻译单元工作，对 opaque 状态不变量、外部函数和回调需要人工复核；它不构成完整的跨线程、密码学或全程序安全证明。实体 Android、双独立 NAT、系统休眠恢复与实际 72 小时验收继续按验收矩阵执行。
