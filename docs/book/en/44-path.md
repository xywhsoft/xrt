---
num: 44
slug: path
title: Path Handling
volume: 卷五 系统服务
type: practice
lead: Purely lexical operations that never touch the disk — joining with automatic cleanup, four-way splitting, safety checks, and segment-by-segment iteration.
api: path
---

## Orientation

The path module handles paths' **lexical layer** — joining, splitting, cleanup, validation — none of it touching the file system (pure string operations, zero IO, zero side effects, disk-free testing). Three core operations: `xrtPathJoin`, multi-segment joining with **automatic lexical cleanup** (`../` folding, separator normalization), four-way splitting (Name/Stem/Ext/directory in one pass), and `xrtPathIsSafeEntry`, the safety check (rejecting traversal and device names — the security gate for external input entering paths). Plus zero-allocation segment iteration and system-path queries (home/cwd/real).

## Introduction

A user-uploaded file goes to `data/{用户名}/icon.png` (username) — the username is external input. Concatenate directly: a username of `../../etc` writes the file outside the sandbox; a username of `CON.txt` (a Windows device name) behaves bizarrely. This kind of "path traversal" is a regular on security-vulnerability charts, and the root cause is always **external input entering a path without lexical validation**.

The second daily pain is cleaning up hand-concatenated paths: `"project" + "src/../include/xrt.h"` — the joined path carries the redundant `src/..` segment; most file systems tolerate it, but log comparisons, cache keys, and test assertions are all polluted by "one path, many spellings". `xrtPathJoin` builds cleanup into joining: the output is always the folded canonical form — **one path, one spelling** is the lexical layer's core promise.

## Concepts

### Joining and automatic cleanup

`xrtPathJoin("project", "src/../include/xrt.h")` (two cstr segments in, an owning str out) → `project\include\xrt.h` — `src/..` folded, separators normalized per platform. Cleanup is **lexical-level** (no symlink resolution, no disk existence check): `..` is preserved when it cannot fold (`../secret` is not wrongly folded to empty — that is precisely the safety semantics). Cross-platform forms: `XPATH_NATIVE` follows the current platform (Windows backslashes/Unix forward slashes); explicitly specifying a form yields cross-platform-consistent products (test snapshots, manifest files).

### Four-way splitting and rebuilding

| Split | Result (for `project\include\xrt.h`) |
| --- | --- |
| `xrtPathName` | `xrt.h` (the last segment) |
| `xrtPathStem` | `xrt` (extension removed) |
| `xrtPathExt` | `.h` (dot included) |
| Directory part | rebuilt by joining or obtained by iteration |

`xrtPathWithName` replaces the file name and keeps the directory (`renamed=project\include\runtime.h`) — the standard posture for changing extensions or adding suffixes. Splitting and rebuilding are mutually inverse: `目录 + Name == 原路径` (directory + Name == original path) — tests assert exactly this inverse law.

### The safety check: a security gate for external input

`xrtPathIsSafeEntry` is a one-vote-veto check rejecting three dangerous shapes: **traversal** (`../` sequences — escaping the base directory), **absolute paths** (external input should never decide the root), and **platform device names** (`CON`/`NUL` and other Windows reserved names). The standard pipeline before external input enters a path: `IsSafeEntry 校验 → Join 拼接（自动清理）→ 使用` (IsSafeEntry validation → Join (automatic cleanup) → use) — the safe sample's output (`rejected`) is this gate's enforcement record. The second parameter controls whether subdirectory entries are allowed (archive extraction opens it as needed).

### Anatomy of one path

```diagram flow
- Input: project/src/../include/xrt.h (users' varied spellings)
- Join cleanup: fold src/.., normalize separators - produce the canonical form
- Four-way split: Name=xrt.h / Stem=xrt / Ext=.h / directory=project/include
- Inverse rebuild: directory + Name == the original; WithName swaps the name, keeps the directory
```

### Segment iteration and system paths

`xrtPathIterInit/Next` walks segment by segment with zero allocation (borrowing — each segment is a view slicing the original path, Chapter 25's pipeline thinking); segment counting and level-by-level directory creation (Chapter 47) both rely on it. System-path queries: home (user directory), cwd (current directory), real (the true path after resolving symbolic links — this one **does** touch the file system, the module's sole exception).

## Examples

### Complete program: join-with-cleanup and four-way splitting

From the repository sample `examples/path/basic/main.c`:

```embed path="examples/path/basic/main.c" title="examples/path/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/path/basic/main.c -lws2_32 -liphlpapi
path=project\include\xrt.h
name=xrt.h
stem=xrt
ext=.h
renamed=project\include\runtime.h
components=3
local=1
```

**What just happened.** (1) The output of `Join("project", "src/../include/xrt.h")` no longer contains `src/..` — joining is cleanup, the product canonical; the separator is the Windows backslash (the same program on Linux prints forward slashes — platform form, automatic). (2) Four-way splitting in one pass: Name is `xrt.h`, Stem drops `.h`, Ext includes the dot — three views obtained independently. (3) `WithName` replaces the file name, directory untouched — the standard rename posture. (4) `components=3` comes from zero-allocation iteration — three segments (project/include/xrt.h) counted one by one, no heap allocation throughout. (5) `local=1` is the relative-path verdict — the absolute/relative divide serves path routing (config relative to a base directory).

### Complete program: the safety check's triple rejection

From `examples/path/safe/main.c`:

```embed path="examples/path/safe/main.c" title="examples/path/safe/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/path/safe/main.c -lws2_32 -liphlpapi
assets/icon.png: safe
../secret.txt: rejected
CON.txt: rejected
```

**What just happened.** (1) The ordinary relative path `assets/icon.png` passes — the check doesn't block legitimate input. (2) `../secret.txt` is rejected — traversal sequences don't escape the gate; the third input (an absolute-path shape) is likewise rejected — external input shouldn't carry a root. (3) `CON.txt` is rejected — cross-platform interception of Windows device names (Linux rejects it too; consistency first). The tour sample is left as full-interface reading: parse structure (root/flags/stem/ext), build and clean, relativization, and relative-vs-absolute verdicts — one ok per group, four groups covering the path module's complete interface surface.

## Contracts

- **Purely lexical**: everything except `real` (symlink resolution) stays off the file system — testability maxed.
- **Joining is cleanup**: Join's (two cstr segments) product is the folded, normalized canonical form; multi-segment paths use nested Joins or one Join after assembly; one path, one spelling.
- **Splitting inverse**: directory + Name == original path; Stem + Ext == Name (with the dot).
- **Security discipline**: external input passes `IsSafe` before entering a path; traversal/absolute/device names — the triple rejection.
- **Platform forms**: NATIVE per platform; explicit forms for cross-platform-consistent products.
- **Zero-allocation iteration**: segment views borrow the original path, validity tied to it.

### From examples to engineering: three hosts of paths

**Config and resource resolution**: at startup, normalize the three base paths "config directory/data directory/temp directory" (Join + real); thereafter every relative path Joins onto a base — path routing centralized, avoiding bare string paths scattered everywhere. **User-input mapping**: upload file names, static-resource URL paths — the IsSafeEntry gatekeeper + Join + extension whitelisting (Chapter 36's pipeline mirrored at the file layer). **Cross-platform products**: paths in manifest files, cache keys, test snapshots — explicit form (not NATIVE) guarantees identical products on Linux and Windows, so Chapter 12's hash fingerprints are reproducible. The three hosts share one bottom line: **normalize a path before using it** — the bare string path is the root of all path problems.

### The relation to Chapters 45~48

path is the lexical prelude to the file family (Chapters 45~48): a file is opened by path, and the path first passes Join/IsSafeEntry's normalization and security check. The file chapters all point back here — "where the path came from" is told once at the source (this chapter), and the file chapters focus on "what happens after opening". The reverse relation exists too: this chapter's real query (the only disk-touching interface) depends on file-system metadata operations internally — the lexical layer and the IO layer meet in a handshake at real and stay apart everywhere else.

### Three cross-platform difference points

Three differences are worth memorizing for cross-platform path code. **Separators**: Join normalizes automatically; a hand-written `/` passes most Windows APIs — but canonical forms, cache keys, and log comparisons derail on mixed separators. **Root shapes**: Unix's single root `/` versus Windows drive letters `C:` plus UNC `\server` — the absolute-path verdict (via IsLocal) follows the shape table; `/foo` is absolute on Unix and "drive-less relative" on Windows. **Device and reserved names**: `CON`/`NUL`/`COM1` are Windows-specific traps — IsSafeEntry checks uniformly against the strictest platform (rejecting on Linux too) so products are distributable cross-platform. All three differences are smoothed over by this chapter's APIs — you may "forget the platform" while writing lexical code; that is precisely why the module exists.

## Pitfalls

### Pitfall 1: external input straight into a path

Symptom: a security audit reports a path traversal hole — `../../` input reads and writes outside the sandbox; or device-name input causes bizarre behavior.

Cause: no validation step before joining — "the user gives a file name" was assumed as an implicit contract.

```c bad
str sPath = xrtPathJoin("data/", UserNameBuf);   /* UserNameBuf is external input */
SaveTo(sPath);   /* UserName="../etc/passwd": the traversal segment escapes the data sandbox */
```

```c good
xstrview Name = (xstrview){ UserName, strlen(UserName) };
if ( !xrtPathIsSafeEntry(Name, false) ) {
	RejectUpload();   /* traversal/absolute/device names rejected at the gate */
	return false;
}
```

### Pitfall 2: hand-concatenating paths as strings

Symptom: `"dir/" + name` produces mixed separators on Windows (`dir/\file`); `a/b/../c` redundant segments pollute cache keys and log comparisons.

Cause: skipping Join's cleanup and normalization — doing path work with string Concat (Chapter 25).

```c bad
str sPath = xrtStrConcat(XRT_STR_LITERAL("data/"), NameView);
sPath = xrtStrConcat((xstrview){ sPath, strlen(sPath) }, XRT_STR_LITERAL("/icon.png"));
/* mixed separators + no cleanup + three allocations */
```

```c good
str sPath = xrtPathJoin("data/", NameBuf);   /* one call: normalize + cleanup + one allocation */
```

## Exercises

### Basic: verify the inverse law (a regression test for the lexical layer)

Take five paths of different shapes (relative/absolute/multi-segment/with extension/without) and "split then rebuild"; assert the rebuilt result equals the original's canonical form — the inverse law, measured.

### Advanced: a path router (one entrance for base directories)

Implement `resolve(基准目录, 输入路径)` (base directory, input path): if the input is relative, Join the base; if absolute, reject or allow per config; output canonical form. Write the config switch's behavioral difference into a comment.

### Challenge: an upload sandbox (the security gate in full)

A composite validator: file-name check (IsSafe triple rejection) + length cap + extension whitelist + result-path uniquification (on conflict, append a number to the Stem). Acceptance criteria: twenty crafted inputs (traversal/device/over-long/out-of-whitelist/conflict) each handled as expected; zero file-system calls throughout (purely lexical, fully testable).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Purely lexical | the whole module does zero IO except real (symlink resolution) - disk-free testing |
| Join | joining is cleanup: `../` folded, separators normalized, one canonical form |
| Four-way split | Name (last segment) / Stem (extension removed) / Ext (dot included) / directory rebuilt |
| Security | `IsSafeEntry(视图, 是否允许子目录)` (view, allow subdirectories): traversal/absolute/drive/device names all rejected - mandatory for external input |
| Iteration | `IterInit/Next` zero-allocation segment walk; segment views borrowed |
| System paths | home/cwd queries; real resolves symlinks (the only disk-touching interface) |
| Three hosts | base-path normalization / user-input mapping (gatekeeper + whitelist) / cross-platform products |
| Three cross-platform differences | separator normalization / root-shape verdicts / device names uniformly checked - all smoothed by the API |
