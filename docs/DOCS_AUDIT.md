# XRT 文档审计

> 审计日期：2026-04-01  
> 审计范围：根目录 README、`docs/`、`docs/api/`、`docs/guide/`、`docs/case/` 以及 `docs` 下的零散说明文档。

[返回文档中心](README.md)

---

## 1. 总结结论

当前 XRT 文档不是“完全不能用”，而是处在一种很典型的过渡状态：

- API 文档主体已经能对照当前源码主线使用
- 教学与案例文档多数仍停留在“主题短讲”
- 导航和交叉链接存在一批系统性路径错误
- 还有少量内部设计稿留在 `docs/`，会干扰正式文档边界

因此，当前文档工作的重点不应该只是“补几页字”，而是同时处理：

1. 断链
2. 结构定位
3. 覆盖缺口
4. 教学深度不足


## 2. 已发现的问题

### 2.1 链接问题

首次静态扫描时，本地文件链接问题主要集中在这些文件：

| 文件 | 问题特征 |
|------|----------|
| `docs/ARCHITECTURE.md` | 大量相对路径多写一层 `..` |
| `docs/ARCHITECTURE.en.md` | 同上 |
| `docs/api/api-xnet-v2.md` | 指向 `lib/` 的路径整体偏了一层 |
| `docs/api/api-network-tls.md` | 指向 `nettls.h` 的路径偏了一层 |
| `docs/README.md` | 返回根 README 的路径偏了一层 |
| `docs/guide/README.md` | 返回根 README 的路径偏了一层 |
| `docs/FAQ.md` / `docs/MIGRATION.md` / `docs/PERFORMANCE.md` | 返回根 README 或 `dev/bench` 的路径偏了一层 |
| `docs/TEST_RUNNER_SPEC.md` | 使用 `/D:/git/xrt/...` 形式的工作区绝对路径，不适合作为仓库文档链接 |

本轮已经优先修复主入口和高价值导航的路径问题；当前本地文件链接和已发现的目录锚点问题都已完成一轮收口。后续仍需继续复查的重点是：

- 新增文档后的增量断链检查
- 英文文档同步情况
- 历史页是否继续保留在正式入口


### 2.2 过期或失真的状态描述

下面这些页面的“状态描述”已经不能准确反映现状：

| 文件 | 当前问题 |
|------|----------|
| `docs/README.md` | 之前的表述更接近“第一轮教学体系已建成”，实际仍在重建中 |
| `docs/guide/README.md` | 之前把现有专题页描述成“正式教学入口”，实际还缺少系统课程结构 |
| `docs/case/README.md` | 之前把现有案例页描述成“正式案例主线”，实际仍缺少从简单到复杂的梯度 |
| `docs/EXAMPLES.md` | 说明偏抽象，没有准确映射当前 `examples/` 目录与模块覆盖 |

这些文件不一定“事实错误”，但会造成一个更严重的问题：

- 读者会以为教程已经完整
- 实际进入正文后，会发现大多是概念提要，而不是深入教学


### 2.3 教学覆盖缺口

当前教学主线还没有覆盖 XRT 全部公开模块。缺口主要在：

- 第 2 阶段内存主线：`llist`
- 第 3 阶段文本与数据：`regex / crypto`
- 第 6 阶段网络补充：`xurl / xhttp_util / xws / proxy`
- 系统能力的 API 合同层：`subprocess / file_async` 仍缺独立 API 页

这意味着当前 `guide/` 还没有实现“覆盖全部模块”的目标。


### 2.4 API 文档覆盖缺口

当前公开 API 里，至少有下面几块仍缺少独立或足够完整的正式 API 文档：

| 模块 | 当前状态 |
|------|----------|
| `file_async` | 只有 `guide/file-async-intro.md`，没有独立 API 合同页 |
| `subprocess` | 只有 `guide/subprocess-intro.md`，没有独立 API 合同页 |
| `xnet_proxy` | 目前散落在 `api-xnet-v2.md`、`api-xhttp.md`、`api-xws.md` 中，缺少单独整理 |
| `llist` | 文档仍在，但当前源码树暂无独立 public header，也未找到 `xrtLList*` / `xrtLLB*` 公开声明，属于待校准历史页 |

`xnet_proxy` 不一定必须单开一篇，但至少要在 API 索引中明确它的入口位置。


### 2.5 结构边界问题

下面这个文件更像内部工程稿，而不是面向使用者的正式文档：

- `docs/TEST_RUNNER_SPEC.md`

它当前的问题不是内容没价值，而是位置不对：

- 主题属于测试体系改造 spec
- 行文方式偏内部实施计划
- 使用了工作区绝对路径
- 会让 `docs/` 的“正式用户文档”边界变模糊

更合理的做法是：

- 移入 `dev/`
- 或在 `docs/` 中明确标记为“内部设计稿”


### 2.6 格式一致性问题

已发现的典型格式问题包括：

- `guide/file-async-intro.md` 之前缺少一级标题
- 部分目录锚点和真实标题不一致
- 中英文页的结构并不完全对齐

这类问题看起来比断链轻，但会直接影响目录导航和整体专业感。


## 3. 文档分类建议

### 3.1 可以继续作为正式主线保留并精修

- `docs/api/` 大部分模块文档
- `docs/ARCHITECTURE.md`
- `docs/FAQ.md`
- `docs/MIGRATION.md`
- `docs/PERFORMANCE.md`

### 3.2 应按“重写而不是修补”处理

- `docs/guide/README.md`
- `docs/case/README.md`
- `docs/EXAMPLES.md`
- `docs/guide/` 下多数专题页
- `docs/case/` 下多数案例页

原因很简单：问题不只是字不够多，而是教学结构本身还没有建立起来。

### 3.3 应补齐的新文档

- `file_async` API 文档
- `subprocess` API 文档
- 面向全部公开模块的教学路线图
- 面向全部公开模块的案例梯度图

### 3.4 应重新归类的文档

- `docs/TEST_RUNNER_SPEC.md`


## 4. 重建优先级

建议按下面顺序推进：

1. 修主入口和高频断链
2. 稳定 `docs/README.md`、`docs/guide/README.md`、`docs/case/README.md`
3. 建立正式教学路线图
4. 优先重写多任务专题
5. 补齐 `subprocess`、`file_async`、`queue`、`xurl / xhttp_util / xws / proxy`
6. 再逐步把容器、内存、文本模块补成完整课程


## 5. 多任务专题的特别说明

当前最需要重写、也最能决定整套教程质量的，是多任务专题。

后续这一部分不能直接从 API 开始，而应按下面顺序讲：

1. 为什么一条主线程不够
2. 线程能解决什么，代价是什么
3. 协程能解决什么，边界是什么
4. 为什么需要 `future / task / promise`
5. `queue` 为什么重要
6. `wait-source` 为什么能统一等待模型
7. 什么时候该混合使用线程、协程和 future

如果这部分讲透了，其他模块的教学都会顺很多；如果这部分继续只写成提纲，后续教程会一直显得“像 API 注释的扩写版”。


## 6. 当前动作

本轮已经开始做这些收口工作：

- 修复主入口与高价值导航的相对路径问题
- 重写 `docs/README.md`、`docs/guide/README.md`、`docs/case/README.md` 的状态说明
- 新增 [教学重建路线图](guide/ROADMAP.md)
- 第 2 阶段已补出 `bsmm / memunit / mempool_fs / mempool` 正式教学页
- 第 3 阶段已补出 `jnum` 正式教学页
- 第 3 阶段已补出 `xson` 正式教学页
- 明确哪些文档属于正式入口，哪些只是过渡页或内部稿
