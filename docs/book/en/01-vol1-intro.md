---
num: 1
slug: vol1-intro
title: Volume 1 · Getting Started &amp; the Core: Introduction
volume: 卷一 起步与核心
type: intro
lead: This volume settles the foundation every XRT program stands on — environment, the first program, errors, memory, and trimming.
api: core
---

## Orientation

Volume 1 is the foundation of the whole book. Whether you end up writing network services, parsing configuration, or making secure connections with XRT, every line you write stands on this volume's content: how errors are reported (Chapter 4), how memory is allocated and returned (Chapter 5), how faults are injected to localize problems (Chapter 6), how temporary objects are managed at zero cost (Chapter 7), and how small on-demand trimming can go (Chapter 8). These are not "advanced features" — they are the library-wide language; every later chapter uses these rules directly and never explains them a second time.

The volume's knowledge map and reading order:

```diagram flow
- Meet XRT: the kernel-plus-extensions two-layer structure, dev environment, single-header integration
- First program: type aliases, the view contract, resource limits, and reference counting
- Error model: the xerror quadruple, cause chains, the thread error slot
- Memory basics: the global heap, alignment, allocator replacement
- Memory debugging and stats: fault injection, leak detection, process-level statistics
- Temporary memory: arena scoped allocation, zero management overhead per function
- Version and trimming: ABI conventions, module macro closure, on-demand delivery
- Atomics (finale): atomic primitives, the common base of spinning and waiting
```

Suggested paths for three kinds of readers: **readers new to C** should read straight through and compile-and-run each chapter's complete programs by hand — the programs in this volume are short, but every one is worth running; **readers with C engineering experience** can move quickly through Chapters 2 and 3 and invest the time in Chapter 4 (the error model is one of XRT's most distinctive designs) and Chapter 6 (fault injection underpins the testing methodology of the later networking and TLS chapters); **readers arriving from other languages** should pay special attention to the "view" concept in Chapter 3 and the ownership conventions in Chapter 5 — they are the C counterparts of string slices and smart pointers respectively, but with stricter rules.

### Naming and terminology conventions

Before reading XRT code, agree on a set of read-the-name-know-the-meaning rules, used throughout the book. Functions and types use a module prefix + UpperCamelCase: things starting with `xrt` are kernel functions (`xrtJsonParse` is the json module's parse function), short lowercase names starting with `x` are types (`xerror`, `xstrview`), and all-caps names starting with `X` are constants and macros (`XERR_TIMEOUT`, `XRT_NPOS`). Parameter names carry Hungarian-style prefixes: `p` for pointers, `i` for integers, `s` for zero-terminated strings, `b` for booleans, `arr` for arrays, `Text/View` for views — seeing `pConfig` and `iCount` tells you the type without jumping back to the declaration. Output parameters are almost always pointer parameters (C has no references), and functions do not write outputs on failure — "no side effects on failure" means error-recovery code never has to clean up half-built results.

Three recurring terms, stated up front: **owning** means "you are responsible for freeing it, exactly once"; **borrowing** means "you may use it but must not free it, and its lifetime follows the source"; **managed** means the object carries its own lifecycle mechanism (reference counting or an owning pool) — you increase and decrease holdings by rule instead of allocating and freeing directly. The views of Chapter 3 are the carrier of borrowing; the global heap of Chapter 5 is the exit point of owning; reference counting is the implementation of management — the three form the skeleton of XRT's memory narrative.

### Learning self-checks

After each chapter, self-check with three questions: what problem did this chapter solve (can you say it in one sentence to a colleague)? Which hard rules does the Contracts section list (can you recite them with the book closed)? Can you find shadows of the Pitfalls symptoms in your own past code? If all three check out, move on; if one sticks, go back and reread that section — this volume is the foundation, and the floors above will not make up for it. This checklist is also the book's learning method: every XRT module deserves to be digested with the "problem, contract, pitfall" trio; once this habit forms, reading any new module gets noticeably faster.

### How to use this book

Every chapter follows a fixed structure: **Orientation** says where the chapter stands and where it is going; **Introduction** starts from a real problem, not from an API list; **Concepts** builds mental models with state-machine or flow diagrams; **Examples** are complete compilable programs — each with a build command and real output, most taken directly from the repository's `examples/` directory (the corner of each code block carries a source link that jumps to Gitee for comparison); **Contracts** gathers the hard rules for errors, ownership, and threading; **Pitfalls** gives symptoms, causes, and wrong/right code pairs; **Exercises** come in basic, advanced, and challenge tiers. The suggested reading rhythm: read the concepts and examples through first, then run the programs yourself, and finally do at least the basic exercises — only after these three steps does a chapter's knowledge land.

When you meet an unfamiliar function, two entry points: each chapter's end-of-chapter "API references in this chapter" links straight to the module's full reference page (signatures, parameter constraints, return values, errors, examples); or open the website's API search page and type the function name. The reference pages and this book share the same set of facts, so they never disagree.

### Reading preparation

You need only two things: a C11 compiler (GCC/Clang/MSVC all work; this book's examples use Windows + GCC output as the baseline, with other platforms identical or differences noted), and a copy of the XRT source — `git clone https://gitee.com/xywhsoft/xrt.git` or download the archive directly. No third-party libraries need installing beforehand: the XRT runtime has zero third-party dependencies and the examples use only the C standard library. Every example's build command is in its chapter's output block — copy and run; keep the repository root as the working directory so the relative paths in the commands (like `-I single`, `examples/...`) resolve correctly.

### The road map of the book

After Volume 1: Volumes 2 through 5 are the "data and systems" half — math and randomness, containers, text and structured data, logging and files/processes; Volumes 6 through 9 are the "concurrency and networking" half — threads and coroutines, networking, security, and the Web protocol core; Volumes 10 and 11 cover the five extension libraries; Volume 12 returns to the engineering perspective (build, trim, test, performance); Volume 13 ties the whole book together with five complete projects. Each volume's opening introduction gives its knowledge map and prerequisites — if a prerequisite volume is unread, the introduction points back to the exact chapters.

By the end of Volume 1, you should be able to answer without looking anything up: what a fallible XRT function returns and where to read the error details; where a block of memory comes from and who frees it; what defense line backs an untrusted input; and which single macro line trims XRT down to only the modules you need. Set out with these questions.
