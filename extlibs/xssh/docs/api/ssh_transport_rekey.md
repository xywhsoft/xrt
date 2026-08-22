# SSH Rekey 策略

`ssh_transport_rekey` 是无分配、无时钟依赖的纯状态层。调用方显式传入单调毫秒时间，并在每个包
进入密码处理前调用 `xrtSshRekeyReserveSend` 或 `xrtSshRekeyReserveReceive`。

## 默认策略

- 每个方向 1 GiB wire 数据。
- 每代密钥 1 小时。
- 发送和接收软阈值各 `2^31` 包，给 rekey 状态机保留提前量。
- 128 位块密码默认 `2^32` 个块。
- 不可取消的协议硬上限为每方向 `2^32` 包。

软阈值达到后返回 `XSSH_REKEY_RECOMMENDED`，当前包已经被登记，可以继续处理并启动 rekey。
完整硬额度可以使用；额度耗尽后的下一包返回 `XSSH_REKEY_REQUIRED`，状态保持不变，该包不得继续
使用旧密钥。计数采用饱和加法，不会因调用值异常而回绕。

## 生命周期

`xrtSshRekeyInit` 复制默认或自定义策略。`xrtSshRekeyRequest` 可记录应用、算法或对端触发的主动
请求。写密钥和读密钥分别生效时调用 `xrtSshRekeyResetSend` 与 `xrtSshRekeyResetReceive`；双方均已
生效后调用 `xrtSshRekeyComplete` 清除请求标记。方向性重置不会抹掉另一方向已经使用的新代额度。

`xrtSshRekeyReset` 同时重置双向计数和时间，只适用于驱动确实拥有共同提交边界的场景。发送和接收
各自维护代起始时间，任一方向达到时间阈值都会建议 rekey。

时间倒退不会伪造一个巨大的 elapsed 值；transport 必须提供真正的单调时钟。自定义策略可以用零
禁用任一软阈值，但 `HardPacketLimit` 必须在 `1..2^32` 内，不能关闭。
