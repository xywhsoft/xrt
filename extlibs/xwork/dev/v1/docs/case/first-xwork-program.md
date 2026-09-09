# 第一个 xwork 程序

> 状态：中文初稿，待审阅。

本范例对应 [`examples/first_xwork_program.c`](../../examples/first_xwork_program.c)，用于验证最小 runtime、workspace、run 生命周期是否可用。

## 解决的问题

这个程序不接入真实模型、不注册 host tool，也不写入持久化 store。它只证明最小共享对象模型可以工作：

- 创建 `xwork_runtime`。
- 注册一个 workspace。
- 创建一个 run。
- 推进 run 到 started/completed。
- 销毁 runtime 并释放其拥有的对象。

## 构建与运行

构建命令见 [`examples/README.md`](../../examples/README.md)。最小命令为：

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
```

运行后退出码为 `0` 表示生命周期推进成功。

## 执行流程

```text
xwork_runtime_options_init
xwork_workspace_options_init
xwork_run_options_init
xwork_runtime_create
xwork_runtime_add_workspace
xwork_run_create
xwork_run_start
xwork_run_complete
xwork_runtime_destroy
```

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_runtime_create()` | 创建 runtime。 |
| `xwork_runtime_add_workspace()` | 将 workspace 挂到 runtime。 |
| `xwork_run_create()` | 用 instruction 和 workspace refs 创建 run。 |
| `xwork_run_start()` | 推进 run 到 started。 |
| `xwork_run_complete()` | 标记 run 完成。 |
| `xwork_runtime_destroy()` | 释放 runtime 及其拥有的 workspace/run。 |

## 扩展方向

- 接入 `xwork_run_execute()`，让 run 进入 orchestrator tool loop。
- 添加 file persistence，验证 run snapshot 和 event log。
- 注册 host tool，再接入 policy/approval。

## 相关文档

- [第一个 xwork 程序教程](../guide/first-xwork-program.md)
- [Runtime API](../api/api-runtime.md)
- [Workspace API](../api/api-workspace.md)
- [Run API](../api/api-run.md)
