# Pattern benchmark

`bench_pattern.c` separates cold compilation from immutable lookup and capture
matching. It exercises both whole-field captures and shared-prefix
`prefix{id}suffix` captures from 1 to 10,000 patterns with randomized hits, so
the reported curve exposes growth caused by state size and cache pressure rather
than a favorable repeated key.

Build and run through the repository performance tooling, or select the
`pattern` module and compile this benchmark as a single-header consumer. The
first command-line argument is the lookup iteration count. The optional second
argument raises the maximum corpus from the default 10,000 up to 100,000
patterns for capacity runs.

The benchmark intentionally reports compilation time, compiled bytes, lookup
nanoseconds, capture nanoseconds, and a checksum. Affix metrics use the
`pattern_affix_` prefix. Compare builds only on the same machine, compiler,
optimization flags, corpus, and iteration count.
