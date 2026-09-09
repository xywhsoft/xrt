# Consuming and Verifying a Release Bundle

This case shows how downstream projects can verify and consume an xllm release bundle, avoiding the problem "it compiles in the repository, but not from the release package".

[Back to Case Studies](README.en.md) | [Release Gate Introduction](../guide/release-gate-intro.en.md) | [Release API / Tool Reference](../api/api-release.en.md)

## Problem

After receiving an xllm release package, you need to confirm three things:

- The package is not damaged or accidentally modified.
- Headers, source files, and dependencies are complete.
- A downstream project can compile a minimal program in a clean directory.

This is different from running tests inside the xllm repository. Release package consumption verification should simulate a real user and depend only on bundle contents and public documentation.

## Roles and Check Depth

| Role | Recommended Checks |
| --- | --- |
| Ordinary user | Checksum + minimal compile |
| Team integrator | Artifact verify + downstream smoke |
| Release maintainer | Release gate + downstream smoke |

If you are only learning xllm, start with minimal compile. If you will provide the package to other projects, run full artifact verification.

## Step 1: Generate or Obtain the Release Package

If you generate the package inside the xllm repository:

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

If you download a release package, put the zip and checksum files in the same directory and record the version.

## Step 2: Verify Version Consistency

Release maintainers should run:

```bat
cmd /c .\build.bat verify-version
```

It confirms that these locations point to the same version:

- `VERSION`
- Version macros in `xllm.h`
- Return value of `xllm_version()`
- `RELEASE_NOTES.md`

Ordinary users can print this in a minimal program:

```c
printf("xllm version: %s\n", xllm_version());
```

## Step 3: Verify the Artifact

After generating the release directory, run:

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

This check focuses on:

- Whether the release directory structure is complete.
- Whether `release_metadata.json` exists and has the correct version.
- Whether `SHA256SUMS.txt` matches.
- Whether `BUNDLE_SHA256SUMS.txt` inside the bundle matches.
- Whether compile verification can be completed using release artifacts.

If checksums do not match, do not continue consuming the package. Regenerate or re-download it first.

## Step 4: Extract into a Clean Directory

Downstream verification should use a clean directory to avoid accidentally using repository files. For example:

```powershell
New-Item -ItemType Directory -Force build\downstream-check | Out-Null
Expand-Archive build\release_bundle\xllm-windows.zip build\downstream-check -Force
```

`xllm-windows.zip` in this document is the conventional release package layout name. If your local release script outputs a platform package with a version, such as `xllm-0.1.0-windows.zip`, use the filename recorded in `release_metadata.json` and `SHA256SUMS.txt`. The important point is that include and source paths used for compilation must come from the extracted directory.

## Step 5: Compile a Minimal Program

The minimal program does three things: include xllm, print version, and create a runtime.

```c
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "xllm.h"

#include <stdio.h>

int main(void)
{
    xllm_runtime *pRuntime = NULL;

    xrtInit();
    printf("xllm version: %s\n", xllm_version());

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        return 1;
    }

    xllm_runtime_destroy(pRuntime);
    return 0;
}
```

Adjust the compile command to the release package layout. Windows/gcc usually looks like:

```bat
gcc -std=c11 -Wall -Wextra -Iinclude -Ilib ^
    app.c ^
    -o app.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
```

If your release package requires compiling SQLite source too, refer to the `lib\sqlite\sqlite3.c` usage in repository examples.

## Step 6: Verify Memory Works

If your downstream project uses memory, compile one more ingest/search-style program:

```c
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "xllm-memory.h"

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tOptions;

    xrtInit();
    xllm_runtime_create(NULL, &pRuntime);

    xllm_memory_options_init(&tOptions);
    tOptions.sNamespace = "downstream-check";
    tOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

    if ( xllm_memory_create(pRuntime, &tOptions, &pMemory) != XRT_NET_OK ) {
        return 1;
    }

    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
```

This can reveal whether the release package is missing `xllm-memory.h`, memory implementation files, or SQLite dependencies.

## Step 7: Run Downstream Smoke

The repository provides downstream smoke:

```bat
cmd /c .\build.bat downstream-smoke
```

Its purpose is to simulate a real user. If it fails, check first:

- Whether the bundle is missing files.
- Whether include paths match the documentation.
- Whether implementation macros are correct.
- Whether SQLite or xrt dependencies are missing.
- Whether checksums or metadata are inconsistent.

## Step 8: Run Release Gate

Release maintainers should run this before public release:

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

This final gate should run after release notes, versions, bundle, checksums, and downstream smoke are all ready.

## Suggested Consumer Directory Layout

A downstream project can organize release package contents like this:

```text
third_party/
  xllm/
    include/
    src/
    lib/
    release_metadata.json
```

Then add these to its build system:

- xllm include path.
- xrt include path.
- SQLite include/source, if using memory.
- Windows networking and system libraries.

## Key Checklist

1. Verify the hash of the zip or release directory.
2. Confirm that `release_metadata.json` has the correct version.
3. Compile a minimal program that only includes `xllm.h`.
4. If using session, compile `xllm-session.h`.
5. If using memory, compile `xllm-memory.h` and include SQLite dependencies.
6. If using a provider, set the API key and run a minimal chat.
7. When something fails, record `xllm_version()`, compile command, include paths, and error output.

## Common Questions

Do not secretly include files from the xllm repository path. Downstream verification must use only release package contents.

Do not ignore checksum failures. Continuing after a hash mismatch makes later problems hard to diagnose.

Do not test only core if your product uses memory or session. Compile those header paths separately.

Do not confuse signatures and checksums. Checksums verify integrity; signatures verify publisher identity. Public distribution or automatic updates also need a signing strategy.

## Next Steps

- To understand release commands, read [Release Gate Introduction](../guide/release-gate-intro.en.md).
- To see API-layer details, read [Release API / Tool Reference](../api/api-release.en.md).
- To verify minimal chat, read [Minimal Chat Call](minimal-chat.en.md).
