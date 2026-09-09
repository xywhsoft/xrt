# 模块名 API

> 用一句话说明这个模块解决什么问题。

[返回 API 索引](README.md) | [相关教程](../guide/README.md) | [相关案例](../case/README.md)

---

## 目录

- [模块定位](#模块定位)
- [常量与宏](#常量与宏)
- [公共类型](#公共类型)
- [标准调用顺序](#标准调用顺序)
- [功能分组一](#功能分组一)
- [常见用法](#常见用法)
- [常见错误](#常见错误)
- [相关示例](#相关示例)

---

## 模块定位

说明这个模块解决什么问题、不解决什么问题，以及它和 runtime/workspace/run/tool/persistence 等其他模块的边界。

---

## 常量与宏

| 名称 | 值 | 说明 | 什么时候使用 |
| --- | --- | --- | --- |
| `NAME` | `value` | 说明 | 使用场景 |

---

## 公共类型

### `type_name`

说明这个类型代表什么，以及调用者是否需要直接创建、初始化、reset 或 destroy。

| 字段 / 值 | 类型 / 值 | 默认值 | 说明 | 所有权 |
| --- | --- | --- | --- | --- |
| `field` | `type` | `default` | 字段含义 | borrowed / copied / owned / caller-owned |

---

## 标准调用顺序

1. 初始化 options 或输入对象。
2. 创建或注册资源。
3. 调用主要 API。
4. 读取结果。
5. reset/free/destroy。

---

## 功能分组一

### `xwork_function_name`

一句话说明这个函数。

**功能：**

说明这个函数做什么，适合在哪个场景使用，不适合解决什么问题。

**函数原型：**

```c
XWORK_API xwork_status xwork_function_name(type *pArg);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pArg` | 输入 / 输出 | 否 | 参数含义、生命周期、所有权、有效范围、单位和默认值 |

**返回值：**

- `XWORK_OK`：成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- 其他错误码按函数语义列出。

**资源归属：**

- 说明该函数是否分配资源。
- 说明调用者应该用哪个 `reset` / `destroy` 函数清理。
- 说明返回指针是 borrowed、owned 还是由 runtime 托管。

**补充说明：**

- 调用顺序要求。
- 线程安全和 re-entry 注意事项。
- persistence/recovery 边界。
- profile、workspace、host tool 或 replay 差异。
- 兼容性和版本注意事项。

**范例代码：**

```c
#include "xwork.h"

int main(void)
{
    return 0;
}
```

**相关 API：**

- `related_function`

---

## 常见用法

说明这个模块最常见的 2-3 种组合方式。

## 常见错误

| 问题 | 原因 | 处理方式 |
| --- | --- | --- |
| 错误现象 | 常见原因 | 正确做法 |

## 相关示例

- `examples\...`
