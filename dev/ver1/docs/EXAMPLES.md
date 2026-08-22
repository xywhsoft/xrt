# XRT 示例说明

> 面向想快速验证构建方式和最小用法的读者。

[English Version](EXAMPLES.en.md) | [返回索引](README.md)

---

## 推荐构建方式

### 1. 源码树方式

优先直接使用源码树：

```c
#include "xrt.h"
```

Linux / gcc：

```bash
gcc main.c xrt.c -o app
```

Windows / tcc：

```bash
tcc main.c xrt.c -o app.exe
```

### 2. 单头文件分发

单头文件仍可用，但更适合分发场景。开始新项目或排查问题时，优先使用源码树版本。


## 建议阅读顺序

1. 先跑下面的最小运行时示例。
2. 再看结构化数据示例，确认 `xvalue + json` 的基本写法。
3. 然后进入 [教学入口](guide/README.md) 补足模块选择和组合思路。
4. 需要完整程序时，继续看 [范例入口](case/README.md)。


## 最小运行时示例

```c
#include "xrt.h"
#include <stdio.h>

int main( void )
{
	str sGreeting;
	xvalue pNum;

	xrtInit();

	sGreeting = xrtCopyStr( "Hello, XRT!", 0 );
	pNum = xvoCreateInt( 42 );

	printf( "Greeting: %s\n", (char*)sGreeting );
	printf( "Number: %lld\n", (long long)xvoGetInt( pNum ) );

	xvoUnref( pNum );
	xrtFree( sGreeting );

	xrtUnit();
	return 0;
}
```


## 结构化数据示例

```c
#include "xrt.h"
#include <stdio.h>

int main( void )
{
	xvalue pUser;
	xvalue pSkills;

	xrtInit();

	pUser = xvoCreateTable();
	pSkills = xvoCreateArray();

	xvoArrayAppendText( pSkills, "C", 0, FALSE );
	xvoArrayAppendText( pSkills, "Python", 0, FALSE );

	xvoTableSetText( pUser, "name", 0, "Alice", 0, FALSE );
	xvoTableSetInt( pUser, "age", 0, 25 );
	xvoTableSetValue( pUser, "skills", 0, pSkills, TRUE );

	printf( "name = %s\n", (char*)xvoTableGetText( pUser, "name", 0 ) );
	printf( "age = %lld\n", (long long)xvoTableGetInt( pUser, "age", 0 ) );
	printf( "skill[0] = %s\n", (char*)xvoArrayGetText( pSkills, 0 ) );

	xvoUnref( pUser );
	xrtUnit();
	return 0;
}
```


## 下一步

- [教学入口](guide/README.md)
- [API 索引](api/README.md)
- [范例入口](case/README.md)
