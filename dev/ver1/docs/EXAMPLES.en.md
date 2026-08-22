# XRT Examples Guide

> Quick verification notes for users who want the recommended build style and the smallest runnable samples.

[中文](EXAMPLES.md) | [Back to Index](README.en.md)

---

## Recommended Build Style

### 1. Source-tree build

Prefer the source tree first:

```c
#include "xrt.h"
```

Linux / gcc:

```bash
gcc main.c xrt.c -o app
```

Windows / tcc:

```bash
tcc main.c xrt.c -o app.exe
```

### 2. Single-header distribution

Single-header distribution is still supported, but it is better treated as a packaging form than as the default development view.


## Suggested Reading Order

1. Run the minimal runtime sample below.
2. Continue with the structured-data sample to confirm `xvalue + json` usage.
3. Move to [Guide Entry](guide/README.en.md) for the learning path and module choices.
4. Move to [Case Entry](case/README.en.md) when you want a full application instead of a tiny snippet.


## Minimal Runtime Example

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


## Structured Data Example

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


## Next Steps

- [Guide Entry](guide/README.en.md)
- [API Index](api/README.en.md)
- [Case Entry](case/README.en.md)
