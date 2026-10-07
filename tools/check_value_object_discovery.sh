#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve previous backing discovery evidence\n' >&2; exit 1; }
mkdir -p "$OUT"
CC="$(command -v gcc)"; PY="$(command -v python3)"
"$PY" tools/amalgamate.py --check >"$OUT/generated-before.log" 2>&1
frozen_files() {
    find src include single config -type f -print0
    find tests/value tests/single -type f -print0
    printf '%s\0' examples/value/discovery/main.c
    printf '%s\0' "$CC" "$PY" tests/test.h tools/build.py tools/amalgamate.py
    printf '%s\0' tools/check_value_object_discovery.ps1 tools/check_value_object_discovery.sh
}
frozen_files | sort -zu >"$OUT/inputs.list"
xargs -0 sha256sum <"$OUT/inputs.list" >"$OUT/inputs.sha256"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
suite=value_object_discovery_tests,value_lifetime_copy_tests,value_finalizer_construction_tests,value_finalizer_publication_tests,value_finalizer_lifetime_tests,value_iterator_ownership_tests,ownership_adapter_tests
if ! "$PY" tools/build.py --compiler "$CC" --suite "$suite" --rebuild \
    --cflag=-O1 --cflag=-g --cflag=-fsanitize=address,undefined \
    --cflag=-fno-omit-frame-pointer --cflag=-fno-pie \
    --ldflag=-fsanitize=address,undefined --ldflag=-no-pie >"$OUT/gcc.log" 2>&1; then
    tail -n 35 "$OUT/gcc.log" >&2; exit 1
fi
[[ "$(grep -c '^\[test\]' "$OUT/gcc.log")" = 17 ]]
[[ "$(grep -c 'Object discovery: 200 copied backings, 200 detached/abort graphs, 800 concurrent lifetimes, full clone OOM prefix;' "$OUT/gcc.log")" = 2 ]]
[[ "$(grep -c 'Object read: exact family, prepared/committed, single/pair/shared receivers, NUL keys, allocation-free boundary, reentry/refusal/error cleanup, COW snapshot and once-only lifetime passed' "$OUT/gcc.log")" = 2 ]]
grep -q '^\[pass\]' "$OUT/gcc.log"
frozen_files | sort -zu >"$OUT/inputs.after.list"
cmp "$OUT/inputs.list" "$OUT/inputs.after.list"
sha256sum --check "$OUT/inputs.sha256" >"$OUT/inputs-check.log"
"$PY" -c 'import json,sys;json.dump({"Complete":True,"InputsUnchanged":True,"Tests":16,"Examples":1,"Programs":17,"DiscoveryLayouts":2,"ReadLayouts":2,"Scope":"Actual selected modular and single XRT implementations under Linux ASan/UBSan/leaks. Exact-policy protected single/pair/shared read views, prepared/committed, claimed/cleared/finalizing refusal, reentry, binary keys, prior/first error, explicit false, full alias-COW allocation prefix and ledgers. Original discovery/construction/copy/publication/finalizer/cursor/adapter populations retained. Same-object concurrent mutation, transitive payload immutability, xlang automatic collection/unmapping not certified."},open(sys.argv[1],"w"),indent=2)' "$OUT/results.json"
printf 'Backing discovery Linux: 16 tests / 1 example / 2 actual layouts, sanitizer and input inventory passed\n'
