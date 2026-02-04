# XRT Single Header Maker

用于将 XRT 库的所有源文件合并为一个单头文件的工具。

## 功能特性

- 自动合并 xrt.h 和所有 lib/*.h 文件
- 自动处理 include 依赖关系
- 移除重复的 include 指令
- 移除头文件保护（XXRTL_CORE）
- 添加生成时间戳和版本信息
- 保持原有代码格式和缩进
- 支持 XRT_IMPLEMENTATION 宏

## 文件结构

```
tools/
└── single_head_maker/
    ├── single_head_maker.c    # 工具源代码
    └── README.md              # 说明文档

build_single_head.bat         # 构建脚本（项目根目录）

singlehead/                   # 输出目录
└── xrt.h                   # 生成的单头文件
```

## 使用方法

### 方法 1：使用批处理脚本（推荐）

在项目根目录运行：

```bash
build_single_head.bat
```

### 方法 2：手动编译和运行

```bash
cd tools/single_head_maker
tcc single_head_maker.c -o single_head_maker.exe
single_head_maker.exe -o singlehead/xrt.h -s ..
```

### 方法 3：命令行选项

```bash
single_head_maker [options]

Options:
  -o <output>   输出文件路径（默认：singlehead/xrt.h）
  -s <source>   源代码目录（默认：..）
  -h            显示帮助信息

示例：
  single_head_maker -o singlehead/xrt.h
  single_head_maker -o output/xrt.h -s /path/to/xrt
```

## 使用单头文件

### 在一个源文件中定义实现

```c
// main.c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main() {
    xrtInit();
    xrtUnit();
    return 0;
}
```

### 在其他头文件中包含

```c
// other.c
#include "xrt.h"

void my_function() {
    str s = xrtStrCreate("Hello, XRT!");
}
```

## 文件合并顺序

工具严格按照以下顺序合并文件（与 xrt.c 中的 include 顺序一致）：

1. xrt.h - 主头文件
2. lib/suplib.h - 补充依赖库
3. lib/base.h - 基础模块
4. lib/charset.h - 字符集转换
5. lib/os.h - 操作系统接口
6. lib/math.h - 数学运算
7. lib/string.h - 字符串处理
8. lib/path.h - 路径处理
9. lib/time.h - 时间处理
10. lib/file.h - 文件操作
11. lib/thread.h - 线程管理
12. lib/hash.h - 哈希算法
13. lib/network.h - 网络接口
14. lib/xid.h - 分布式 ID
15. lib/buffer.h - 动态缓冲区
16. lib/array_point.h - 指针数组
17. lib/array.h - 结构体数组
18. lib/bsmm.h - 块结构内存管理
19. lib/memunit.h - 内存单元管理
20. lib/mempool_fs.h - 固定大小内存池
21. lib/stack.h - 静态栈
22. lib/stack_dyn.h - 动态栈
23. lib/avltree_base.h - AVL 树基础
24. lib/avltree.h - AVL 树
25. lib/mempool.h - 通用内存池
26. lib/dict.h - 字典
27. lib/list.h - 列表
28. lib/value.h - 动态类型系统
29. lib/jnum.h - JSON 数字
30. lib/json.h - JSON 处理
31. lib/template.h - 模板引擎

## 处理规则

工具会自动处理以下情况：

1. **移除 lib/ 的 include**：将 `#include "lib/xxx.h"` 转换为注释
2. **移除 xrt.h 的 include**：将 `#include "xrt.h"` 转换为注释（除了第一个文件）
3. **移除头文件保护**：移除 `#ifndef XXRTL_CORE`、`#define XXRTL_CORE` 和 `#endif`
4. **保留条件编译**：保留所有 `#ifdef`、`#if defined` 等条件编译指令

## 注意事项

1. **编译器要求**：支持 C99 或更高版本的编译器
2. **TCC 兼容性**：工具使用 TCC 编译，但也支持 GCC/Clang
3. **路径问题**：确保从正确的目录运行工具
4. **文件编码**：输入文件必须是 UTF-8 编码（不带 BOM）

## 优势

- ✅ **易于集成**：只需包含一个头文件
- ✅ **无需链接**：不需要编译和链接 xrt 库
- ✅ **快速部署**：适合快速原型开发
- ✅ **跨平台**：生成的单头文件支持 Windows 和 Linux
- ✅ **完整功能**：包含 XRT 的所有功能模块

## 与标准 XRT 的区别

| 特性 | 标准 XRT | 单头文件 XRT |
|------|---------|-------------|
| 文件数量 | 多文件（xrt.h + xrt.c + lib/*.h） | 单文件（xrt.h） |
| 编译方式 | 需要编译和链接 | 只需包含 |
| 编译时间 | 较短（模块化编译） | 较长（每次都编译全部） |
| 代码大小 | 较小（分模块） | 较大（全部包含） |
| 适用场景 | 大型项目、团队开发 | 快速原型、小型项目 |

## 故障排除

### 问题：编译失败

**原因**：源文件路径不正确

**解决**：
- 确保从项目根目录运行工具
- 检查 `-s` 参数指定的源目录路径

### 问题：生成的文件无法编译

**原因**：可能存在依赖顺序问题

**解决**：
- 检查文件合并顺序是否正确
- 确保所有必要的文件都已包含

### 问题：找不到头文件

**原因**：生成的文件路径不正确

**解决**：
- 检查 `-o` 参数指定的输出路径
- 确保编译器的 include 路径包含输出目录

## 版本历史

- v1.0.0 (2026-02-05)
  - 初始版本
  - 支持基本的文件合并功能
  - 自动处理 include 依赖
  - 移除头文件保护

## 许可证

MIT License - 与 XRT 主项目保持一致

## 联系方式

如有问题或建议，请联系：
- 作者：xLeaves [xywhsoft] <xywhsoft@qq.com>
- 项目地址：https://github.com/yourusername/xrt
