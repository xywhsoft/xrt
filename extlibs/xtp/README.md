# xtp

纯 C XTP 3.0 扩展。当前已实现 P1 Codec：32 字节固定头、两类命令、保留顺序的 table/array、批量编码、完整帧借用解码及参数访问。无网络、线程、全局状态和成功路径堆分配；不兼容 1.0/2.0。

`xtpParamAt` 对 array/table 均按发送顺序 O(1) 读取；`xtpParamFind` 仅对 table 按 key 顺序查找。去重校验只排序临时索引，不改变字段顺序。Body 和值不做转换。

```sh
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --cflag=-O2
python tools/package.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --kind static --verify
```

选择 `XTP_MODULE_XTP_CODEC`，声明入口 `<xtp.h>`。扩展单头只包含本库，先提供同一份 XRT；单个编译单元定义 `XTP_IMPLEMENTATION`。

完整协议、交付语义和阶段验收见 [设计](../../docs/design/xtp3.md) 与 [API 方案](../../docs/design/xtp3-api.md)。[Codec 示例](examples/codec/main.c) 可直接运行；[覆盖与进度](docs/coverage.md) 明确尚未完成的阶段。
