#!/bin/sh

set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
OUT_DIR="$SCRIPT_DIR/bin"
CC=${CC:-gcc}

mkdir -p "$OUT_DIR"

i_pass=0
i_fail=0
i_total=0

tmp_list=$(mktemp)
find "$SCRIPT_DIR" -mindepth 3 -maxdepth 3 -name main.c | sort > "$tmp_list"

while IFS= read -r s_src
do
	s_name=$(basename "$(dirname "$s_src")")
	s_module=$(basename "$(dirname "$(dirname "$s_src")")")
	s_out="$OUT_DIR/${s_module}_${s_name}"

	printf '[BUILD] %s/%s\n' "$s_module" "$s_name"
	if "$CC" "$s_src" -I "$ROOT_DIR" -lm -pthread -ldl -o "$s_out"; then
		i_pass=$((i_pass + 1))
		printf '[ OK ] %s/%s\n' "$s_module" "$s_name"
	else
		i_fail=$((i_fail + 1))
		printf '[FAIL] %s/%s\n' "$s_module" "$s_name"
	fi

	i_total=$((i_total + 1))
done < "$tmp_list"

rm -f "$tmp_list"

printf '\nBuild finished.\n'
printf 'Total  : %s\n' "$i_total"
printf 'Success: %s\n' "$i_pass"
printf 'Failed : %s\n' "$i_fail"

exit 0
