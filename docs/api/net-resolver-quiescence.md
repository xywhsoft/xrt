# Resolver 拥有图的暂时忙碌判定

`xrtNetResolverOwnershipQuiescenceV1Get`、
`xrtNetResolveOpOwnershipQuiescenceV1Get`、
`xrtNetResolverFutureOwnershipQuiescenceV1Get` 返回不可变的
`xrtownershipquiescencev1`。宿主应在 ownership Freeze 内先按精确 `Ops`
匹配，再调用 `Busy`；结果为 true 时保留原图并等待，不能判作不支持的节点。

结果就绪不代表回调栈退出，也不代表原生线程及 TLS 清理完成。三个策略分别
描述 Resolver 的执行／停泊／退出状态、操作完成回调、Future producer 的发布／
回调与桥接 Watch 转换。查询只读，不关闭、取消、join 或分配，不产生虚构引用。

`Busy == false` 不是接纳许可。仍必须用原有 Count／Trace 和带精确策略身份的
adapter 验证；未知自定义回调不因为这三个接口而获得认证。原有 Count／Trace、
Prepare／Ready 及 Clear／Finish 的严格条件没有改变。空指针 Busy 为 false。

本次验收仅运行现有 `test_net_resolver_future_ownership` 的
`active`、`exit`、`pending`、`unknown` 四个选择器，输入为数字回环地址。
`active` 在真实回调中检查三个 Busy 和 Count 拒绝；`exit` 用真实线程局部析构
阻挡退出，证明诊断不被修改且必须等到物理线程完成才能转为非忙碌。
没有运行通用网络套件、外部 DNS、通信或安全性质测试。

专项入口为 `tools/check_net_resolver_quiescence.py`；覆盖模块化／单头、
GCC／TCC O0／O2 以及 Linux 实际闭包 ASan／UBSan。
