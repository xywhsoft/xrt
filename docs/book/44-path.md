---
num: 44
slug: path
title: 路径处理
volume: 卷五 系统服务
type: practice
lead: 纯词法操作不碰磁盘——拼接自动清理、四路分解、安全校验与逐段迭代。
api: path
---

## 导读

path 模块处理路径的**词法层**——拼接、分解、清理、校验，全部不触碰文件系统（纯字符串操作，零 IO、零副作用、测试免磁盘）。核心操作三件：`xrtPathJoin` 多段拼接且**自动词法清理**（`../` 折叠、分隔符归一）、四路分解（Name/Stem/Ext/目录一次到位）、`xrtPathIsSafeEntry` 安全校验（拒绝穿越与设备名——外部输入进路径的安检门）。另有零分配的逐段迭代与系统路径查询（home/cwd/real）。

## 引入

用户上传的文件要存进 `data/{用户名}/icon.png`——用户名是外部输入。直接拼：用户名传 `../../etc` 就把文件写到了沙箱外；用户名传 `CON.txt`（Windows 设备名）行为诡异。这类“路径穿越”是安全漏洞榜的常客，根因都是**外部输入未经词法校验直接进路径**。

第二个日常痛点是手工拼路径的清理：`"project" + "src/../include/xrt.h"`——拼出来的路径带着 `src/..` 冗余段，多数文件系统能容忍，但日志对比、缓存键、测试断言全被“同一路径多种写法”污染。`xrtPathJoin` 把清理内建进拼接：输出永远是折叠后的规范形态——**同一路径只有一种写法**是词法层的核心承诺。

## 概念

### 拼接与自动清理

`xrtPathJoin("project", "src/../include/xrt.h")`（两段 cstr 进、拥有式 str 出）→ `project\include\xrt.h`——`src/..` 被折叠、分隔符按平台归一。清理是**词法级**的（不解析符号链接、不查磁盘存在性）：`..` 在无法折叠时保留（`../secret` 不会被错误折叠成空——那正是安全语义）。跨平台形态：`XPATH_NATIVE` 按当前平台（Windows 反斜杠/Unix 正斜杠），显式指定形态用于生成跨平台一致的产物（测试快照、清单文件）。

### 四路分解与重建

| 分解 | 结果（对 `project\include\xrt.h`） |
| --- | --- |
| `xrtPathName` | `xrt.h`（最后一段） |
| `xrtPathStem` | `xrt`（去扩展名） |
| `xrtPathExt` | `.h`（含点） |
| 目录部分 | 拼接重建或迭代获得 |

`xrtPathWithName` 替换文件名保留目录（`renamed=project\include\runtime.h`）——改扩展名、加后缀的标准姿势。分解与重建互逆：`目录 + Name == 原路径`——测试就断言这条互逆律。

### 安全校验：外部输入的安检门

`xrtPathIsSafeEntry` 一票否决式校验，拒绝三类危险形态：**穿越**（`../` 序列——逃出基准目录）、**绝对路径**（外部输入不该决定根）、**平台设备名**（`CON`/`NUL` 等 Windows 保留名）。外部输入进路径前的标准流水线：`IsSafeEntry 校验 → Join 拼接（自动清理）→ 使用`——safe 范例的输出（`rejected`）就是这道门的执法记录。第二参数控制是否允许子目录条目（归档解包按需开）。

### 一条路径的解剖图

```diagram flow
- 输入：project/src/../include/xrt.h（用户各色写法）
- Join 清理：折叠 src/..、归一分隔符——产出规范形态
- 四路分解：Name=xrt.h / Stem=xrt / Ext=.h / 目录=project/include
- 重建互逆：目录 + Name == 原路径；WithName 换名保目录
```

### 逐段迭代与系统路径

`xrtPathIterInit/Next` 零分配逐段遍历（借用视图——每段是原路径的切片，第 25 章管线思想）；段计数、逐级建目录（第 47 章）都靠它。系统路径查询：home（用户目录）、cwd（当前目录）、real（解析符号链接后的真实路径——这个**要**触文件系统，是本模块唯一的例外）。

## 示例

### 完整程序：拼接清理与四路分解

来自仓库范例 `examples/path/basic/main.c`：

```embed path="examples/path/basic/main.c" title="examples/path/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/path/basic/main.c -lws2_32 -liphlpapi
path=project\include\xrt.h
name=xrt.h
stem=xrt
ext=.h
renamed=project\include\runtime.h
components=3
local=1
```

**刚才发生了什么。** ① `Join("project", "src/../include/xrt.h")` 的输出已经没有 `src/..`——拼接即清理，产物是规范形态；分隔符是 Windows 反斜杠（Linux 上跑同程序输出正斜杠——平台形态自动）。② 四路分解一次到位：Name 是 `xrt.h`、Stem 去 `.h`、Ext 含点——三个视图各自独立取得。③ `WithName` 替换文件名、目录不动——改名的标准姿势。④ `components=3` 来自零分配迭代——三段（project/include/xrt.h）逐段数出，迭代全程无堆分配。⑤ `local=1` 相对路径判定——绝对/相对的分野在路径路由（配置相对基准目录）时用。

### 完整程序：安全校验三连拒

来自 `examples/path/safe/main.c`：

```embed path="examples/path/safe/main.c" title="examples/path/safe/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/path/safe/main.c -lws2_32 -liphlpapi
assets/icon.png: safe
../secret.txt: rejected
CON.txt: rejected
```

**刚才发生了什么。** ① 正常相对路径 `assets/icon.png` 通过——校验不拦合法输入。② `../secret.txt` 被拒——穿越序列逃不出安检门；第三个输入（绝对路径形态）同样被拒——外部输入不该携带根。③ `CON.txt` 被拒——Windows 设备名的跨平台拦截（Linux 上跑同样拒绝，一致性优先）。把 tour 范例留作全接口阅读：解析结构（root/flags/stem/ext）、build 与 clean、相对化转换、相对与绝对判定各一组 ok——四组覆盖 path 模块的完整接口面。

## 契约

- **纯词法**：除 `real`（解析符号链接）外全部不碰文件系统——可测试性满格。
- **拼接即清理**：Join（两段 cstr）产物是折叠归一的规范形态；多段路径用嵌套 Join 或拼接后一次 Join；同一路径一种写法。
- **分解互逆**：目录 + Name == 原路径；Stem + Ext == Name（有点）。
- **安检纪律**：外部输入进路径前必过 `IsSafe`；穿越/绝对/设备名三连拒。
- **平台形态**：NATIVE 按平台、显式指定用于跨平台一致产物。
- **迭代零分配**：段视图借用原路径，时效同源。

### 从示例到工程：路径的三个宿主

**配置与资源解析**：应用启动时把"配置目录/数据目录/临时目录"三个基准路径规范化（Join + real），后续一切相对路径都 Join 到基准上——路径路由 centralized，避免散落各处的裸字符串路径。**用户输入映射**：上传文件名、URL 静态资源路径——IsSafeEntry 门卫 + Join 拼接 + 白名单扩展（第 36 章管线在文件层的镜像）。**跨平台产物**：清单文件、缓存键、测试快照里的路径——显式形态（不用 NATIVE）保证 Linux 与 Windows 产物一致，第 12 章哈希指纹才可复现。三宿主共享同一条底线：**路径先规范化再使用**——裸字符串路径是路径问题的万恶之源。

### 与第 45~48 章的关系

path 是文件族（第 45~48 章）的词法前置：打开文件前先有路径，路径先经过 Join/IsSafeEntry 的规范化与安检。文件族各章都会回头引用本章——"路径怎么来的"在源头（本章）讲一次，文件章专注"打开之后的事"。反向关系也存在：本章的 real 查询（唯一触盘接口）内部依赖文件系统元数据操作——词法层与 IO 层在 real 处握手，其余场合泾渭分明。

### 跨平台路径的三个差异点

写跨平台路径代码时三个差异点值得记牢。**分隔符**：Join 自动归一，手写 `/` 在 Windows 多数 API 也能过——但规范形态、缓存键、日志对比会因混合分隔符翻车。**根形态**：Unix 单根 `/` 与 Windows 盘符 `C:` + UNC `\server`——绝对路径判定（IsLocal 反查）按形态表走，`/foo` 在 Unix 是绝对、在 Windows 是"无盘符相对"。**设备名与保留名**：`CON`/`NUL`/`COM1` 是 Windows 特有陷阱——IsSafeEntry 按最严格平台统一把关（Linux 上也拒），产物才可跨平台分发。三个差异点全由本章 API 抹平——你写词法代码时可以"忘记平台"，这正是模块存在的意义。

## 避坑

### 坑 1：外部输入直接进路径

症状：安全审计报告路径穿越漏洞——`../../` 输入读写到沙箱外；或设备名输入造成诡异行为。

原因：拼接前没有校验环节——把“用户给的是文件名”当成了隐式契约。

```c bad
str sPath = xrtPathJoin("data/", UserNameBuf);   /* UserNameBuf 是外部输入 */
SaveTo(sPath);   /* UserName="../etc/passwd" 时：穿越段逃出 data 沙箱 */
```

```c good
xstrview Name = (xstrview){ UserName, strlen(UserName) };
if ( !xrtPathIsSafeEntry(Name, false) ) {
	RejectUpload();   /* 穿越/绝对/设备名在门口被拒 */
	return false;
}
```

### 坑 2：手工字符串拼接路径

症状：`"dir/" + name` 在 Windows 出现混合分隔符（`dir/\file`）；`a/b/../c` 冗余段污染缓存键与日志对比。

原因：跳过 Join 的清理与归一——用字符串 Concat（第 25 章）做路径的活。

```c bad
str sPath = xrtStrConcat(XRT_STR_LITERAL("data/"), NameView);
sPath = xrtStrConcat((xstrview){ sPath, strlen(sPath) }, XRT_STR_LITERAL("/icon.png"));
/* 分隔符混乱 + 无清理 + 三次分配 */
```

```c good
str sPath = xrtPathJoin("data/", NameBuf);   /* 一次调用：归一+清理+一次分配 */
```

## 练习

### 基础：互逆律验证（词法层的回归测试）

对五个不同形态的路径（相对/绝对/多段/带扩展/无扩展）做“分解再重建”，断言重建结果与原路径规范形态一致——互逆律的实测。

### 进阶：路径路由器（基准目录的统一入口）

实现 `resolve(基准目录, 输入路径)`：输入是相对路径则 Join 基准目录、是绝对路径按配置拒绝或放行；输出规范形态。配置开关的行为差异写进注释。

### 挑战：上传沙箱（安检门的完全体）

综合校验器：文件名安检（IsSafe 三连拒）+ 长度上限 + 扩展名白名单 + 结果路径唯一化（冲突时 Stem 加序号）。验收标准：二十组构造输入（穿越/设备/超长/白名单外/冲突）各得预期处置；全程零文件系统调用（纯词法层可测）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 纯词法 | 全模块除 real（符号链接解析）外零 IO——测试免磁盘 |
| Join | 拼接即清理：`../` 折叠、分隔符归一、规范形态唯一 |
| 四路分解 | Name（最后一段）/ Stem（去扩展名）/ Ext（含点）/ 目录重建 |
| 安检 | `IsSafeEntry(视图, 是否允许子目录)`：穿越/绝对路径/盘符/设备名连拒——外部输入必过 |
| 迭代 | `IterInit/Next` 零分配逐段，段视图借用 |
| 系统路径 | home/cwd 查询；real 解析符号链接（唯一触盘接口） |
| 三宿主 | 基准路径规范化 / 用户输入映射（门卫+白名单）/ 跨平台产物 |
| 跨平台三差异 | 分隔符归一 / 根形态判定 / 设备名统一把关——API 全抹平 |
