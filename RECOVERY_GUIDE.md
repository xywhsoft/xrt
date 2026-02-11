# XRT 网络库优化恢复指南

## 📋 恢复前准备

1. **确认文件完整性**
   ```bash
   # 检查关键文件是否存在
   ls -la network_optimization_round2_spec.md
   ls -la network_optimization_context_snapshot.txt
   ls -la RECOVERY_GUIDE.md  # 本文件
   ```

2. **备份当前工作区**（可选但推荐）
   ```bash
   # 创建备份分支
   git checkout -b backup_before_recovery
   git add .
   git commit -m "Backup before account switch"
   ```

---

## 🎯 恢复步骤

### 第一步：理解项目背景

**阅读顺序建议：**
1. `network_optimization_context_snapshot.txt` （快速概览）
2. `network_optimization_round2_spec.md` （详细实施计划）

**重点关注：**
- 当前已完成：Round 1 事件驱动重构（11步）
- 当前进行中：Round 2 深度优化（16个问题）
- 实施计划：12个步骤，按依赖顺序执行

### 第二步：环境检查

```bash
# 1. 确认代码基线
cd d:\git\xrt
git status

# 2. 编译验证当前状态
gcc -m64 -DDEBUG_TRACE test.c xrt.c -lWs2_32 -lIPHLPAPI -O0 -o test_run.exe
./test_run.exe

# 3. 确认测试用例状态
# - Test_TLS_Comprehensive() 应该已启用
# - Test_NetTCPUDP() 可选启用
```

### 第三步：实施计划概览

**16个优化问题分类：**

📌 **P0 关键问题（必须优先解决）**
- P0.1: io_uring wakeup 机制无效（Linux 唤醒问题）
- P0.2: IOCP 0 字节误判（UDP 通信异常）
- P0.3: 操作池内存浪费（32MB 固定分配）
- P0.4: TLS 握手 Sleep 轮询（延迟高）

🔧 **P1 性能优化**
- P1.5: PollRemove 线性扫描
- P1.6: TCP NoDelay 未启用
- P1.7: xnetbuf memmove 瓶颈
- P1.8: io_uring 超时兼容性

🛡️ **P2 健壮性增强**
- P2.9: Stop 未先 PollRemove
- P2.10: Windows KeepAlive 参数缺失
- P2.11: 连接超时硬编码
- P2.12: 无 Graceful Shutdown
- P2.13: UDP Ex 模式无远端地址

🚀 **P3 增强项**
- P3.14-P3.16: 扩容、缓冲区、HTTP 异步化

### 第四步：按步骤实施

**推荐实施顺序：**

```
Step 1: xrt.h — 扩展配置字段
  ↳ 为后续步骤提供新配置项

Step 2: netpoll.h — 操作池动态扩容 (解决 P0.3)
Step 3: netpoll.h — IOCP 0 字节修复 (解决 P0.2)
Step 4: netpoll.h — PollRemove 优化 (解决 P1.5)
Step 5: netpoll.h — io_uring wakeup (解决 P0.1)
Step 6: netpoll.h — io_uring 超时 (解决 P1.8)

Step 7: netsock.h — Windows KeepAlive (解决 P2.10)

Step 8: nettcp.h — TCP 增强 (解决 P1.6, P2.9, P2.11, P2.12)
Step 9: nettcp.h — TLS 去 Sleep (解决 P0.4)

Step 10: netudp.h — UDP 远端地址 (解决 P2.13) 【可选】

Step 11: 小修优化
Step 12: 编译测试
```

### 第五步：每步实施 checklist

**实施前：**
- [ ] 阅读对应 spec 文档的该步骤详情
- [ ] 理解改动目的和影响范围
- [ ] 准备好相关文件的备份

**实施中：**
- [ ] 按 spec 修改代码
- [ ] 保持向后兼容性
- [ ] 注意 TCC 兼容性

**实施后：**
- [ ] 编译验证（gcc + tcc）
- [ ] 运行相关测试用例
- [ ] 记录测试结果
- [ ] git commit（建议每步一个 commit）

---

## 🔧 常用命令参考

### 编译命令
```bash
# GCC 编译（调试模式）
gcc -m64 -DDEBUG_TRACE test.c xrt.c -lWs2_32 -lIPHLPAPI -O0 -o test_run.exe

# TCC 编译（发布模式）
tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -o release/x64/xrt.dll

# 查看编译警告
gcc -m64 -Wall -Wextra test.c xrt.c -lWs2_32 -lIPHLPAPI -O0 -o test_run.exe
```

### 测试命令
```bash
# 运行完整测试
./test_run.exe

# 重点关注的测试项
# - TLS 综合测试（Test_TLS_Comprehensive）
# - TCP/UDP 网络测试（Test_NetTCPUDP）
```

### Git 操作
```bash
# 每步完成后提交
git add .
git commit -m "Step X: 简要说明改动内容"

# 查看当前状态
git status
git log --oneline -10

# 创建里程碑标签
git tag -a v1.2.0-round2-stepX -m "Round 2 Step X completed"
```

---

## ⚠️ 注意事项

### 风险控制
1. **不要同时修改多个文件** - 保持每步专注
2. **及时测试验证** - 每步完成后都要编译运行
3. **保留回滚路径** - 用 git commit 分隔每个步骤

### 兼容性要求
- **Linux**: kernel 5.1+ （io_uring 支持）
- **Windows**: 支持 IOCP 和 TCC 编译器
- **API**: 所有改动必须向后兼容

### 已知限制
- P1.7 (TLS xnetbuf 改 ringbuf) 风险高，本轮不处理
- P3.16 (HTTP 异步化) 范围过大，需独立项目

---

## 📞 问题排查

### 编译失败
1. 检查是否遗漏了某个头文件包含
2. 确认 TCC 兼容性宏定义是否完整
3. 查看具体错误信息定位问题文件

### 测试失败
1. 先确认是新引入的问题还是原有问题
2. 用 git diff 查看具体改动
3. 回滚到上一稳定版本对比

### 性能问题
1. 启用 DEBUG_TRACE 观察日志
2. 重点关注事件循环和 IO 路径
3. 对比优化前后的性能指标

---

## ✅ 完成标准

每个步骤完成后应满足：
- [ ] 零编译错误
- [ ] 零编译警告（或仅为已知的项目级警告）
- [ ] 相关测试用例通过
- [ ] git commit 已提交

全部 12 步完成后应满足：
- [ ] 所有 P0 级问题已解决
- [ ] 主要 P1/P2 问题已解决
- [ ] Test_TLS_Comprehensive() 通过
- [ ] Test_NetTCPUDP() 通过
- [ ] Linux 和 Windows 平台均能正常编译运行

---

## 📚 参考资料

- **详细实施计划**: `network_optimization_round2_spec.md`
- **上下文快照**: `network_optimization_context_snapshot.txt`
- **代码基线**: `d:\git\xrt\`
- **测试入口**: `test.c` 中的测试函数

---

> 💡 **提示**: 建议打印或导出本指南，实施过程中随时查阅。每完成一步就在 checklist 上打勾，有助于跟踪进度。
