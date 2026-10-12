# HTTP 缓存存储

`http_cache_store` 把无状态缓存协议判定与具体响应存储连接起来。它提供不可变
`xhttpcacherecord` 和线程安全、有界的内存 `xhttpcache`，不替代
`http_cache_policy`、`http_cache_validate` 或 `http_cache_range` 的协议决定。

## 设计边界

- `xhttpcacherecord` 只保存已经由上层判定可以存储的响应。
- `xhttpcache` 只负责主键、`Vary` 变体、引用生命周期、容量和 LRU。
- 新鲜度、验证、304 更新、Range 合并和不安全方法失效仍由对应协议模块决定。
- 派生更新通过当前 Record 条件提交，不能用旧快照覆盖并发写入。
- 每条记录只有一次紧凑分配，没有固定 8 KiB 或其他预留正文缓冲。
- 命中返回独立引用；其他线程可以同时替换或淘汰索引，不会使命中视图失效。

这一分层允许协议工具、内存客户端缓存和未来持久化缓存共享同一套判定逻辑，
同时避免让零分配协议解析依赖 Map、Mutex 或存储策略。

## 主键

`xhttpcachekey` 包含：

| 成员 | 含义 |
| --- | --- |
| `Method` | 区分方法的大小写敏感 HTTP token |
| `URI` | 调用方选定的稳定目标 URI |
| `Partition` | 可选站点、用户或租户隔离键 |
| `Fields` | 当前有效请求字段 |
| `FieldCount` | 请求字段数量 |

缓存至少按方法和目标 URI 选择响应。`Partition` 是额外的隐私和多租户边界；
空分区只匹配空分区。高层客户端应丢弃 URL fragment，并在需要时统一 scheme、
host 和默认端口的表示，然后把结果作为 `URI`。

`xrtHttpCacheKeyInit` 初始化无分区、无字段的普通主键。Cookie、自动
`Accept-Encoding` 等修改必须先应用到请求，再把最终字段交给缓存。

## 不可变记录

`xrtHttpCacheRecordInputInit` 建立 HTTP/1.1 输入骨架。调用方随后设置 Header、
Trailer、时间和正文片段，再调用 `xrtHttpCacheRecordCreate`。

正文由 `xhttpcachepart` 数组表示：

- `Offset` 是片段在完整表示中的偏移。
- `Data` 是片段字节。
- 片段必须按偏移排序、非空且不重叠。
- `HAS_LENGTH` 表示 `Length` 已知。
- `COMPLETE` 还要求片段无空洞覆盖 `[0, Length)`。

因此同一数据结构既能保存完整 200，也能保存截断 200 或规范化后的 206。
当前模块不自行合并片段；组合动作由 `xrtHttpCacheCombinePlan` 决定后，调用方
构造新的不可变 Record 替换旧记录。

创建时会复制全部响应 Header、Trailer、正文和主键文本。对于请求字段，只复制
响应 `Vary` 实际引用的名称和值。`Vary: *` 永远不能匹配，因此创建返回
`XERR_UNSUPPORTED`，上层应把它作为正常的不存储决定。

## Vary 匹配

`xrtHttpCacheRecordMatches` 先精确比较方法、URI 和分区，再逐个比较 `Vary`
指定的原请求字段：

- 字段名大小写不敏感。
- 同名字段按出现顺序比较。
- 字段值忽略两端 OWS，内部字节保持不变。
- 缺失字段只匹配缺失字段。
- 重复的 `Vary` 名称只保存一次。

该策略对未知字段保持保守：可能放弃一次语义等价但词法不同的命中，不会把无法
证明等价的响应错误复用。未来的字段专用规范化可以在这一唯一匹配点扩展。

## 内存缓存

`xrtHttpCacheCreate` 创建引用计数、线程安全的缓存。默认限额为：

| 限额 | 默认值 |
| --- | ---: |
| 最大条目 | 1024 |
| Record 总字节 | 64 MiB |
| 单 Record 字节 | 8 MiB |

`MaxBytes` 精确累计 Record 的单次分配大小；`MaxEntries` 单独约束 Map 和 LRU
元数据。两者共同形成边界，避免以难以解释的估算值冒充整个进程分配硬上限。

每个正在调用缓存 API 的线程必须持有自己的 `xhttpcache` 引用。最后一次
`xrtHttpCacheRelease` 不得与没有独立引用保护的并发调用竞态；满足该条件时，
内部 Mutex 保护索引、LRU 和统计，命中 Record 的独立引用保护锁外读取。

`xrtHttpCachePut` 的结果：

| 结果 | 含义 |
| --- | --- |
| `ERROR` | 参数、分配或同步错误，没有提交半成品 |
| `CONFLICT` | 条件插入或替换发现当前值已经变化 |
| `REJECTED` | Record 超过单条目或总字节限额 |
| `STORED` | 首次保存该 Vary 变体 |
| `REPLACED` | 原子替换相同主键和 Vary 原请求值 |

提交后缓存持有独立 Record 引用。超限时从最近最少使用端淘汰；命中会更新 LRU。
同一请求有多个可匹配响应时，优先选择有效 `Date` 较新者，否则使用接收时间，
最后使用提交顺序消除平局。

`xrtHttpCacheGet` 在命中时返回独立 Record 引用，调用方必须
`xrtHttpCacheRecordRelease`。未命中不设置错误。

配置、主键、RecordInput、正文片段数组、后端 Ops、命中指针、删除数量、Entry
和统计都是固定描述符，可以位于完整但未对齐的存储。创建与注册接口在返回前复制
配置和 Ops；Record 创建成功后拥有全部 Header、Trailer、正文与键文本。地址范围
回绕或输入输出重叠会在分配、锁定或存储变更前作为参数错误拒绝。普通未命中与
执行失败会把命中指针、删除数量或统计输出清零，自定义后端只接收库内对齐的临时
输出，验证通过后才一次性发布给调用方。

普通 `xrtHttpCachePut` 是明确的最后写入者获胜接口，不返回 `CONFLICT`。它适合
保存完整网络响应或调用方有意覆盖当前值的场景。`Put`、`Insert` 和 `Replace`
都不接管调用方引用；成功后 Cache 自己增加并持有引用。

## 条件提交

读取后再构造新记录的操作必须使用条件接口：

- `xrtHttpCacheInsert` 只在对应主键和 `Vary` 变体不存在时原子插入。
- `xrtHttpCacheReplace` 只在 `Expected` 仍是当前版本时原子替换。
- `xrtHttpCacheRemoveRecord` 只在 `Expected` 仍是当前版本时原子删除。

`Expected` 应当是先前 `xrtHttpCacheGet` 返回的不可变 Record。内存后端以精确
Record 身份判断版本；自定义持久化后端可以把它映射到内部修订号。条件不成立是
正常并发结果，返回 `CONFLICT`，不会覆盖竞争者，也不要求设置新的全局错误。

`Replace` 允许新记录改变 `Vary` 选择器。如果它与同一主键下的另一变体碰撞，
内存后端会在同一个锁内删除重复项、替换当前项并执行容量限制，不会暴露两个相同
变体，也不会把内部去重误计为 LRU 淘汰。

调用方处理冲突时必须重新 `Get` 当前记录，并根据最新内容重新计算派生结果。
例如合并 Range 片段时，不能直接重交基于旧快照构造的 Record，否则仍会丢失
竞争请求刚提交的片段。客户端缓存对片段合并采用有界的重新读取、重新规划和
重新提交；304 更新发生冲突时保留更新者，当前请求仍可重放自己已经验证的快照。

## 删除与统计

- `xrtHttpCacheRemoveRecord` 按当前 Record 版本执行条件删除。
- `xrtHttpCacheRemove` 删除与完整 Key 和 `Vary` 选择匹配的记录。
- `xrtHttpCacheRemoveURI` 删除指定 URI 和分区下的全部方法与变体。
- `xrtHttpCacheClear` 清空记录并保留 Map 桶容量。
- `xrtHttpCacheStats` 返回条目、Record 字节、命中、未命中、替换、拒绝、条件
  冲突、淘汰和显式删除的一致快照。

两个 Remove 函数都返回 `bool`，删除数量通过可空的 `size_t*` 输出；成功删除零条
与后端失败因此不会混淆。`Clear` 同样返回执行状态。自定义后端必须通过这些状态
返回值和当前结构化错误完整表达持久化、锁或 I/O 失败，不能用零删除伪装失败。

不安全方法成功后的 URI、`Location` 和 `Content-Location` 失效应使用
`xrtHttpCacheInvalidationNext` 生成目标，再调用 `xrtHttpCacheRemoveURI`。

## 自定义后端

`xrtHttpCacheOpen` 复制 `xhttpcacheops`。除可选的 `Close` 外，`Get`、`Put`、
`Insert`、`Replace`、`RemoveRecord`、`Remove`、`RemoveURI`、`Clear` 和
`Stats` 都必须存在；缺少任一回调都会以无效参数拒绝打开。最后一个 Cache 引用
释放时才调用一次 `Close`。

全部回调必须能被并发调用。特别是：

- `Insert` 必须把不存在判断和插入放在同一个事务或锁内。
- `Replace` 必须把当前版本比较和写入放在同一个事务或锁内。
- `RemoveRecord` 必须把当前版本比较和删除放在同一个事务或锁内。
- 无条件 `Put` 只能返回 `ERROR`、`REJECTED`、`STORED` 或 `REPLACED`；
  返回 `CONFLICT` 属于后端契约错误。
- `Get` 命中时向调用方转移一个独立 Record 引用；统一句柄会再次验证该 Record
  的方法、URI、分区与 `Vary` 选择确实匹配查询 Key，后端返回错误记录会被释放
  并报告内部错误。
- `Stats` 返回失败时，统一句柄保证调用方的统计输出为全零。
- 三个写入回调都不接管传入引用；后端提交成功后自行持有独立引用。

仅在进程内比较 `Expected` 指针不足以实现跨进程后端。后端应在 Record 与自己的
版本号、事务序列或内容修订之间建立稳定映射，并保证比较与提交不可分割。

## 示例

```c
xhttpcachekey Key;
xhttpcacherecordinput Input;
xhttpcachepart Part;
xhttpcacherecord* pRecord;
xhttpcacherecord* pUpdated;
xhttpcacherecord* pHit = NULL;
xhttpcache* pCache;

xrtHttpCacheKeyInit(
	&Key,
	XRT_STR_LITERAL("GET"),
	XRT_STR_LITERAL("https://example.test/data")
);
xrtHttpCacheRecordInputInit(
	&Input, &Key, XHTTP_STATUS_OK
);

Part.Offset = 0;
Part.Data = XRT_BYTES_LITERAL("payload");
Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
	XHTTP_CACHE_RECORD_COMPLETE;
Input.Parts = &Part;
Input.PartCount = 1;
Input.Length = Part.Data.Size;

pRecord = xrtHttpCacheRecordCreate(&Input);
pCache = xrtHttpCacheCreate(NULL);
xrtHttpCacheInsert(pCache, pRecord);

if ( xrtHttpCacheGet(
	pCache, &Key, &pHit
) == XHTTP_CACHE_LOOKUP_HIT ) {
	Part.Data = XRT_BYTES_LITERAL("updated");
	Input.Length = Part.Data.Size;
	pUpdated = xrtHttpCacheRecordCreate(&Input);

	/* 仅在 pHit 仍是当前版本时提交派生更新。 */
	if ( pUpdated != NULL ) {
		xrtHttpCacheReplace(pCache, pHit, pUpdated);
	}
	xrtHttpCacheRecordRelease(pUpdated);
	xrtHttpCacheRecordRelease(pHit);
}

xrtHttpCacheRecordRelease(pRecord);
xrtHttpCacheRelease(pCache);
```

完整可运行范例位于 `examples/http/cache_store/main.c`。

## 历史资产

旧版 `dev/ver1` 没有可直接迁移的 HTTP 客户端缓存存储后端。本模块复用了已经
重构并压实的 HTTP 字段、`Vary`、缓存策略、验证、Map、Mutex、分配器和引用计数
能力；旧版 `lib/xweb.h` 的静态资源 ETag、条件请求、Range 处理及
`test/test_xweb.h` 的边界用例作为后续客户端自动缓存集成的行为参考，而不是被
重复复制进存储容器。

## 规范依据

- RFC 9111 2：主缓存键至少包含请求方法和目标 URI。
- RFC 9111 4.1：`Vary` 选择、缺失字段和星号规则。
- RFC 9111 3.3、3.4：不完整响应和部分响应组合。
- RFC 9111 4：多个匹配响应应选择最新项。
- RFC 9111 7.2：可通过额外分区键降低跨站计时信息泄露。
