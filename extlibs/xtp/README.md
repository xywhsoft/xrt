# xtp

纯 C XTP 3.0 扩展。当前已实现 P1 Codec 与 P2 Reader：32 字节固定头、两类命令、保留顺序的 table/array、批量编码、完整帧借用解码、STREAM/RECORD 增量输入和有界异常包丢弃。不兼容 1.0/2.0。

Codec 不分配；Reader 对完整连续帧直接借用，仅为跨输入帧分配组装缓冲。无网络、线程和全局状态。字节流损坏的帧边界不可推测恢复，RECORD 可在可信记录边界恢复；可辨认长度的坏帧直接跳过 Body。配置始终限制工作量、缓冲及增长峰值、坏帧数和接收期限。

`xtpParamAt` 对 array/table 均按发送顺序 O(1) 读取；`xtpParamFind` 仅对 table 按 key 顺序查找。去重校验只排序临时索引，不改变字段顺序。Body 和值不做转换。

```sh
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --cflag=-O2
python tools/package.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --kind static --verify
```

选择 `XTP_MODULE_XTP_CODEC` 或 `XTP_MODULE_XTP_READER`，声明入口 `<xtp.h>`。扩展单头只包含本库，先提供同一份 XRT；单个编译单元定义 `XTP_IMPLEMENTATION`。

Reader 回调返回 PAUSE 表示尚未消费该消息；完整借用帧仍在调用者的未消费后缀，已组装帧由 Reader 保留。调用 Resume 后重交未消费后缀；AGAIN 按 Used 继续，空 Input 不代表 EOF。已有网络后端只需传入字节和边界，见 [增量后端示例](examples/backend/main.c)。

完整协议、交付语义和阶段验收见 [设计](../../docs/design/xtp3.md) 与 [API 方案](../../docs/design/xtp3-api.md)。[Codec 示例](examples/codec/main.c) 可直接运行；[覆盖与进度](docs/coverage.md) 明确尚未完成的阶段。
