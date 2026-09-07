---
num: 18
slug: map
title: 哈希映射 xmap 与 xintmap
volume: 卷三 容器与数据结构
type: practice
lead: 字节键与整数键两套映射——GetOrAdd 一步取槽、插入序与键序两种迭代。
api: map
---

## 导读

映射解决"按键找值"：路由表按路径找统计、会话表按 ID 找状态、缓存按键找条目。XRT 提供两套映射：**`xmap`** 键是任意字节串（`xbytesview`，含内嵌零），迭代按**插入序**；**`xintmap`** 键是原生 `int64`（无哈希计算），迭代按**键升序**、稀疏键当"稀疏数组"用最快。两套共享同一套使用节奏——本章的主角是 `GetOrAdd`：一步完成"查找或创建"，返回**零初始化的值槽指针**，计数场景直接 `++`。这是使用频率仅次于数组的高频容器，也是第 23 章选型表的核心一格。

## 引入

统计一个 HTTP 服务的路由命中：每个请求到来，按路径找到该路由的计数器加一；路径从未出现过就新建一个计数器。用"先查有没有、没有再插入、插入后再初始化"的三步写法，每个环节都有失败路径，代码里三分之二是指甲盖大小的防御。更糟的是"查找两次"——`Has` 查一次、`Add` 内部又定位一次，热路径白烧一倍哈希。

`GetOrAdd` 把三步合成一步：键存在返回既有槽，不存在则**插入零初始化的新槽**并返回它——出参告诉你这次是不是新建。计数场景变成两行：取槽、加一。这就是"API 形状决定代码形状"的直观例子——好的接口让正确写法成为最短写法。

## 概念

### 两套映射，两种键

| 维度 | `xmap` 字节键 | `xintmap` 整数键 |
| --- | --- | --- |
| 键类型 | `xbytesview`（任意二进制） | `int64`（原生） |
| 键计算 | 哈希（SipHash 家族） | 直接位映射，零计算 |
| 迭代顺序 | **插入序**（内部维护顺序链） | **键升序** |
| 典型场景 | 字符串键、二进制键、配置与报表 | 会话 ID、稀疏下标、按 ID 导出 |

字节键的世界里"路径、字段名、指纹"都是合法键——`xbytesview` 不要求零结尾，第 3 章的二进制安全约定在键上同样成立。整数键跳过哈希直接定位，连续小键时它就是最快形态的稀疏数组；迭代按键升序，"按 ID 顺序导出全部会话"不需要排序步骤。

### 值内联：零逐条分配

与 `xarray` 的值语义一致，映射的**值内联存储在槽里**——`Init` 只需要值大小。插入一万条路由统计，分配次数是容器级的（桶数组增长），不是一万次。值的生命周期跟着容器走：`Unit` 一次归还全部。要存大对象或归属别处的对象？值类型用指针（`sizeof(ptr)`），配合第 14 章 slot_map 的句柄——"映射定位 + 句柄取对象"的组合在第 57 章连接表里再看完全体。

### 迭代的三态契约

```diagram flow
- IterBegin：拍下顺序快照并取得引用（迭代期间不得改容器）
- IterNext：步进一次，交出键视图与值指针；末尾返回 NULL 结束
- IterEnd：释放快照引用——三段式缺一不可
```

迭代交出的键是**借用视图**（`xbytesview`）、值是**槽内指针**——两者的时效都限于迭代期间（下一次 `Next` 或 `End` 之后不保证）。"迭代期间不得改容器"是快照语义的纪律：插入删除会让快照失效，需要边遍历边改就先把待处理项收集到数组（第 14 章），迭代结束后再改。

### 重复键与失败语义

`xmap` 的插入族在键已存在时返回**既有槽**（不覆盖、不报错）——"插入即取槽"与 `GetOrAdd` 行为一致，只是没有新建标志。删除按键进行，键不存在返回失败。所有失败路径容器保持原状（第 14 章建立的容器三约定之二、三条之三在这里照常生效）。

## 示例

### 完整程序：路由统计与插入序迭代

来自仓库范例 `examples/containers/map/main.c`：

```embed path="examples/containers/map/main.c" title="examples/containers/map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/map/main.c -lws2_32 -liphlpapi
/health requests=1 status=200
/metrics requests=8 status=204
```

**刚才发生了什么。** ① `xrtMapInit(&tRoutes, sizeof(routestat))` 只传值大小——`routestat` 整条结构内联在槽中，不是指针。② `GetOrAdd("/health")` 首次访问：插入零初始化槽（`bNew=true`），返回槽指针后**直接** `Requests++`、写状态码——新槽已清零的约定省掉了初始化三步。第二个路由同样两行就位。③ 迭代输出按**插入顺序** `/health`、`/metrics`——不是哈希值的乱序。这条性质来自内部维护的顺序链，价值在稳定性：配置导出、报表生成、测试断言都能精确复现。④ `IterNext` 同时交出键视图与值指针，`%.*s` 打印键是第 3 章的姿势。⑤ `IterEnd` 释放快照引用，`Unit` 归还容器——三段式迭代加容器收尾，一个不少。

### 完整程序：整数键与键序迭代

来自 `examples/containers/int_map/main.c`，会话 ID 从负数到百万级：

```embed path="examples/containers/int_map/main.c" title="examples/containers/int_map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/int_map/main.c -lws2_32 -liphlpapi
session=-9 requests=3 authenticated=no
session=1000001 requests=1 authenticated=yes
```

**刚才发生了什么。** ① 键 `-9` 与 `1000001` 相差百万——整数键映射对稀疏度完全不敏感，这就是"稀疏数组"的用法：下标不必连续，内存按实际条目数分配。② 输出按**键升序**：`-9` 在 `1000001` 前，与插入顺序相反——这是它与 `xmap` 插入序的本质分工，选型时先问"我要的遍历顺序是业务顺序（插入序）还是键顺序（有序）"。③ `GetOrAdd` 的节奏与字节键完全一致——两套映射学一套节奏即可上手另一套。

### 性能形态与容量

哈希映射的复杂度画像很规整：单键操作摊还 O(1)（哈希计算 + 桶定位），迭代 O(n)、顺序稳定。容量策略与第 14 章三件套一致：`Reserve` 预扩张避免装载过程的反复 rehash，`Trim` 在转入只读后收紧。两个实用细节：其一，键的哈希在容器内部完成（字节键走 SipHash 家族，密钥由容器自持），调用方不需要预算哈希——传入的就是键本身；其二，`xintmap` 的键序迭代意味着"导出按 ID 排序的会话列表"不需要排序步骤，log 级数据量时这一步节省经常比查找本身更可观。把这两套映射放进第 23 章的选型表：按键定位两套都行，要插入序选 `xmap`、要键序与极致性能选 `xintmap`。

### 从示例到工程：映射的三种宿主形态

映射句柄放哪，决定它在工程里的角色。**函数内临时统计**：栈上句柄 + 结尾 `Unit`，词频统计这类一次性任务两行生命周期。**长生命周期表**：挂在服务对象上，`Init` 在服务启动、`Unit` 在服务关闭——路由表、会话表都是这个形态，注意它跨请求存活，迭代纪律（三段式、禁增删）要在并发章节（卷六）的外层同步之下理解。**值槽装指针**：值类型声明为 `sizeof(ptr)`，槽里放对象指针或第 17 章 slot_map 的句柄值——"映射定位、句柄取对象"的复合结构里，映射只负责"键到句柄"这一跳，对象本体在池或 slot_map 里。三种形态覆盖了映射的绝大多数工程用法，第 57 章连接表是第三种形态的并发放大版。

## 契约

- **GetOrAdd 语义**：存在返回既有槽；不存在插入**零初始化**槽并返回；出参 `bNew` 区分两种情况。
- **值内联**：值住在槽里，`Unit` 一次归还；值类型为 `ptr` 时对象归属仍在调用方。
- **迭代三段**：`Begin`/`Next`/`End` 缺一不可；迭代期间不得增删；键视图与值指针时效止于迭代期。
- **顺序承诺**：`xmap` 插入序、`xintmap` 键升序——输出可精确复现，可写进测试断言。
- **键的二进制安全**：字节键可含内嵌零；长度以视图为准，不看零结尾。
- **宿主形态**：临时统计（栈句柄）、长活表（挂服务对象）、键到句柄（值装 `ptr`）三种形态各有收尾纪律。

### 一个常被问的问题：为什么没有"线程安全映射"

卷三的全部容器都不是线程安全的——这不是遗漏，是分层决策。容器内加锁意味着每次 `GetOrAdd` 都过一次锁，单线程程序白白付同步成本；而多线程程序的正确粒度往往不在单次容器操作——"查表 + 决策 + 写回"三步需要的是外层一致性，容器级锁保不住这个语义。因此 XRT 把同步交给卷六的原语：读多写少用读写锁包住整张表、跨线程转移用通道、真正的高并发用分片（每线程一张映射，定期合并）。第 23 章选型表会在"线程"一栏再次提醒这一条——选容器时不考虑并发，写并发时不指望容器。

## 避坑

### 坑 1：把迭代中交出的指针存起来

症状：迭代后使用保存的键或值指针，读到陈旧或悬空内容；小数据量下偶发正常。

原因：迭代交出的是槽内地址与借用视图，下一次 `Next`/`End` 或任何容器编辑都可能使其失效。

```c bad
routestat* pSaved = NULL;
xbytesview KeySaved;
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	pSaved = pStat;      /* 保存迭代期指针 */
	KeySaved = Path;
}
xrtMapIterEnd(&tIterator);
pSaved->Requests++;      /* 迭代已 End——指针时效已过 */
```

```c good
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	Process(&Path, pStat);   /* 迭代期内用完即弃 */
}
xrtMapIterEnd(&tIterator);
/* 需要长期保存：迭代中把键/值拷贝进数组，迭代结束后处理数组 */
```

### 坑 2：迭代中增删容器

症状：崩溃、跳过条目或重复条目；复现依赖条目数量与操作时序，几乎无法稳定重现。

原因：迭代建立在顺序快照之上；增删会重组内部结构，快照与实际结构脱节。

```c bad
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	if ( pStat->Requests == 0 ) {
		xrtMapRemove(&tRoutes, Path);   /* 迭代中删容器 */
	}
}
```

```c good
xarray tDead;
xrtArrayInit(&tDead, sizeof(xbytesview));
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	if ( pStat->Requests == 0 ) {
		xrtArrayPush(&tDead, &Path);    /* 先收集键（注意：键源须存活至删除完成） */
	}
}
xrtMapIterEnd(&tIterator);
for ( size_t i = 0; i < tDead.Count; i++ ) {
	xrtMapRemove(&tRoutes, *(xbytesview*)xrtArrayGet(&tDead, i));
}
xrtArrayUnit(&tDead);
```

注：good 版收集的是键视图——视图的源内存必须活到删除完成；键若是字面量或外部缓冲则天然满足，若键由容器内部持有则应收集一份拷贝（如 `xrtTempDup` 到 arena）。

## 练习

### 基础：词频统计

读入一段英文（硬编码字符串即可），按单词为键统计词频，按插入序输出全部词条。提示：空格分词、`GetOrAdd` 计数；输出前先在纸上写出预期的词条顺序（插入序），再与程序对拍。

### 进阶：两套映射对照

同一组会话数据（ID 从 -5 到 100）分别装进 `xmap`（键用 8 字节二进制视图）与 `xintmap`，观察两版迭代顺序差异；再用第 12 章的 `Hash64` 给"键字节串"算指纹对比直接整数键的开销直觉。

### 挑战：LRU 完全体（映射 + 链表 + 上限淘汰）

实现带容量上限的 LRU：`xmap` 存"键 → 条目"，条目内嵌 `xlistnode`（第 17 章）挂进时序链；get 命中 `MoveFront`、put 超容淘汰链尾并从映射删除。验收标准：容量 3、访问序列 1/2/3/1/4 后淘汰的是 2；淘汰后映射条目数与链节点数一致；零泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 两套映射 | `xmap` 字节键插入序 / `xintmap` 整数键键升序 |
| 取槽 | `GetOrAdd` 一步查找或创建，新槽零初始化，`bNew` 出参 |
| 值内联 | 值住槽里，`Unit` 全量归还；存对象用 `ptr` 值 + 句柄 |
| 迭代三段 | `Begin`/`Next`/`End`；迭代期指针不出迭代期；迭代中禁增删 |
| 二进制键 | `xbytesview` 含零字节合法，长度为准 |
| 稀疏数组 | 连续小键场景 `xintmap` 即最快稀疏数组 |
| 容量 | `Reserve`/`Trim` 三件套同名同义；哈希由容器内部完成 |
| 选型口诀 | 按插入序导出 → `xmap`；按键序导出或整数键 → `xintmap` |
