# 最小聊天调用

本案例展示一个最小的 xllm 文本聊天程序：创建 runtime，注册 provider，注册 profile，发送一轮用户消息，读取模型回答。

[返回范例解析](README.md) | [第一个 xllm 程序](../guide/first-xllm-program.md) | [Core API](../api/api-core.md)

## 问题

你希望用最少的 xllm API 调用完成一次聊天请求。这个场景不需要长期历史、不需要工具、不需要 memory，只需要把一段用户文本发送给模型并拿到回答。

这个案例适合：

- 验证 provider API key 是否可用。
- 验证 xllm 编译和链接是否正确。
- 学习 runtime、profile、turn、response 的最小关系。
- 给更复杂的 session、tool、memory 场景打基础。

## 架构

最小调用链如下：

```text
应用程序
  -> xllm_runtime
    -> provider adapter
    -> provider profile
  -> xllm
    -> xllm_turn
    -> xllm_send_ex
  <- xllm_response
```

你可以把 `xllm_runtime` 理解为“全局运行环境”，把 `xllm_profile` 理解为“某个 provider/model 的连接配置”，把 `xllm_turn` 理解为“这一轮要发给模型的新输入”。

## 步骤 1：初始化 runtime

```c
xllm_runtime *pRuntime = NULL;

xrtInit();

if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
    fprintf(stderr, "failed to create runtime\n");
    return 1;
}
```

`xrtInit()` 来自 xrt。xllm 建立在 xrt 之上，示例程序应先初始化 xrt。

## 步骤 2：注册 Provider Adapter

以 GLM native adapter 为例：

```c
if ( xllm_register_glm_native_adapter(pRuntime) != XRT_NET_OK ) {
    fprintf(stderr, "failed to register glm native adapter\n");
    xllm_runtime_destroy(pRuntime);
    return 2;
}
```

Adapter 负责把 xllm 的通用请求结构转换成 provider 的 HTTP 请求，再把 provider 响应解析回 `xllm_response`。

## 步骤 3：注册 Profile

Profile 描述这次调用使用哪个 provider、base URL、认证方式、模型和能力。

```c
xllm_profile tProfile;

xllm_profile_init(&tProfile);
tProfile.sId = "glm-native";
tProfile.sProvider = "zhipu";
tProfile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
tProfile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
tProfile.tAuth.sSecret = getenv("GLM_API_KEY");
tProfile.tModels.tText.sModelId = "glm-5-turbo";
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
    fprintf(stderr, "failed to register profile\n");
    xllm_runtime_destroy(pRuntime);
    return 3;
}
```

学习时最容易漏掉的是 `tCaps.uFlags`。它告诉 xllm 当前模型支持文本输入、文本输出、流式输出等能力。后续工具、JSON、图片、文件等能力也都通过这里声明。

## 步骤 4：创建 xllm 对象并绑定 Profile

```c
xllm *pLlm = xllm_create(pRuntime, NULL);

if ( !pLlm || xllm_bind_profile(pLlm, "glm-native") != XRT_NET_OK ) {
    fprintf(stderr, "failed to create llm or bind profile\n");
    xllm_runtime_destroy(pRuntime);
    return 4;
}

xllm_set_system_prompt(pLlm, "You are a concise assistant.");
```

`xllm` 对象适合一次或少量无历史请求。如果你要做连续聊天，应使用 `xllm_session`，见 [带短期历史的 Session Chat](session-chat.md)。

## 步骤 5：构造 Turn

```c
xllm_turn tTurn;

xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "Use one sentence to introduce xllm.");
```

Turn 是“本轮新增输入”。最小聊天里只有一个用户文本 part。后续多模态输入、工具定义、JSON 输出格式也可以放进 turn。

## 步骤 6：发送请求并读取响应

```c
xllm_call_options tCallOptions;
xllm_error tError;
xllm_response *pResponse = NULL;
int iStatus;

xllm_call_options_init(&tCallOptions);
tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
tCallOptions.uTimeoutMs = 120000u;

xllm_error_init(&tError);

iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
if ( iStatus != XRT_NET_OK || !pResponse ) {
    fprintf(stderr, "request failed: code=%d http=%d msg=%s\n",
            (int)tError.eCode,
            (int)tError.iHttpStatus,
            tError.sMessage ? tError.sMessage : "(null)");
    return 5;
}

printf("%s\n", xllm_response_get_text(pResponse));
```

`xllm_response_get_text` 返回可见文本，适合普通聊天 UI。如果你需要读取工具调用、JSON 输出或多段输出，应使用 response API。

## 步骤 7：清理资源

```c
xllm_response_free(pResponse);
xllm_error_free(&tError);
xllm_turn_reset(&tTurn);
xllm_destroy(pLlm);
xllm_runtime_destroy(pRuntime);
```

资源释放顺序不必和创建顺序完全相反，但要确保：

- `xllm_response_free` 释放响应。
- `xllm_turn_reset` 释放 turn 内部复制的消息和 part。
- `xllm_destroy` 释放 `xllm` 对象。
- `xllm_runtime_destroy` 最后释放 runtime。

## 完整示例

完整可运行示例见：

```text
examples/glm/glm_stateless.c
```

运行前设置环境变量：

```powershell
$env:GLM_API_KEY="你的 GLM API Key"
```

然后从仓库根目录运行 GLM 示例构建脚本：

```bat
cmd /c .\examples\glm\build.bat
```

脚本会生成 `build\glm_native_stateless.exe`，同时也会构建 session、reasoning、JSON schema、tool loop、多模态和 stream 示例。运行 stateless 示例前需要设置 `GLM_API_KEY`。示例会发送一句话请求，并打印流式增量和最终可见文本。

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_runtime_create` | 创建运行环境 |
| `xllm_register_glm_native_adapter` | 注册 GLM native adapter |
| `xllm_profile_init` | 初始化 profile |
| `xllm_register_profile` | 把 profile 注册到 runtime |
| `xllm_create` | 创建无历史 `xllm` 对象 |
| `xllm_bind_profile` | 绑定默认 profile |
| `xllm_turn_add_user_text` | 添加用户文本 |
| `xllm_send_ex` | 发送请求并接收错误详情 |
| `xllm_response_get_text` | 读取回答文本 |
| `xllm_response_free` | 释放响应 |

## 扩展点

你可以在这个最小案例上继续加入：

- 流式事件回调：实时显示文本增量。
- JSON schema 输出：让模型返回结构化数据。
- Provider 切换：同一套请求换成 OpenAI compatible、Qwen、Kimi 等 profile。
- Session：保存多轮聊天历史。
- Memory：把检索结果注入 turn。

## 常见问题

如果返回鉴权错误，先检查 `GLM_API_KEY` 是否设置、是否有前后空格、provider base URL 是否正确。

如果返回能力不支持，检查 `tProfile.tModels.tText.tCaps.uFlags` 是否包含请求所需能力。

如果没有流式输出，确认 profile 声明了 `XLLM_CAP_STREAM`，并且 `tCallOptions.eStreamMode` 不是 `XLLM_STREAM_OFF`。

如果响应为空，先打印 `xllm_error`，再打开 diagnostics trace 查看 provider 原始响应。
