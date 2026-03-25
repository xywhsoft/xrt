#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INPUT_DIR="${1:-$ROOT/dev/bench/sweeps/queue_batch/linux}"
OUT_FILE="${2:-}"

if [ ! -d "$INPUT_DIR" ]; then
	printf '%s\n' "input directory not found: $INPUT_DIR" >&2
	exit 1
fi

shopt -s nullglob
FILES=("$INPUT_DIR"/*.log)
shopt -u nullglob

if [ "${#FILES[@]}" -eq 0 ]; then
	printf '%s\n' "no sweep logs found under: $INPUT_DIR" >&2
	exit 1
fi

TMP_ROWS="$(mktemp)"
trap 'rm -f "$TMP_ROWS"' EXIT

PLATFORM=""
SKIPPED_COUNT=0
SKIPPED_LIST=()

for file in "${FILES[@]}"; do
	platform=""
	label=""
	policy="baseline-unpinned"
	cpu_pin="disabled"
	items=""
	capacity=""
	mpsc_producers=""
	mpmc_producers=""
	mpmc_consumers=""
	batch_size=""
	mpsc_single=""
	mpsc_batch=""
	mpmc_single=""
	mpmc_batch=""

	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			queue\ batch\ sweep\ matrix\ \(*\))
				platform="${line#queue batch sweep matrix (}"
				platform="${platform%)}"
				;;
			label:\ *)
				label="${line#label: }"
				;;
			policy:\ *)
				policy="${line#policy: }"
				;;
			cpu_pin:\ *)
				cpu_pin="${line#cpu_pin: }"
				;;
			items_per_producer:\ *)
				items="${line#items_per_producer: }"
				;;
			capacity:\ *)
				capacity="${line#capacity: }"
				;;
			mpsc_producers:\ *)
				mpsc_producers="${line#mpsc_producers: }"
				;;
			mpmc_producers:\ *)
				mpmc_producers="${line#mpmc_producers: }"
				;;
			mpmc_consumers:\ *)
				mpmc_consumers="${line#mpmc_consumers: }"
				;;
			batch_size:\ *)
				batch_size="${line#batch_size: }"
				;;
			mpsc_items_per_sec:\ *)
				mpsc_single="${line#mpsc_items_per_sec: }"
				;;
			mpsc_batch_items_per_sec:\ *)
				mpsc_batch="${line#mpsc_batch_items_per_sec: }"
				;;
			mpmc_items_per_sec:\ *)
				mpmc_single="${line#mpmc_items_per_sec: }"
				;;
			mpmc_batch_items_per_sec:\ *)
				mpmc_batch="${line#mpmc_batch_items_per_sec: }"
				;;
		esac
	done < "$file"

	if [ -n "$platform" ] && [ -n "$label" ] && [ -n "$items" ] && [ -n "$capacity" ] && \
		[ -n "$mpsc_producers" ] && [ -n "$mpmc_producers" ] && [ -n "$mpmc_consumers" ] && \
		[ -n "$batch_size" ] && [ -n "$mpsc_single" ] && [ -n "$mpsc_batch" ] && [ -n "$mpmc_single" ] && [ -n "$mpmc_batch" ]; then
		if [ -z "$PLATFORM" ]; then
			PLATFORM="$platform"
		fi
		mtime="$(stat -c %Y "$file")"
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"$label" "$policy" "$cpu_pin" "$batch_size" "$items" "$capacity" "$mpsc_producers" "$mpmc_producers" "$mpmc_consumers" "$mpsc_single" "$mpsc_batch" "$mpmc_single" "$mpmc_batch" "$mtime" >> "$TMP_ROWS"
	else
		SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
		SKIPPED_LIST+=("$(basename "$file")")
	fi
done

format_rate() {
	awk -v value="$1" 'BEGIN { printf "%.1f", value }'
}

format_ratio() {
	awk -v num="$1" -v den="$2" 'BEGIN { if (den == 0.0) { printf "0.00x" } else { printf "%.2fx", num / den } }'
}

RAW_ROW_COUNT="$(wc -l < "$TMP_ROWS" | tr -d ' ')"
SORTED_ROWS="$(
	sort -t $'\t' -k1,1 -k2,2 -k3,3 -k4,4n -k5,5n -k6,6n -k7,7n -k8,8n -k9,9n -k14,14n "$TMP_ROWS" |
	awk -F '\t' '
		{
			key = $1 FS $2 FS $3 FS $4 FS $5 FS $6 FS $7 FS $8 FS $9
			rows[key] = $0
		}
		END {
			for (key in rows) {
				print rows[key]
			}
		}
	' |
	sort -t $'\t' -k4,4n -k1,1 -k2,2 -k3,3
)"
USED_ROW_COUNT="$(printf '%s\n' "$SORTED_ROWS" | awk 'NF { count++ } END { print count + 0 }')"
DEDUPED_COUNT=$((RAW_ROW_COUNT - USED_ROW_COUNT))
POLICIES_SEEN="$(
	printf '%s\n' "$SORTED_ROWS" |
	awk -F '\t' 'NF { print $2 " (cpu " $3 ")" }' |
	sort -u |
	awk 'BEGIN { sep = "" } NF { printf "%s%s", sep, $0; sep = ", " } END { if (sep == "") printf "none" }'
)"

REPORT=""
REPORT+="# Queue Batch Sweep Summary"$'\n'$'\n'
REPORT+="Generated: $(date '+%Y-%m-%d %H:%M:%S %z')"$'\n'$'\n'
REPORT+="- Platform: ${PLATFORM}"$'\n'
REPORT+="- Source dir: ${INPUT_DIR}"$'\n'
REPORT+="- Logs found: ${#FILES[@]}"$'\n'
REPORT+="- Logs used: ${USED_ROW_COUNT}"$'\n'
REPORT+="- Duplicate samples pruned: ${DEDUPED_COUNT}"$'\n'
REPORT+="- Logs skipped: ${SKIPPED_COUNT}"$'\n'
REPORT+="- Policies seen: ${POLICIES_SEEN}"$'\n'$'\n'
REPORT+="| Label | Policy | CPU pin | Batch size | Items/producer | Capacity | MPSC P | MPMC P | MPMC C | MPSC single | MPSC batch | MPSC batch/single | MPMC single | MPMC batch | MPMC batch/single |"$'\n'
REPORT+="|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"$'\n'
while IFS=$'\t' read -r label policy cpu_pin batch_size items capacity mpsc_p mpmc_p mpmc_c mpsc_single mpsc_batch mpmc_single mpmc_batch _mtime; do
	[ -n "$label" ] || continue
	REPORT+="| ${label} | ${policy} | ${cpu_pin} | ${batch_size} | ${items} | ${capacity} | ${mpsc_p} | ${mpmc_p} | ${mpmc_c} | $(format_rate "$mpsc_single") | $(format_rate "$mpsc_batch") | $(format_ratio "$mpsc_batch" "$mpsc_single") | $(format_rate "$mpmc_single") | $(format_rate "$mpmc_batch") | $(format_ratio "$mpmc_batch" "$mpmc_single") |"$'\n'
done <<< "$SORTED_ROWS"

if [ "${#SKIPPED_LIST[@]}" -gt 0 ]; then
	REPORT+=$'\n'"Skipped logs:"$'\n'
	for name in "${SKIPPED_LIST[@]}"; do
		REPORT+="- ${name}"$'\n'
	done
fi

if [ -n "$OUT_FILE" ]; then
	printf '%s' "$REPORT" > "$OUT_FILE"
fi

printf '%s' "$REPORT"
