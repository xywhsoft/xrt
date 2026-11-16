# XTP 3.0 消息 API 与回调设计

日期：2026-11-16。状态：API 实施设计，协议实现尚未开始。线路格式和生命周期以 [XTP 3.0 扩展库设计方案](xtp3.md) 为准。

四种形态共用一个消息描述、一个 Submit 入口和一个业务回调。Command 与 Params 分别带类型标签，库按标签生成线路 flags。调用者不填写原始 flags，不为四种组合维护四套发送或接收函数。

本方案中的 ParamID 指从 0 开始的参数位置下标，由业务 enum 定义名称，array 和 table 都适用，不是在线上发送的 UInt32 稀疏 key。例如 `MOVE_X=1` 指第 1 项；`ParamID=1001` 不会自动成为一项名为 1001 的参数。稀疏数字 key 是第三种参数格式，不纳入本版。

## 1 调用形态

| Command | Params | 构造方式 | 接收端访问 |
| --- | --- | --- | --- |
| UInt32 | array | CmdID + Array | Command.As.ID + ParamAt |
| UInt32 | table | CmdID + Table | Command.As.ID + ParamAt 或 ParamFind |
| 字符串 | array | CmdName + Array | Command.As.Name + ParamAt |
| 字符串 | table | CmdName + Table | Command.As.Name + ParamAt 或 ParamFind |

数字命令零值合法，必须通过 Kind 区别于无命令。零初始化的 Params 是空 table；零初始化的 Command 是 NONE，不能提交为有路由消息。

发送描述借用调用者的数据，仅需保持到 Submit/Reply 返回。基础接口成功后，Session 已编码并复制到自己的有界存储；失败时不持有输入。接收端返回借用视图，不把发送描述直接当成接收对象，也不建立逐参数堆对象。

## 2 数据类型和声明

本节是可进行语法检查的声明原型。公开字段采用 XRT 的 Data/Size 视图；以下类型不代表线路结构，不使用其 sizeof 计算包长。带下划线的字段由库初始化，应用不得构造、修改或跨失效期使用。

```c
#include <xrt/core.h>
#include <string.h>

typedef struct xtpsession xtpsession;
typedef struct xtpfault xtpfault;

typedef enum xtpcommandkind {
    XTP_COMMAND_NONE = 0,
    XTP_COMMAND_ID = 1,
    XTP_COMMAND_NAME = 2
} xtpcommandkind;

typedef struct xtpcommand {
    xtpcommandkind Kind;
    union {
        uint32_t ID;
        xstrview Name;
    } As;
} xtpcommand;

typedef enum xtpparamkind {
    XTP_PARAMS_TABLE = 0,
    XTP_PARAMS_ARRAY = 1
} xtpparamkind;

typedef struct xtpfield {
    xstrview Key;
    xbytesview Value;
} xtpfield;

/* 发送参数：只读取 Kind 对应的联合成员。 */
typedef struct xtpparams {
    xtpparamkind Kind;
    size_t Count;
    union {
        const xtpfield* Table;
        const xbytesview* Array;
    } As;
} xtpparams;

/* 接收参数：_Wire 为经过完整校验的参数编码，禁止应用自行伪造。 */
typedef struct xtpparamsview {
    xtpparamkind Kind;
    size_t Count;
    xbytesview _Wire;
} xtpparamsview;

typedef enum xtpmessagekind {
    XTP_REQUEST = 1,
    XTP_RESPONSE = 2,
    XTP_PUSH = 3,
    XTP_ACK = 4,
    XTP_NOTIFY = 5
} xtpmessagekind;

typedef enum xtpstatus {
    XTP_STATUS_OK = 0,
    XTP_STATUS_BUSY = -1,
    XTP_STATUS_REJECTED = -2,
    XTP_STATUS_UNSUPPORTED = -3,
    XTP_STATUS_BAD_ARGUMENT = -4,
    XTP_STATUS_RESULT_TOO_LARGE = -5,
    XTP_STATUS_INTERNAL = -6,
    XTP_STATUS_NOT_FOUND = -7,
    XTP_STATUS_STATE_LOST = -8,
    XTP_STATUS_DEADLINE_EXCEEDED = -9
} xtpstatus;

typedef enum xtpresult {
    XTP_OK = 0,
    XTP_AGAIN = 1,
    XTP_CLOSED = 2,
    XTP_E_ARGUMENT = -1,
    XTP_E_LIMIT = -2,
    XTP_E_MEMORY = -3,
    XTP_E_STATE = -4
} xtpresult;

/* Submit 只接受 REQUEST/PUSH/NOTIFY；MessageID 由 Session 分配。 */
typedef struct xtpmessage {
    xtpmessagekind Type;
    xtpcommand Command;
    xtpparams Params;
    xbytesview Body;
} xtpmessage;

/* Reply 没有 Command、MessageID 或 Type，避免构造错误关联的应答。 */
typedef struct xtpresponse {
    int32_t Status;
    xtpparams Params;
    xbytesview Body;
} xtpresponse;

typedef struct xtprequesttoken { uint64_t _Opaque[4]; } xtprequesttoken;
typedef struct xtpoperationtoken { uint64_t _Opaque[2]; } xtpoperationtoken;

/* 只交付入站 REQUEST/PUSH/NOTIFY。Request 仅在 REQUEST 时有效。 */
typedef struct xtpmessageview {
    xtpmessagekind Type;
    uint64_t MessageID;
    xtpcommand Command;
    xtpparamsview Params;
    xbytesview Body;
    xbytesview Packet;         /* 整个输入帧的借用视图，便于显式复制。 */
    xtprequesttoken Request;
} xtpmessageview;

typedef enum xtpaction {
    XTP_ACTION_ACCEPT = 1,
    XTP_ACTION_BUSY = 2,
    XTP_ACTION_REJECT = 3,
    XTP_ACTION_PAUSE = 4
} xtpaction;

typedef struct xtpdecision {
    xtpaction Action;
    int32_t Status;
} xtpdecision;

typedef enum xtpfinish {
    XTP_FINISH_RESPONSE = 1,
    XTP_FINISH_ACK,
    XTP_FINISH_SENT,
    XTP_FINISH_TIMEOUT,
    XTP_FINISH_CLOSED,
    XTP_FINISH_LOCAL_ERROR
} xtpfinish;

typedef enum xtpknowledge {
    XTP_KNOW_NOT_SENT = 1,
    XTP_KNOW_UNKNOWN,
    XTP_KNOW_ACCEPTED,
    XTP_KNOW_REPLIED,
    XTP_KNOW_REJECTED
} xtpknowledge;

typedef struct xtpcompletion {
    xtpoperationtoken Operation;
    xtpmessagekind Type;       /* 原出站 REQUEST/PUSH/NOTIFY。 */
    uint64_t MessageID;
    xtpfinish Reason;
    xtpknowledge Knowledge;
    xtpresult LocalResult;    /* 本地错误；无本地错误时为 XTP_OK。 */
    bool HasRemoteStatus;
    int32_t Status;
    xtpparamsview Params;      /* RESPONSE 的结果参数；其他情况为空。 */
    xbytesview Body;           /* RESPONSE 的结果 Body；其他情况为空。 */
    xbytesview Packet;         /* RESPONSE 整帧；其他完成原因为空。 */
    void* Tag;                /* 原提交者提供的本地关联值，不上线。 */
} xtpcompletion;

typedef struct xtpevents {
    xtpdecision (*OnMessage)(xtpsession*, const xtpmessageview*, void* User);
    void (*OnComplete)(xtpsession*, const xtpcompletion*, void* User);
    void (*OnFault)(xtpsession*, const xtpfault*, void* User);
} xtpevents;

typedef struct xtpsubmitoptions {
    uint64_t DeadlineUs;      /* 0 继承实例有限默认期限，不是无限。 */
    uint32_t LaneID;          /* 本地 lane；0 选择实例默认 lane。 */
    bool Replay;             /* 仅 REQUEST/PUSH；要求 Delivery 已约定。 */
    void* Tag;                /* 借用到完成回调结束；提交失败不持有。 */
} xtpsubmitoptions;

typedef enum xtpparamresult {
    XTP_PARAM_FOUND = 1,
    XTP_PARAM_MISSING = 0,
    XTP_PARAM_WRONG_MODE = -1,
    XTP_PARAM_ARGUMENT = -2
} xtpparamresult;

typedef struct xtpparamiter {
    const xtpparamsview* _Params;
    size_t _Next;
} xtpparamiter;

typedef struct xtpparamentry {
    size_t Index;
    bool HasKey;
    xstrview Key;
    xbytesview Value;
} xtpparamentry;

XRT_EXTERN_C_BEGIN
xtpresult xtpSubmit(xtpsession* Session, const xtpmessage* Message,
    const xtpsubmitoptions* Options, uint64_t NowUs,
    xtpoperationtoken* Operation);
xtpresult xtpSubmitBatch(xtpsession* Session, const xtpmessage* Messages,
    size_t Count, const xtpsubmitoptions* Options, uint64_t NowUs,
    xtpoperationtoken* Operations);
xtpresult xtpReply(xtpsession* Session, xtprequesttoken Request,
    const xtpresponse* Response, uint64_t NowUs);
xtpparamresult xtpParamAt(const xtpparamsview* Params, size_t Index,
    xbytesview* Value);
xtpparamresult xtpParamFind(const xtpparamsview* Params, xstrview Key,
    xbytesview* Value);
xtpparamiter xtpParams(const xtpparamsview* Params);
xtpparamresult xtpParamNext(xtpparamiter* Iter, xtpparamentry* Entry);
XRT_EXTERN_C_END

/* 以下为按值返回的小型 static inline helper，不分配、不注册路由。 */
static inline xtpcommand xtpCmdID(uint32_t ID)
{
    xtpcommand c = { XTP_COMMAND_ID, { 0 } };
    c.As.ID = ID;
    return c;
}
static inline xtpcommand xtpCmdName(xstrview Name)
{
    xtpcommand c = { XTP_COMMAND_NAME, { 0 } };
    c.As.Name = Name;
    return c;
}
static inline xtpparams xtpArray(const xbytesview* Items, size_t Count)
{
    xtpparams p = { XTP_PARAMS_ARRAY, 0, { NULL } };
    p.Count = Count;
    p.As.Array = Items;
    return p;
}
static inline xtpparams xtpTable(const xtpfield* Items, size_t Count)
{
    xtpparams p = { XTP_PARAMS_TABLE, 0, { NULL } };
    p.Count = Count;
    p.As.Table = Items;
    return p;
}
static inline bool xtpCommandIsName(const xtpcommand* Command, xstrview Name)
{
    return Command != NULL && Command->Kind == XTP_COMMAND_NAME &&
        Command->As.Name.Size == Name.Size &&
        (Name.Size == 0 || (Name.Data != NULL &&
        Command->As.Name.Data != NULL &&
        memcmp(Command->As.Name.Data, Name.Data, Name.Size) == 0));
}
static inline xtpdecision xtpAccept(void)
{
    xtpdecision d = { XTP_ACTION_ACCEPT, 0 };
    return d;
}
static inline xtpdecision xtpBusy(void)
{
    xtpdecision d = { XTP_ACTION_BUSY, XTP_STATUS_BUSY };
    return d;
}
static inline xtpdecision xtpReject(int32_t Status)
{
    xtpdecision d = { XTP_ACTION_REJECT, Status };
    return d;
}
static inline xtpdecision xtpPause(void)
{
    xtpdecision d = { XTP_ACTION_PAUSE, 0 };
    return d;
}
```

Kind 是本地 API 枚举，不直接当成 wire bit 使用。可选模块的开关不改变这些公共类型的布局；没有启用的恢复或引用能力在提交时明确拒绝。

## 3 发送方式

下面的三项参数都已由应用按业务 schema 编码成 little-endian 字节；库不会对本机 UInt32、float 或指针直接 memcpy 后声称它们可跨平台互通。

```c
xtpresult example_send_one(xtpsession* s, const xbytesview* args,
    size_t count, uint64_t now)
{
    xtpmessage m = { 0 };
    m.Type = XTP_REQUEST;
    m.Command = xtpCmdID(1001);
    m.Params = xtpArray(args, count);
    return xtpSubmit(s, &m, NULL, now, NULL);
}

xtpresult example_send_four_shapes(xtpsession* s, uint64_t now)
{
    const unsigned char tick[] = { 1, 0, 0, 0 };
    const unsigned char dx[] = { 2, 0 };
    const unsigned char dy[] = { 3, 0 };
    const xbytesview args[] = {
        { tick, sizeof tick }, { dx, sizeof dx }, { dy, sizeof dy }
    };
    const xtpfield fields[] = {
        { XRT_STR_INIT("tick"), { tick, sizeof tick } },
        { XRT_STR_INIT("dx"),   { dx, sizeof dx } },
        { XRT_STR_INIT("dy"),   { dy, sizeof dy } }
    };
    xtpmessage m[4] = { { 0 } };
    size_t i;
    for (i = 0; i < 4; ++i) m[i].Type = XTP_REQUEST;
    m[0].Command = xtpCmdID(1001);
    m[0].Params = xtpArray(args, 3);
    m[1].Command = xtpCmdID(1001);
    m[1].Params = xtpTable(fields, 3);
    m[2].Command = xtpCmdName(XRT_STR_LITERAL("player.move"));
    m[2].Params = xtpArray(args, 3);
    m[3].Command = xtpCmdName(XRT_STR_LITERAL("player.move"));
    m[3].Params = xtpTable(fields, 3);
    return xtpSubmitBatch(s, m, 4, NULL, now, NULL);
}
```

example_send_one 展示常用的单条调用，example_send_four_shapes 展示四种模式的原子批量受理。后者是 API 组合示例；应用通常为一个 Command 固定一种参数 schema，不在每条消息上任意切换后要求对方猜测语义。

Submit 的 Options=NULL 使用实例默认的有限期限、默认 lane 和不恢复策略；Operation=NULL 表示调用者不需要操作令牌，不影响正常完成通知。Message 只接受 REQUEST/PUSH/NOTIFY，Status 和 MessageID 无法由这个描述误填。

SubmitBatch 的 Messages 和非空 Options/Operations 均为 Count 项；Options=NULL 表示全部默认。Count 必须大于零且受批量上限约束。成功时整批被 Session 接管；任何失败保留所有输入与输出令牌，并且不发布部分操作。单条 Submit 与 Count=1 的 Batch 使用相同的校验与受理实现。

调用者传入的 fields 顺序就是线上顺序和接收端的下标顺序，编码器不重新排列参数。预检可以使用临时索引检查重复 key，但该索引不影响消息顺序。空参数可以保持零初始化，也可以 Array(NULL,0) 表达空 array。两者均不创建 Count=0 的 Metadata，但 PARAM_ARRAY 标志仍能表达空 array 模式。

XRT_STR_LITERAL/XRT_STR_INIT 只用于字符串字面量或真实字符数组；动态字符串指针使用显式 `{ptr, length}` 的 xstrview，不把指针 sizeof 当作字符串长度。所有接收字符串均不承诺 NUL 结尾。

## 4 一个业务回调

OnMessage 接收统一的 xtpmessageview。Command.Kind 表达路由形态，Params.Kind 表达参数形态，两者相互独立。应用只读取匹配的联合成员；不靠 ID 是否为零、指针是否为空或 Body 内容猜模式。

```c
typedef struct example_context {
    uint64_t (*NowUs)(void* Clock);
    void* Clock;
} example_context;

enum example_move_param {
    MOVE_TICK = 0, MOVE_DX = 1, MOVE_DY = 2, MOVE_PARAM_COUNT = 3
};

static xtpdecision example_move(xtpsession* s, const xtpmessageview* m,
    example_context* ctx)
{
    xbytesview tick, dx, dy;
    if (m->Params.Count != MOVE_PARAM_COUNT)
        return xtpReject(XTP_STATUS_BAD_ARGUMENT);

    /* 两种参数模式按同一业务顺序读取，不需要分支。 */
    if (xtpParamAt(&m->Params, MOVE_TICK, &tick) != XTP_PARAM_FOUND ||
        xtpParamAt(&m->Params, MOVE_DX, &dx) != XTP_PARAM_FOUND ||
        xtpParamAt(&m->Params, MOVE_DY, &dy) != XTP_PARAM_FOUND)
        return xtpReject(XTP_STATUS_BAD_ARGUMENT);
    if (tick.Size != 4 || dx.Size != 2 || dy.Size != 2)
        return xtpReject(XTP_STATUS_BAD_ARGUMENT);

    /* 示例只校验并回显；实际移动逻辑由应用替换。 */
    if (m->Type == XTP_REQUEST) {
        const xbytesview result[] = { tick, dx, dy };
        xtpresponse response = { 0 };
        xtpresult r;
        response.Params = xtpArray(result, 3);
        r = xtpReply(s, m->Request, &response, ctx->NowUs(ctx->Clock));
        if (r != XTP_OK) return xtpReject(XTP_STATUS_INTERNAL);
    }
    return xtpAccept();
}

xtpdecision example_on_message(xtpsession* s, const xtpmessageview* m,
    void* user)
{
    example_context* ctx = (example_context*)user;
    if (m->Command.Kind == XTP_COMMAND_ID) {
        switch (m->Command.As.ID) {
        case 1001: return example_move(s, m, ctx);
        default: return xtpReject(XTP_STATUS_NOT_FOUND);
        }
    }
    if (xtpCommandIsName(&m->Command, XRT_STR_LITERAL("player.move")))
        return example_move(s, m, ctx);
    return xtpReject(XTP_STATUS_NOT_FOUND);
}
```

这个 handler 同时支持两种命令和两种参数。发送示例中 args 和 fields 的顺序均为 tick、dx、dy，所以同一组 ParamAt 调用适用于四种组合。应用仍可按需限制 Params.Kind；无需建立通用路由、运行时 schema 或动态参数对象。

ID=1001 和字符串 player.move 的关联是示例中的应用决定，XTP 不维护该映射。array/table 的第 0 项对应 tick，也由业务 schema 明确规定。ParamAt 只按实际位置取值，不隐含 key 匹配；如果业务需要同时校验 table 名称和顺序，可以通过 ParamNext 一次检查 Index/Key，或主动使用 ParamFind，不能要求 XTP 猜测命令 schema。

### 4.1 决策与所有权

| 返回值 | REQUEST | PUSH | NOTIFY |
| --- | --- | --- | --- |
| Accept | 已同步 Reply 或接管异步应答令牌 | 已处理或排入应用有界队列，生成成功 ACK | 已处理，或所需数据已保留 |
| Busy | 尚未受理，返回 BUSY RESPONSE | 尚未接管，返回 BUSY ACK | 丢弃，不回包 |
| Reject(code) | 尚未受理，返回错误 RESPONSE | 尚未接管，返回拒绝 ACK | 丢弃，不回包 |
| Pause | 本次无业务效果，保留当前帧等待显式恢复 | 同左，不生成 ACK | 同左，不生成 ACK |

Reject 的 code 必须是主协议允许的非零拒绝码；不能以 STATE_LOST 或 DEADLINE_EXCEEDED 冒充业务未受理。BUSY 使用 Busy helper。ACCEPT/PAUSE 的 Status 必须零；Action 与 Status 的非法组合属于本地回调契约错误，不上线。

已成功 Reply 后必须返回 Accept。已经产生业务效果的失败应通过相应业务结果表达，不能返回 Busy、Reject 或 Pause 声称尚未受理。

Request 是可复制的不透明令牌，经过合法续接仍可指向原逻辑请求；它不保留 Params、Body 或 Packet。异步处理者在返回前复制所需字段，或复制 Packet 后重新 Decode 建立副本视图，并保证 Session/回调代码存活。可选引用模块必须提供正式的所有权接口，不能靠复制 view 保留数据。完成 Reply 时使用新的 NowUs，不复用收到消息时的旧时钟。

### 4.2 结果回调

OnMessage 不重复发布 RESPONSE/ACK；Session 将它们关联到原操作，通过 OnComplete 交付结果。普通 PUSH 的 ACK、RESULT_ACK、重复消息及恢复细节不要求应用再写一轮分流代码。

OnComplete 通过 Operation 或 Tag 关联本地业务。Reason 区分 RESPONSE、ACK、本地发送完成、超时、关闭及本地错误；Knowledge 继续表达 NOT_SENT/UNKNOWN/ACCEPTED/REPLIED/REJECTED。LocalResult 表达本地错误，Status 仅在 HasRemoteStatus=true 时可解释为远端返回。

RESPONSE 的 Params/Body 与 OnMessage 使用相同的视图和读取函数。应答无 Command，结果参数模式可以与请求不同；例如 table 请求返回 array 结果，只要应用 schema 已约定。ACK、本地超时等没有结果内容，Params 为空 table，Body 为空。

同一成功提交的操作最多完成一次；Submit 失败不触发 OnComplete，不接管 Tag。回调内的视图不得直接存入异步任务。STATE_LOST 仍按主协议报告 UNKNOWN，不因有远端 Status 就伪装成确定的业务结果。

## 5 参数读取的精确语义

| 入口 | array | table | 成本 |
| --- | --- | --- | --- |
| ParamAt(params,index,out) | 按输入顺序读取 | 按输入顺序读取 Value，跳过 Key | 校验类型和边界后 O(1)，不比较 key |
| ParamFind(params,key,out) | WRONG_MODE | 按输入顺序匹配完整 key | 最多 O(Count) 次有界字节比较 |
| Params + ParamNext | 输入顺序，HasKey=false | 输入顺序，HasKey=true | 每项偏移读取 O(1)，整次 O(Count) |

ParamAt 的 Index 始终是发送时的第几项，从零开始；table 和 array 使用同一语义。ParamFind 不把字符串数字转换成数组下标，也不在未找到 key 时回退为位置读取。

FOUND 可以返回 Size=0 的实际空值；MISSING 才表示越界、未找到或迭代结束。有效的 table 调用 ParamAt 不再返回 WRONG_MODE；ParamFind 用于 array 时仍返回 WRONG_MODE，并与 MISSING 分开。所有非 FOUND 返回在输出指针有效时将输出清零。

ParamAt/Find/Next 只接受库产生且仍有效的 ParamsView。其参数区域已在交付前完整校验，不在每次查询时重复扫描整个表；仍保留模式、输出指针和索引范围等局部检查。

迭代器与 ParamsView 的寿命相同。array 和 table 的 Index 都与发送时的输入位置一致，可由业务约定为 ParamID；它不是从 key 推导的 ID。库不执行参数重排，改变输入顺序就是改变位置语义。

所有读取返回原始字节视图。数值解码按业务 schema 显式检查长度和字节序；第一版不生成 ParamIntByID、ParamIntByName、ParamFloatByID 等组合函数。

## 6 设计约束

- 一套结构表达组合，不增加 SendIDArray、SendNameArray、SendIDTable、SendNameTable。
- 一个 OnMessage 表达入站差异，业务自行决定是否接受一种或多种形态。
- 四个按值构造 helper 不分配、不重排参数、不调用 strlen；实际校验发生在编码或提交预检中，临时去重索引不改变参数输入和线路顺序。
- 零号 CommandID、空参数和空值都具有明确表达，不借用零值猜类型。
- 发送 descriptor 和接收 view 分开，Response 与有路由消息分开，令牌与线路 MessageID 分开。
- 不使用可变参数、void* 值装箱、字符串/数字隐式转换、协议内路由表或基于 Opcode 的通用调用器。
- 不增加线路字段，ROUTE_ID/PARAM_ARRAY 继续由两个独立 bit 表示；32 字节固定头不变。

## 7 验证范围

以文档中的声明原型和 C 示例进行编译期检查，核对四种模式可表达、回调签名、Reply 的无路由描述以及 C 工具链兼容性。该检查不实现 Session、参数解码或网络，不构成运行时功能和性能验收。

正式 P1～P3 另外测试：零号 CommandID、错误 Kind、ParamFind 的 WRONG_MODE 与 MISSING、实际空值、动态字符串长度、非 NUL 结尾输入、非字典序 table 的原顺序保留、table 的 ParamAt/ParamFind 结果一致、ParamNext 顺序、重复 key 拒绝、批量失败原子性、Reply 令牌失效、Tag 寿命、回调重入及完成恰好一次。
