---
num: 46
slug: dir
title: Files (Part 3): Directories, Traversal, and the Sandbox Root
volume: 卷五 系统服务
type: practice
lead: Directory enumeration with zero extra stat, recursive traversal, and the sandbox root — a system-level defense line against untrusted file names.
api: file, path
---

## Orientation

The final file chapter covers three directory capabilities — rising from single-file operations to "the container and boundary of files". **Directory enumeration** (`xrtDirOpen/Next`): three-state per-item enumeration, and **one system call carries out type and size** — XRT's directory API turns the traditional readdir-plus-stat pattern's 2N calls into N here, an order-of-magnitude win on large directories. **Recursive traversal**: depth-first walking of the directory tree (unfolded in the dir_tour sample). **The sandbox root** (`xrtRootOpen`): a directory handle as security boundary — operations inside the root take relative names only, and traversal and absolute paths are rejected at the system level; it is the second defense line beyond Chapter 43's lexical validation (IsSafeEntry) (the system-level link of defense in depth) (when lexical checks can be bypassed, system-level semantics catch the rest).

## Introduction

Extracting a user-uploaded archive: entry names inside the package are untrusted input — a traversal entry like `../../evil.so`, written straight to disk, lands outside the sandbox. Chapter 43's lexical validation (IsSafeEntry) intercepts the vast majority, but the lexical layer has a fundamental weakness: **it judges strings** — symbolic links, platform special cases, and normalization differences can all detach the lexical verdict from the file system's actual behavior.

The sandbox root solves it at the system level: `xrtRootOpen` opens a directory and yields a **handle**; from then on every operation (create file, open file, create directory) is based on the handle and takes relative names only — traversal has nowhere to hide **at the operating-system level** (not "check then allow", but "there is no path out at all"). The root handle has a side benefit: it remains valid after the directory is renamed or moved (the handle doesn't depend on path strings) — log rotation and directory reorganization don't interrupt in-flight operations.

## Concepts

### Directory enumeration: the three-state interface and the zero-extra-stat dividend

```diagram flow
- Open: xrtDirOpen(path, flags) -> directory stream handle
- Enumerate: xrtDirNext(handle, entry out-param) -> three states: ITEM got one / END done / ERROR failed
- Entry: xdirentry carries a name view + type/size (one call brings all)
- Close: xrtDirClose
```

What "zero extra stat" means for performance: after traditional readdir returns a name, learning "file or directory" takes another stat — N entries, 2N system calls; the XRT directory API carries type and size directly (the native directory information on Windows/Linux already includes them) — N calls, an order of magnitude faster on large directories. The entry's name is a view under **borrowing**, valid until the next Next (Chapter 18's iteration discipline).

### Recursive traversal (depth-first walking of the tree)

Depth-first walking of the directory tree: enumerate the current level → recurse into subdirectories encountered → backtrack when done. Recursion depth is guarded by Chapter 3's resource limits (against maliciously deep trees); concurrent additions and deletions during traversal are a real risk — contents modified externally mid-enumeration — the traversal semantics treat it as "best-effort snapshot"; when strict consistency is needed, collect first, then process (the directory edition of Chapter 18's Pitfall 2).

### The sandbox root: the handle is the boundary (system-level, not lexical)

`xrtRootOpen` / `xrtRootOpenIn` (nested sub-roots) open a directory as a root; `xrtRootFileOpen` (open a file inside), `xrtRootDirCreate/Remove` (create/remove directories inside) all take **relative names only** — `../` and absolute paths are rejected. The cooperation with Chapter 43's lexical validation: the lexical layer is the first filter (fast rejection of obviously malicious shapes, zero cost), the system-level root is the final guarantee (semantically correct, covering lexical blind spots) — **two defense lines are not redundancy but defense in depth** — the standard engineering-security posture: the cheap one absorbs volume, the sturdy one catches the rest.

### Temp directories in concert (the standard container for batch staging)

`xrtDirTemp` (mentioned in Chapter 45) creates a uniquely named temporary directory inside a given directory — for the scene "a batch of temp files needs an independent home" (extraction staging, build intermediates): create the temp directory → put things in freely → delete the whole directory to finish. An order of magnitude cleaner than scattered temp files — the "home" semantics turn cleanup into one call.

## Examples

### Complete program: enumeration with zero extra stat

From the repository sample `examples/file/directory/main.c`:

```embed path="examples/file/directory/main.c" title="examples/file/directory/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/file/directory/main.c -lws2_32 -liphlpapi
.git
.gitattributes
...
```

**What just happened.** (1) `xrtDirOpen(".", 0)` opens the current directory — returning a directory stream handle (not a file handle; the two kinds never mix). (2) `xrtDirNext`'s three-state loop: ITEM prints the entry name, END ends naturally, ERROR aborts with a report — isomorphic with Chapter 40's line-reader three states (LINE/END/failure); the unified shape of streaming interfaces continues onto directories. (3) Each entry carries name, type, and size in one call — contents naturally vary with the current directory (the listing differs per repository) — this sample only prints names, but the type field in `Entry` (file/directory) is at hand, no extra stat needed. (4) The name is borrowing-based view (Chapter 3's convention) — printed with `%.*s` and discarded; to keep it, copy it yourself (Pitfall 1's good form is exactly "copy first, then collect").

### Complete program: sandbox-root operations

From `examples/file/root/main.c` — safe storage of untrusted relative names:

```embed path="examples/file/root/main.c" title="examples/file/root/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/file/root/main.c -lws2_32 -liphlpapi
stored and removed .xrt-root-example-data/message.txt
```

**What just happened.** (1) `xrtRootOpenIn` opens a root under a parent directory — the nested form allows "one root per upload". (2) `xrtRootDirCreate` creates a directory inside the root, `xrtRootFileOpen` opens a file inside — all relative names: passing `../escape` or an absolute path is rejected at the system level (not a lexical check but the natural consequence of handle semantics — the operation happens inside the root; "outside the root" is not an option). (3) After storage and cleanup, `xrtRootClose` closes the root — resources inside are managed by the root's lifecycle as one. (4) The printed path carries the root prefix (audit-readable), but no operation ever depended on that string — **the handle doesn't depend on the path**; renames and moves don't interrupt. The dir_tour and root_tour samples are left as further reading: recursive traversal and root operations each form a group of assertion outputs covering the complete interface surface.

## Contracts

- **Three-state enumeration**: ITEM/END/ERROR, isomorphic with the library's streaming interfaces; name views borrowed no longer than the iteration.
- **Zero extra stat**: type and size come out with each entry — N calls for a large directory, not 2N.
- **Traversal boundaries**: depth guarded by resource limits; mid-traversal additions/deletions follow "best-effort snapshot"; for strict consistency, collect first, then process.
- **Sandbox semantics**: inside the root, relative names only; traversal and absolute paths rejected at the system level; the root handle doesn't depend on path strings (still valid after renames and moves).
- **Defense in depth**: lexical validation (Chapter 43) as fast filter + system-level root semantics as backstop — the two layers cooperate, not duplicate.
- **Temp directories**: `xrtDirTemp` uniquely named, whole-directory management — the standard container for batch staging.

### From examples to engineering: three hosts of directory capabilities

**Uploads and extraction** (the sandbox root's home turf): one root per upload (nested RootOpenIn) — traversal entries rejected at the system level, legitimate entries restored inside the root, whole root deleted after processing; Chapter 43's lexical validation does the upfront fast filtering, the root handle provides the final guarantee. **Resource directory access** (enumeration + root combined): plugin assets, static-file directories — once the root handle is fixed, operations can rename and reassemble the directory (the handle doesn't depend on the path) without service interruption. **Batch build staging** (the temp directory's home turf): intermediates of build/conversion tasks managed as whole directories — DirTemp makes the home, the task deletes it when done, an order of magnitude cleaner than scattered temp files (the batch edition of Chapter 45's atomic delivery).

### The engineering discipline of traversal

Recursive traversal looks simple; engineering it adds four disciplines. **Depth cap**: a maliciously deep tree (or a symlink loop) can blow up naive recursion — the resource limits gate at the enumeration entrance. **Symlink policy**: follow (default, may loop) or skip (identify by the type field) — the policy is declared explicitly and written into comments. **Error recovery**: a single directory's enumeration failure (permissions) — abort or skip? Decide by the traversal's purpose (audits abort, cleanups skip). **Order assumptions**: enumeration order is the file system's (unordered) — lists needing order are sorted after collection (Chapter 14). The four together: **a traverser is framework code, not loop boilerplate** — answer the four questions before writing it.

### Convergence with the first two file chapters

With this, the three file chapters converge into the complete map: Chapter 44's handles and reads/writes (single-file lifecycle), Chapter 45's mapping/locks/atomic delivery (concurrency and performance), this chapter's directories and sandbox (organization and security). Their typical combinations run through the rest of the book: Volume 7's static-file service (root handle + enumeration + mapping), Volume 12's build tools (directory traversal + temp directories + atomic delivery), the extractor (sandbox root + three-state enumeration + Full reads/writes). File capability is the foundation of system services — with the foundation laid, the next chapter's asynchronous IO and signals add the last two bricks.

## Pitfalls

### Pitfall 1: deleting directory contents during traversal (the best-effort snapshot trap)

Symptom: traversal occasionally misses entries or hits ERROR — a concurrent cleanup script is deleting files inside the traversal target; reproduction depends on timing.

Cause: directory enumeration is a "best-effort snapshot" — the underlying directory stream's semantics don't promise to reflect every change made during the walk.

```c bad
while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
	if ( IsOld(Entry.Name) ) {
		DeleteByName(DirPath, Entry.Name);   /* deleting while traversing - the underlying stream is disturbed */
	}
}
```

```c good
xarray tOld;
xrtArrayInit(&tOld, sizeof(str));
while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
	if ( IsOld(Entry.Name) ) {
		xrtArrayPush(&tOld, &(str){ xrtStrDupView(Entry.Name) });   /* collect the names first */
	}
}
xrtDirClose(Dir);
for ( size_t i = 0; i < tOld.Count; i++ ) {
	DeleteByName(DirPath, *(str*)xrtArrayGet(&tOld, i));   /* delete after the traversal ends */
}
```

### Pitfall 2: treating lexical validation as the only defense line (missing depth)

Symptom: a penetration test bypasses IsSafeEntry — symlinks, case confusion, or platform path-normalization differences make a "lexically safe" path actually point outside the root — string judgment and file-system resolution detach at the boundary.

Cause: the lexical layer judges strings; the file system resolves real paths — the two come apart in symlink scenarios and the like.

```c bad
if ( xrtPathIsSafeEntry(Name, false) ) {
	str sFull = xrtPathJoin(BaseDir, NameBuf);
	xrtFileOpen(sFull, ...);   /* lexically passed, so open - a symlink may point outside the root */
}
```

```c good
xroot Root = xrtRootOpen(BaseDir, 0);
xfile File = xrtRootFileOpen(Root, NameBuf, ...);   /* system-level boundary: traversal never happens */
/* lexical validation can still run upfront as a fast filter - but the root handle is the final guarantee */
```

## Exercises

### Basic: enumeration statistics (using the zero-extra-stat data)

Enumerate a directory; count files and directories by type and print the total size — put the one-call data fully to work.

### Advanced: a recursive lister (the four traversal disciplines' debut)

Implement `list_tree(路径, 深度上限)` (path, depth cap): print the tree with indentation (depth-first), count files and subdirectories per level, mark over-limit directories as skipped — recursive traversal combined with resource limits.

### Challenge: a safe extractor (defense in depth in full)

Implement archive extraction with a sandbox root: create the directory structure inside (RootDirCreate level by level), write files (RootFileOpen+Write), reject every malicious entry (traversal/absolute/device names) and list them. Acceptance criteria: with a package containing five malicious entries (one each of traversal/absolute path/device name/symlink loop/over-deep nesting), all rejected with zero files outside the root; legitimate entries fully restored; after deleting the whole root, zero residue — one line of conclusion per attack class.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three-state enumeration | `DirOpen/Next/Close`: ITEM = got one / END = done / ERROR = failed, isomorphic with the library's streaming interfaces |
| Zero extra stat | type and size ride along with each entry - N calls, not 2N; an order of magnitude faster on large directories |
| Recursive traversal | depth-first walk + resource-limit depth gate; mid-walk additions/deletions follow best-effort snapshot semantics |
| Sandbox root | `RootOpen/RootOpenIn/RootFileOpen/RootDirCreate`: relative names only, traversal rejected at the system level |
| Defense in depth | lexical validation (Chapter 43) fast-filters + root-handle semantics backstop - the cheap absorbs volume, the sturdy catches |
| Temp directories | `xrtDirTemp` uniquely named - whole-directory batch staging |
| Three hosts | uploads/extraction (root's home) / resource directories (enumeration + root) / batch staging (temp directory) |
| Four traversal disciplines | depth cap / symlink policy / error recovery / order assumptions - framework, not boilerplate |
