# 从零开始写第一个 xllm 程序

> 状态：中文初稿已生成，待审阅。

这篇教程带你写一个最小 xllm 程序。目标不是一次学完所有能力，而是先把最重要的调用顺序跑通：创建 runtime、注册 adapter/profile、构造 turn、发送请求、读取 response、释放资源。

## 你要先理解的 4 个对象

| 对象 | 你可以把它理解成 | 生命周期 |
| --- | --- | --- |
| `xllm_runtime` | xllm 的全局运行环境 | 程序启动后创建，退出前销毁 |
| `xllm_adapter` | 某类 provider 的协议实现 | 注册到 runtime |
| `xllm_profile` | 一组具体 provider 配置 | 注册到 runtime，通过 id 选择 |
| `xllm` | 已绑定 profile 的轻量聊天对象 | 用完调用 `xllm_destroy` |

最小调用顺序是：

```text
xrtInit
  -> xllm_runtime_create
  -> xllm_register_*_adapter
  -> xllm_register_profile
  -> xllm_create / xllm_bind_profile
  -> xllm_turn_add_user_text
  -> xllm_send_ex
  -> xllm_response_get_text
  -> free/reset/destroy
```

## 第一步：选择一个 provider

真实调用需要 provider 的 endpoint、模型名和密钥。为了让你先看清结构，下面使用 GLM native 作为示例。

你需要准备环境变量：

```bat
set GLM_API_KEY=你的密钥
```

示例中的关键配置是：

```c
tProfile.sId = "glm-native";
tProfile.sProvider = "zhipu";
tProfile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
tProfile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
tProfile.tAuth.sSecret = getenv("GLM_API_KEY");
tProfile.tModels.tText.sModelId = "glm-5-turbo";
```

如果你使用 OpenAI-compatible、Qwen、Gemini、Ollama 等 provider，整体顺序不变，只需要换 adapter 注册函数和 profile 字段。

## 第二步：写最小程序

下面的代码使用 `xllm-session.h`，因为它提供了更适合初学者的 `xllm_turn_add_user_text` 和 `xllm_send_ex`。

```c
#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    xllm_runtime *runtime = NULL;
    xllm *llm = NULL;
    xllm_response *response = NULL;
    xllm_profile profile;
    xllm_turn turn;
    xllm_error error;
    int status;

    xrtInit();

    xllm_profile_init(&profile);
    xllm_turn_init(&turn);
    xllm_error_init(&error);

    if (xllm_runtime_create(NULL, &runtime) != XRT_NET_OK) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }

    if (xllm_register_glm_native_adapter(runtime) != XRT_NET_OK) {
        fprintf(stderr, "failed to register adapter\n");
        xllm_runtime_destroy(runtime);
        return 2;
    }

    profile.sId = "glm-native";
    profile.sProvider = "zhipu";
    profile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
    profile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
    profile.tAuth.eKind = XLLM_AUTH_BEARER;
    profile.tAuth.sSecret = getenv("GLM_API_KEY");
    profile.tModels.tText.sModelId = "glm-5-turbo";
    profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;

    if (xllm_register_profile(runtime, &profile) != XRT_NET_OK) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(runtime);
        return 3;
    }

    llm = xllm_create(runtime, NULL);
    if (!llm || xllm_bind_profile(llm, "glm-native") != XRT_NET_OK) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(llm);
        xllm_runtime_destroy(runtime);
        return 4;
    }

    xllm_set_system_prompt(llm, "You are a concise assistant.");
    xllm_turn_add_user_text(&turn, "用一句话介绍 xllm。");

    status = xllm_send_ex(llm, &turn, NULL, &response, &error);
    if (status != XRT_NET_OK || !response) {
        fprintf(stderr, "request failed: %s\n",
            error.sMessage ? error.sMessage : "(no message)");
        xllm_error_reset(&error);
        xllm_turn_reset(&turn);
        xllm_destroy(llm);
        xllm_runtime_destroy(runtime);
        return 5;
    }

    printf("%s\n", xllm_response_get_text(response));

    xllm_response_free(response);
    xllm_error_reset(&error);
    xllm_turn_reset(&turn);
    xllm_destroy(llm);
    xllm_runtime_destroy(runtime);
    return 0;
}
```

## 第三步：编译

仓库里的示例通常带有自己的 `build.bat`。如果你只是学习，可以先参考已有 provider 示例：

```bat
cd D:\git\xllm\examples\glm
build.bat
```

如果你自己新建单文件程序，编译时要保证：

- 头文件能找到 `xllm-session.h`、`xllm.h` 和 `lib/xrt.h`。
- 实现文件包含进构建。单头模式通常通过 `XLLM_IMPLEMENTATION` 或示例构建脚本处理。
- Windows 下需要可用的 C 编译器，例如 MinGW-w64 `gcc`。

## 你刚刚完成了什么

这个程序虽然短，但它已经走完了真实调用的主路径：

1. `xllm_runtime_create` 创建运行时。
2. `xllm_register_glm_native_adapter` 注册协议适配器。
3. `xllm_register_profile` 注册具体 provider 配置。
4. `xllm_create` 创建轻量聊天对象。
5. `xllm_bind_profile` 选择 profile。
6. `xllm_turn_add_user_text` 添加用户输入。
7. `xllm_send_ex` 发起调用。
8. `xllm_response_get_text` 读取文本结果。
9. `xllm_response_free`、`xllm_turn_reset`、`xllm_destroy`、`xllm_runtime_destroy` 释放资源。

## 常见问题

### 为什么推荐先用 `xllm-session.h`

`xllm.h` 提供底层 request/response API，适合你完全控制 message、part、context block。`xllm-session.h` 增加了 turn helper，初学时更容易写对资源管理。

### 为什么要注册 adapter，还要注册 profile

adapter 解决“怎么和某类 provider 通信”；profile 解决“这次具体用哪个 endpoint、密钥、模型和能力声明”。一个 adapter 可以服务多个 profile。

### response 里的字符串要释放吗

不要单独释放 `xllm_response_get_text` 返回的字符串。它属于 `xllm_response`，最后调用 `xllm_response_free(response)`。

## 下一步

- 继续阅读 [Provider 与 Profile 入门](provider-profile-intro.md)。
- 需要查函数细节时看 [Core API](../api/api-core.md) 和 [Session API](../api/api-session.md)。

[返回教程入口](README.md)
