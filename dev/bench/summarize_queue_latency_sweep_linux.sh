#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INPUT_DIR="${1:-$ROOT/dev/bench/sweeps/queue_latency/linux}"
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
	mpsc_single_p95=""
	mpsc_batch_p95=""
	mpmc_single_p95=""
	mpmc_batch_p95=""
	mpmc_single_spread=""
	mpmc_batch_spread=""
	mpmc_single_maxmin=""
	mpmc_batch_maxmin=""

	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			queue\ latency\ sweep\ matrix\ \(*\))
				platform="${line#queue latency sweep matrix (}"
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
			mpsc_latency_single_p95_us:\ *)
				mpsc_single_p95="${line#mpsc_latency_single_p95_us: }"
				;;
			mpsc_latency_batch_p95_us:\ *)
				mpsc_batch_p95="${line#mpsc_latency_batch_p95_us: }"
				;;
			mpmc_latency_single_p95_us:\ *)
				mpmc_single_p95="${line#mpmc_latency_single_p95_us: }"
				;;
			mpmc_latency_batch_p95_us:\ *)
				mpmc_batch_p95="${line#mpmc_latency_batch_p95_us: }"
				;;
			mpmc_latency_single_consumer_spread_pct:\ *)
				mpmc_single_spread="${line#mpmc_latency_single_consumer_spread_pct: }"
				;;
			mpmc_latency_batch_consumer_spread_pct:\ *)
				mpmc_batch_spread="${line#mpmc_latency_batch_consumer_spread_pct: }"
				;;
			mpmc_latency_single_consumer_max_to_min:\ *)
				mpmc_single_maxmin="${line#mpmc_latency_single_consumer_max_to_min: }"
				;;
			mpmc_latency_batch_consumer_max_to_min:\ *)
				mpmc_batch_maxmin="${line#mpmc_latency_batch_consumer_max_to_min: }"
				;;
		esac
	done < "$file"

	if [ -n "$platform" ] && [ -n "$label" ] && [ -n "$items" ] && [ -n "$capacity" ] && \
		[ -n "$mpsc_producers" ] && [ -n "$mpmc_producers" ] && [ -n "$mpmc_consumers" ] && \
		[ -n "$batch_size" ] && [ -n "$mpsc_single_p95" ] && [ -n "$mpsc_batch_p95" ] && \
		[ -n "$mpmc_single_p95" ] && [ -n "$mpmc_batch_p95" ] && [ -n "$mpmc_single_spread" ] && \
		[ -n "$mpmc_batch_spread" ] && [ -n "$mpmc_single_maxmin" ] && [ -n "$mpmc_batch_maxmin" ]; then
		if [ -z "$PLATFORM" ]; then
			PLATFORM="$platform"
		fi
		mtime="$(stat -c %Y "$file")"
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"$label" "$policy" "$cpu_pin" "$batch_size" "$items" "$capacity" "$mpsc_producers" "$mpmc_producers" "$mpmc_consumers" "$mpsc_single_p95" "$mpsc_batch_p95" "$mpmc_single_p95" "$mpmc_batch_p95" "$mpmc_single_spread" "$mpmc_batch_spread" "$mpmc_single_maxmin" "$mpmc_batch_maxmin" "$mtime" >> "$TMP_ROWS"
	else
		SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
		SKIPPED_LIST+=("$(basename "$file")")
	fi
done

format_value() {
	awk -v value="$1" 'BEGIN { printf "%.3f", value }'
}

format_ratio() {
	awk -v num="$1" -v den="$2" 'BEGIN { if (den == 0.0) { printf "0.00x" } else { printf "%.2fx", num / den } }'
}

RAW_ROW_COUNT="$(wc -l < "$TMP_ROWS" | tr -d ' ')"
SORTED_ROWS="$(
	sort -t $'\t' -k1,1 -k2,2 -k3,3 -k4,4n -k5,5n -k6,6n -k7,7n -k8,8n -k9,9n -k18,18n "$TMP_ROWS" |
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
REPORT+="# Queue Latency Sweep Summary"$'\n'$'\n'
REPORT+="Generated: $(date '+%Y-%m-%d %H:%M:%S %z')"$'\n'$'\n'
REPORT+="- Platform: ${PLATFORM}"$'\n'
REPORT+="- Source dir: ${INPUT_DIR}"$'\n'
REPORT+="- Logs found: ${#FILES[@]}"$'\n'
REPORT+="- Logs used: ${USED_ROW_COUNT}"$'\n'
REPORT+="- Duplicate samples pruned: ${DEDUPED_COUNT}"$'\n'
REPORT+="- Logs skipped: ${SKIPPED_COUNT}"$'\n'
REPORT+="- Policies seen: ${POLICIES_SEEN}"$'\n'$'\n'
REPORT+="## Tail Latency"$'\n'$'\n'
REPORT+="| Label | Policy | CPU pin | Batch size | MPSC p95 single | MPSC p95 batch | MPSC p95 batch/single | MPMC p95 single | MPMC p95 batch | MPMC p95 batch/single |"$'\n'
REPORT+="|---|---|---|---:|---:|---:|---:|---:|---:|---:|"$'\n'
while IFS=$'\t' read -r label policy cpu_pin batch_size items capacity mpsc_p mpmc_p mpmc_c mpsc_single_p95 mpsc_batch_p95 mpmc_single_p95 mpmc_batch_p95 mpmc_single_spread mpmc_batch_spread mpmc_single_maxmin mpmc_batch_maxmin _mtime; do
	[ -n "$label" ] || continue
	REPORT+="| ${label} | ${policy} | ${cpu_pin} | ${batch_size} | $(format_value "$mpsc_single_p95") | $(format_value "$mpsc_batch_p95") | $(format_ratio "$mpsc_batch_p95" "$mpsc_single_p95") | $(format_value "$mpmc_single_p95") | $(format_value "$mpmc_batch_p95") | $(format_ratio "$mpmc_batch_p95" "$mpmc_single_p95") |"$'\n'
done <<< "$SORTED_ROWS"
REPORT+=$'\n'"## MPMC Fairness"$'\n'$'\n'
REPORT+="| Label | Policy | CPU pin | Batch size | Spread single pct | Spread batch pct | Max/min single | Max/min batch |"$'\n'
REPORT+="|---|---|---|---:|---:|---:|---:|---:|"$'\n'
while IFS=$'\t' read -r label policy cpu_pin batch_size items capacity mpsc_p mpmc_p mpmc_c mpsc_single_p95 mpsc_batch_p95 mpmc_single_p95 mpmc_batch_p95 mpmc_single_spread mpmc_batch_spread mpmc_single_maxmin mpmc_batch_maxmin _mtime; do
	[ -n "$label" ] || continue
	REPORT+="| ${label} | ${policy} | ${cpu_pin} | ${batch_size} | $(format_value "$mpmc_single_spread") | $(format_value "$mpmc_batch_spread") | $(format_value "$mpmc_single_maxmin") | $(format_value "$mpmc_batch_maxmin") |"$'\n'
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
