# XRT Single Header Maker

用于将 XRT 库的所有源文件合并为一个单头文件的工具。

## 功能特性

- ✅ 自动合并 xrt.h 和所有 lib/*.h 文件
- ✅ 动态展开 #include 依赖关系
- ✅ 支持 XRT_IMPLEMENTATION 宏（单头文件标准模式）
- ✅ 自动检测 xrt.c 路径（从当前目录向上搜索）
- ✅ 自动创建输出目录
- ✅ 添加生成时间戳和 MIT 许可证
- ✅ 保持原有代码格式和缩进

## 文件结构

```
tools/
└── single_head_maker/
    ├── single_head_maker.c    # 工具源代码
    ├── single_head_maker.exe  # 编译后的可执行文件
    ├── xrt.h                 # 单头文件版本的 XRT（用于编译工具）
    └── README.md              # 说明文档

build_single_head.bat         # 构建脚本（项目根目录）

singlehead/                   # 输出目录
└── xrt.h                   # 生成的单头文件
```

## 使用方法

### 方法 1：自动检测模式（推荐）

直接运行，无需任何参数：

```bash
cd tools/single_head_maker
./single_head_maker.exe
```

工具将：
1. 从当前可执行文件所在目录开始
2. 逐级向上搜索 xrt.c 文件
3. 找到后在 xrt.c 所在目录的 `singlehead/` 子目录生成单头文件

### 方法 2：指定参数模式

```bash
cd tools/single_head_maker
tcc single_head_maker.c -o single_head_maker.exe
single_head_maker.exe -o singlehead/xrt.h -c ../xrt.c -s ..
```

### 命令行选项

```bash
single_head_maker [options]

Options:
  -o <output>   输出文件路径（默认：自动检测）
  -c <xrt.c>    xrt.c 文件路径（默认：自动检测）
  -s <source>   源代码目录（默认：自动检测）
  -h            显示帮助信息

示例：
  # 自动检测（推荐）
  single_head_maker
  
  # 手动指定所有路径
  single_head_maker -o singlehead/xrt.h -c ../xrt.c -s ..
```

## 使用单头文件

### 在一个源文件中定义实现

```c
// main.c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main() {
    xrtInit();
    
    // 使用 XRT 函数
    str s = xrtCopyStr("Hello, XRT!", 0);
    printf("%s\n", s);
    
    xrtUnit();
    return 0;
}
```

### 在其他源文件中包含

```c
// other.c
#include "xrt.h"

void my_function() {
    str s = xrtCopyStr("Hello, XRT!", 0);
}
```

## 生成文件结构

生成的单头文件结构：

```c
// MIT License 说明
// 使用说明

#ifndef XRT_SINGLE_HEADER
#define XRT_SINGLE_HEADER

// ========================================
// 头文件部分（声明和类型定义）
// ========================================
// xrt.h 的内容（包含所有 lib/*.h 的声明）

#ifdef XRT_IMPLEMENTATION
// ========================================
// 实现部分（函数实现）
// ========================================
// xrt.c 的内容（不包含 xrt.h 的 include）
#endif

#endif // XRT_SINGLE_HEADER
```

## 原理说明

### 自动搜索机制

当不提供任何参数时，工具会：

1. 获取当前可执行文件所在路径（xCore->AppPath）
2. 从该路径开始，逐级向上搜索 `xrt.c` 文件
3. 找到 xrt.c 后，在其所在目录的 `singlehead/` 子目录生成单头文件

### XRT_IMPLEMENTATION 宏

- **定义 XRT_IMPLEMENTATION**：包含函数实现
- **不定义 XRT_IMPLEMENTATION**：仅包含声明和类型定义

这样可以在多个源文件中包含同一个头文件，而只在一个文件中定义实现，避免了链接错误。

## 处理规则

工具会自动处理以下情况：

1. **跳过 xrt.h 的 include**：在处理 xrt.c 时自动跳过 `#include "xrt.h"`
2. **展开 lib/ 的 include**：将 `#include "lib/xxx.h"` 替换为实际内容
3. **保留系统头文件**：`#include <stdio.h>` 等保持不变
4. **防重复引用**：已处理的文件会自动跳过，避免循环包含
5. **保持代码格式**：原有代码的缩进和格式不变

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

### 问题：找不到 xrt.c

**原因**：从当前目录向上搜索不到 xrt.c 文件

**解决**：
- 确保在 xrt 项目目录树内运行工具
- 或者使用 `-c` 参数手动指定 xrt.c 路径

### 问题：编译失败

**原因**：生成的单头文件有问题

**解决**：
- 确保只在一个源文件中定义 `XRT_IMPLEMENTATION`
- 其他文件只包含 `#include "xrt.h"` 而不定义 `XRT_IMPLEMENTATION`

### 问题：输出目录创建失败

**原因**：没有权限创建 singlehead 目录

**解决**：
- 确保有写入权限
- 或者使用 `-o` 参数手动指定输出路径

## 版本历史

- v2.0.0 (2026-02-05)
  - 新增：支持 XRT_IMPLEMENTATION 宏
  - 新增：自动检测 xrt.c 路径
  - 优化：文件结构分离声明和实现
  - 重构：动态展开 #include，不再依赖硬编码文件列表

- v1.0.0 (2026-02-04)
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
