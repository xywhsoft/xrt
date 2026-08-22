# XID

XID 模块生成 192 位分布式标识。它保留旧版 XRT 的 24 字节二进制和 32 字符紧凑文本优势，但使用跨平台固定布局、系统安全随机源和值类型 API，删除对本机 IP、结构体端序和逐对象堆分配的依赖。

## 模块

`XRT_MODULE_XID` 启用 `XRT_FEATURE_XID`，并精确依赖 `time`、`random_secure` 和 `codec_base64`。模块不依赖网络、线程、任务或容器。

## 公开类型与常量

| 名称 | 含义 |
| --- | --- |
| `xid` | 固定 24 字节的 XID 值类型 |
| `xiderror` | `xrt.xid` 错误域的稳定错误代码类型 |
| `XID_ERROR_FORMAT` | 文本长度、字母表或规范编码错误 |
| `XID_BINARY_SIZE` | 二进制值长度，固定为 24 |
| `XID_TEXT_SIZE` | 文本长度，固定为 32 |
| `XID_TEXT_CAPACITY` | 包含末尾零字节的写入容量，固定为 33 |
| `XID_ZERO` | 仅用于对象定义的静态全零初始化器 |

## 布局

`xid` 始终是 `XID_BINARY_SIZE`，即 24 字节：

| 字节 | 内容 |
| --- | --- |
| `0..7` | 经符号偏置后按大端保存的 Unix 微秒 |
| `8..23` | 128 位操作系统安全随机数 |

符号偏置让完整 `xtime` 范围按无符号字节顺序排列。不同生成时间的 XID 可以直接按 24 字节比较；同一微秒内的顺序由随机后缀决定。

`XID_ZERO` 初始化全零值，`xrtXidIsZero` 检查全零。全零值是合法的可存储值，但不会由正常生成器产生。

## 生成

`xrtXidMake` 把一个新 ID 写入调用方提供的 `xid`，不分配内存。随机源失败时输出保持全零，并原样传播 `xrt.random` 错误；不会退化到可预测伪随机数。

`xrtXidMakeMany` 批量生成连续数组。它一次获取整批安全随机字节，再为每项写入时间前缀，适合日志批次、对象导入和高吞吐服务。`iCount == 0` 时允许 `pXids == NULL`；大小溢出和非空空指针会失败。随机源失败时整批清零。

`xrtXidMakeString` 是常见路径 helper，直接返回新生成的 32 字符文本；调用方使用 `xrtFree` 释放。

XID 用于唯一标识，不是身份验证令牌。虽然随机后缀来自安全随机源，文本仍公开生成时间，不应代替 session secret、CSRF token 或访问凭据。

## 文本

`XID_TEXT_SIZE` 固定为 32，`XID_TEXT_CAPACITY` 是包含末尾零字节所需的 33 字节容量。字母表为：

```text
-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz
```

字母表按 ASCII 递增并且 URL-safe。固定长度编码因此同时保持二进制顺序和普通字符串字典序，XID 文本可以直接作为数据库有序键、文件名或 URL 路径段。

`xrtXidWrite` 写入调用方缓冲，不分配内存；容量不足时把可用输出首字节清零并返回范围错误。`xrtXidFormat` 返回由 `xrtFree` 释放的文本。

`xrtXidParse` 只接受完整 32 字节规范文本，不接受空白、填充、别名字母、截断或附加数据。解析失败不修改输出 `xid`。错误域为 `xrt.xid`，代码是 `XID_ERROR_FORMAT`；`xrtXidErrorOffset` 返回第一个非法字节、文本末尾或第一个多余字节的位置。

## 时间与比较

`xrtXidTime` 从固定前缀恢复 Unix 微秒，不访问系统时钟。任意 24 字节值都能按同一布局解释，因此二进制反序列化不需要额外“有效”标记。

`xrtXidCompare` 返回负数、零或正数的规范三态结果，先比较时间前缀，再比较随机后缀。`xrtXidEqual` 比较全部 24 字节。空指针是参数错误；XID 是小型值类型，通常应直接嵌入结构或数组，而不是以可空堆对象表示。

## 示例

`examples/id/xid/main.c` 展示无分配生成、固定缓冲写入、解析和时间提取。`examples/id/xid_batch/main.c` 展示批量生成、拥有型文本、三态比较、相等/全零判断和结构化错误位置。并发无碰撞、OOM、完整有符号时间范围和单头文件路径由模块测试覆盖。
