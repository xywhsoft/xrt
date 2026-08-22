# HTTP Upgrade

`<xrt/http_upgrade.h>` 实现 RFC 9110 `Upgrade` 字段的通用协议层，不绑定
WebSocket、HTTP 客户端、服务器或具体网络传输。

## 语法与借用

一个 `xhttpupgradeitem` 表示 `protocol-name[/protocol-version]`。`Protocol` 与
`Version` 都借用原字段值；空 `Version` 表示线路中没有斜杠和版本。名称与版本必须是
非空 HTTP token，斜杠两侧不允许空白。

`xrtHttpUpgradeParse` 解析单个元素。`xrtHttpUpgradeValid` 和
`xrtHttpUpgradeCount` 完整检查一个列表字段值。HTTP `#list` 允许空成员，因此前导、
连续和尾随逗号会被忽略；畸形的非空成员会失败。

## 迭代

`xrtHttpUpgradeNext` 在第一次发布条目前完整验证当前字段值。
`xrtHttpUpgradeFieldNext` 把全部重复 `Upgrade` 字段视为一份有序列表，并在第一次
发布前验证所有同名字段。这样后续坏字段不会让调用方先消费半份可信升级列表。

游标必须由对应的 `CursorInit` 函数初始化，输入在迭代结束前保持不变。游标、输出和
字段描述符支持未对齐存储；输出不能覆盖输入或游标。

协议名称按 RFC 使用 ASCII 大小写不敏感比较，可复用 `xrtHttpTokenEqual`；协议版本
是否区分大小写由具体协议定义。HTTP/1 解析器会拒绝任何畸形 `Upgrade` 字段，只有
列表至少包含一个协议时才发布 `XHTTP1_UPGRADE`。

## 写出

启用 `XRT_FEATURE_HTTP_UPGRADE_WRITE` 后：

- `xrtHttpUpgradeWrite` 规范生成 `name, name/version` 列表；
- `xrtHttpUpgradeElementWrite` 写一个元素；
- `xrtHttpUpgradeBuild` 一次分配零结尾文本，返回值由 `xrtFree` 释放。

直接写出支持空输出精确计长；短缓冲返回所需长度且不写部分结果。描述符、借用值、
长度输出和目标缓冲不得重叠。

## 裁剪

- `XRT_FEATURE_HTTP_UPGRADE`：解析、验证和重复字段迭代；依赖 `HTTP`；
- `XRT_FEATURE_HTTP_UPGRADE_WRITE`：直接写出与 Build；依赖 `HTTP_UPGRADE`。

示例位于 `examples/http/upgrade/main.c`。
