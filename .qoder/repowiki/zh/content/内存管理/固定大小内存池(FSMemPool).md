# 固定大小内存池(FSMemPool)

<cite>
**本文档引用的文件**
- [lib/mempool_fs.h](file://lib/mempool_fs.h)
- [lib/memunit.h](file://lib/memunit.h)
- [lib/bsmm.h](file://lib/bsmm.h)
- [lib/base.h](file://lib/base.h)
- [docs/api-mempool-fs.md](file://docs/api-mempool-fs.md)
- [test/test_mempool_fs.h](file://test/test_mempool_fs.h)
</cite>

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构总览](#架构总览)
5. [详细组件分析](#详细组件分析)
6. [依赖关系分析](#依赖关系分析)
7. [性能考量](#性能考量)
8. [故障排查指南](#故障排查指南)
9. [结论](#结论)
10. [附录](#附录)

## 简介
FSMemPool 是基于 MemUnit 的高性能固定大小对象内存池，专为“频繁分配/释放、对象大小固定”的场景设计，具备以下关键特性：
- 无限容量：通过 BSMM 动态扩展，无 256 限制
- 四链表分组：Idle/Full/Null/Free 分类管理 MemUnit，提升分配效率
- O(1) 分配：大多数情况下常数时间分配
- 智能复用：空单元缓存，避免临界状态抖动
- GC 支持：支持标记-清除垃圾回收

## 项目结构
FSMemPool 的实现位于 lib 目录，配合 MemUnit、BSMM、基础内存接口等模块协同工作。

```mermaid
graph TB
subgraph "内存池层"
FS["FSMemPool<br/>固定大小内存池"]
end
subgraph "单元管理层"
MU["MemUnit<br/>单个256槽位单元"]
end
subgraph "数组管理器"
BSM["BSMM<br/>块结构内存管理器"]
end
subgraph "基础内存"
BM["xrtMalloc/xrtFree<br/>基础分配接口"]
end
FS --> MU
FS --> BSM
MU --> BM
BSM --> BM
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L1-L257)
- [lib/memunit.h](file://lib/memunit.h#L1-L143)
- [lib/bsmm.h](file://lib/bsmm.h#L1-L94)
- [lib/base.h](file://lib/base.h#L1-L132)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L1-L257)
- [lib/memunit.h](file://lib/memunit.h#L1-L143)
- [lib/bsmm.h](file://lib/bsmm.h#L1-L94)
- [lib/base.h](file://lib/base.h#L1-L132)

## 核心组件
- xfsmempool_struct：FSMemPool 的核心结构，包含：
  - ItemLength：元素大小
  - arrMMU：MMU_LLNode 数组管理器（由 BSMM 提供）
  - LL_Idle/LL_Full/LL_Null/LL_Free：四类 MemUnit 链表
- MMU_LLNode：MemUnit 的链表节点，保存 Flag、objMMU 指针及双向链接
- MemUnit：单个 256 槽位的内存单元，负责具体元素的分配/释放
- BSMM：块结构内存管理器，按需分配内存页并管理空闲块

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L97-L123)
- [lib/memunit.h](file://lib/memunit.h#L77-L89)
- [lib/bsmm.h](file://lib/bsmm.h#L24-L30)

## 架构总览
FSMemPool 的整体架构围绕“四链表 + MemUnit + BSMM”展开，通过 Flag 位域在元素头部存储所属单元与槽位索引，实现 O(1) 快速定位与释放。

```mermaid
graph TB
A["FSMemPool<br/>xfsmempool_struct"] --> B["arrMMU(BSMM)<br/>MMU_LLNode数组"]
B --> C["LL_Idle<br/>有空闲槽位的单元"]
B --> D["LL_Full<br/>满载单元"]
B --> E["LL_Null<br/>全空单元(最多1个)"]
B --> F["LL_Free<br/>已释放Flag槽位"]
C --> G["MemUnit<br/>256槽位单元"]
D --> G
E --> G
F --> G
G --> H["元素头部<br/>ItemFlag: 高位=单元索引, 低位=槽位索引"]
H --> I["元素数据区"]
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L24-L33)
- [lib/memunit.h](file://lib/memunit.h#L38-L41)
- [lib/bsmm.h](file://lib/bsmm.h#L52-L82)

## 详细组件分析

### 分配算法与四链表策略
FSMemPool 的分配遵循“优先空闲、其次备用、最后新建”的策略，并在满载/清空前进行链表重分类，以维持高效访问。

```mermaid
flowchart TD
START(["进入 xrtFSMemPoolAlloc"]) --> CHECK_IDLE{"LL_Idle 是否存在？"}
CHECK_IDLE --> |否| TRY_NULL{"是否有 LL_Null 备用单元？"}
TRY_NULL --> |是| USE_NULL["复用 LL_Null 单元<br/>加入 LL_Idle"]
TRY_NULL --> |否| TRY_FREE{"是否有 LL_Free 可复用Flag？"}
TRY_FREE --> |是| REUSE_FLAG["创建新 MemUnit 并复用 Flag 位置<br/>加入 LL_Idle"]
TRY_FREE --> |否| NEW_MMU["创建全新 MemUnit<br/>加入 LL_Idle"]
CHECK_IDLE --> |是| GET_IDLE["从 LL_Idle 取出单元"]
GET_IDLE --> CHECK_FULL{"单元是否接近满载？"}
CHECK_FULL --> |是| MOVE_FULL["从 LL_Idle 移至 LL_Full"]
CHECK_FULL --> |否| CONTINUE["继续分配"]
USE_NULL --> CONTINUE
REUSE_FLAG --> CONTINUE
NEW_MMU --> CONTINUE
MOVE_FULL --> ALLOC["调用 MemUnit 分配元素"]
CONTINUE --> ALLOC
ALLOC --> END(["返回元素指针"])
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L52-L125)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L52-L125)

### 释放与回收机制
释放时通过元素头部的 ItemFlag 快速定位所属 MemUnit 与槽位索引，随后调用 MemUnit 的释放逻辑，并根据单元状态将其移动到 Idle/Null/Free 链表。

```mermaid
sequenceDiagram
participant U as "用户代码"
participant P as "FSMemPool"
participant MU as "MemUnit"
participant B as "BSMM"
U->>P : xrtFSMemPoolFree(p)
P->>P : 解析 ItemFlag 获取单元索引与槽位
P->>MU : xrtMemUnitFreeIdx(idx)
MU-->>P : 释放成功
P->>P : 若单元曾满载 -> 加入 LL_Idle
P->>P : 若单元已清空 -> 加入 LL_Null 或 LL_Free
Note over P : 通过 ClearCheck/IdleCheck 实现链表重分类
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L199-L221)
- [lib/memunit.h](file://lib/memunit.h#L44-L86)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L199-L221)
- [lib/memunit.h](file://lib/memunit.h#L44-L86)

### GC 回收流程
FSMemPool 支持 GC 标记-清除，遍历 Idle/Full 链表对 MemUnit 执行 GC，再根据单元剩余计数进行链表重分类。

```mermaid
sequenceDiagram
participant U as "用户代码"
participant P as "FSMemPool"
participant MU as "MemUnit"
U->>P : xrtFSMemPoolGC(bFreeMark)
loop 遍历 LL_Idle + LL_Full
P->>MU : xrtMemUnitGC(bFreeMark)
MU-->>P : 返回回收计数
end
P->>P : 重新分类：空->LL_Null/LL_Free；有空->LL_Idle
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L224-L254)
- [lib/memunit.h](file://lib/memunit.h#L89-L140)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L224-L254)
- [lib/memunit.h](file://lib/memunit.h#L89-L140)

### 内存块预分配与池大小确定
- MemUnit 预分配：每个 MemUnit 固定 256 个槽位，元素头部额外 4 字节用于 ItemFlag
- BSMM 扩展：当 arrMMU 的可用槽位不足时，BSMM 按步长 64 申请新的内存页
- 池大小上限：理论上无上限，受系统内存限制

```mermaid
classDiagram
class xfsmempool_struct {
+uint32 ItemLength
+xbsmm_struct arrMMU
+MMU_LLNode* LL_Idle
+MMU_LLNode* LL_Full
+MMU_LLNode* LL_Null
+MMU_LLNode* LL_Free
}
class MMU_LLNode {
+uint32 Flag
+xmemunit objMMU
+Prev
+Next
}
class xmemunit {
+uint32 ItemLength
+uint8 Count
+uint8 FreeCount
+uint8 FreeOffset
+uint32 Flag
}
class xbsmm_struct {
+uint32 ItemLength
+uint32 Count
+xptrarray PageMMU
+MemPtr_LLNode* LL_Free
}
xfsmempool_struct --> MMU_LLNode : "管理"
MMU_LLNode --> xmemunit : "指向"
xfsmempool_struct --> xbsmm_struct : "数组管理"
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L97-L123)
- [lib/memunit.h](file://lib/memunit.h#L5-L19)
- [lib/bsmm.h](file://lib/bsmm.h#L24-L30)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L24-L33)
- [lib/memunit.h](file://lib/memunit.h#L5-L19)
- [lib/bsmm.h](file://lib/bsmm.h#L24-L30)

### 适用场景与参数调优
- 适用场景
  - 高频对象分配与释放（如消息、事件、链表节点）
  - 对象大小固定且数量可能超过 256
  - 需要 GC 支持的“对象堆”
- 参数调优建议
  - 初始池大小：根据峰值并发对象数估算，确保 LL_Idle 链表足够长
  - 预热：在业务启动阶段批量分配/释放一次，减少运行期扩展
  - GC 频率：结合业务生命周期，定期执行 GC 回收未使用对象
  - 注意事项：避免跨池释放同一对象，防止未定义行为

**章节来源**
- [docs/api-mempool-fs.md](file://docs/api-mempool-fs.md#L438-L632)

## 依赖关系分析
FSMemPool 的依赖关系清晰，职责分离明确：

```mermaid
graph LR
FS["FSMemPool<br/>lib/mempool_fs.h"] --> MU["MemUnit<br/>lib/memunit.h"]
FS --> BSM["BSMM<br/>lib/bsmm.h"]
MU --> BM["基础内存接口<br/>lib/base.h"]
BSM --> BM
```

**图表来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L1-L257)
- [lib/memunit.h](file://lib/memunit.h#L1-L143)
- [lib/bsmm.h](file://lib/bsmm.h#L1-L94)
- [lib/base.h](file://lib/base.h#L1-L132)

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L1-L257)
- [lib/memunit.h](file://lib/memunit.h#L1-L143)
- [lib/bsmm.h](file://lib/bsmm.h#L1-L94)
- [lib/base.h](file://lib/base.h#L1-L132)

## 性能考量
- 时间复杂度
  - 分配/释放：O(1)，主要为链表操作与数组索引
  - GC：O(N)（N 为当前活跃 MemUnit 数），但仅在必要时触发
- 空间复杂度
  - 每个 MemUnit 固定 256 槽位，元素头部 4 字节 ItemFlag
  - BSMM 按需扩展，步长 64，平衡内存与扩展成本
- 优化建议
  - 预分配：在高负载前批量预热，降低运行期扩展次数
  - 批量释放：尽量顺序释放，减少链表重分类开销
  - GC 频率控制：结合业务特征，避免过于频繁的 GC

[本节为通用性能讨论，无需特定文件分析]

## 故障排查指南
- 常见问题
  - 跨池释放：释放到错误的池导致未定义行为
  - 重复释放：同一元素多次释放
  - 未初始化：未正确创建/初始化池
- 排查步骤
  - 检查释放指针是否来自同一池
  - 使用 GC 标记辅助定位泄漏对象
  - 观察 LL_Idle/LL_Full/LL_Null/LL_Free 的变化趋势
- 相关接口
  - xrtFSMemPoolCreate/xrtFSMemPoolDestroy：池生命周期管理
  - xrtFSMemPoolAlloc/xrtFSMemPoolFree：分配/释放
  - xrtFSMemPoolGC：GC 回收

**章节来源**
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L5-L21)
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L199-L221)
- [lib/mempool_fs.h](file://lib/mempool_fs.h#L224-L254)

## 结论
FSMemPool 通过“四链表 + MemUnit + BSMM”的组合，在固定大小对象的高频分配场景下实现了 O(1) 的分配性能与良好的内存局部性。其无限容量扩展能力与 GC 支持使其适用于需要长期运行、对象规模较大的系统。合理调参与预热可进一步提升稳定性与吞吐。

[本节为总结性内容，无需特定文件分析]

## 附录

### 使用示例与最佳实践
- 基本使用
  - 创建池：指定元素大小
  - 分配/释放：循环批量操作
  - 销毁池：释放所有资源
- 最佳实践
  - 保持对象大小一致
  - 避免跨池释放
  - 合理安排 GC 周期
  - 预热与监控链表状态

**章节来源**
- [docs/api-mempool-fs.md](file://docs/api-mempool-fs.md#L128-L347)
- [docs/api-mempool-fs.md](file://docs/api-mempool-fs.md#L602-L702)