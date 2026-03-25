#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INPUT_DIR="${1:-$ROOT/dev/bench/sweeps/queue/linux}"
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
	family=""
	label=""
	policy="baseline-unpinned"
	cpu_pin="disabled"
	items=""
	capacity=""
	mpsc_producers=""
	mpmc_producers=""
	mpmc_consumers=""
	spsc_rate=""
	mpsc_rate=""
	mpmc_rate=""

	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			queue\ sweep\ matrix\ \(*\))
				platform="${line#queue sweep matrix (}"
				platform="${platform%)}"
				;;
			family:\ *)
				family="${line#family: }"
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
			spsc_items_per_sec:\ *)
				spsc_rate="${line#spsc_items_per_sec: }"
				;;
			mpsc_items_per_sec:\ *)
				mpsc_rate="${line#mpsc_items_per_sec: }"
				;;
			mpmc_items_per_sec:\ *)
				mpmc_rate="${line#mpmc_items_per_sec: }"
				;;
		esac
	done < "$file"

	if [ -n "$platform" ] && [ -n "$family" ] && [ -n "$label" ] && [ -n "$items" ] && \
		[ -n "$capacity" ] && [ -n "$mpsc_producers" ] && [ -n "$mpmc_producers" ] && \
		[ -n "$mpmc_consumers" ] && [ -n "$spsc_rate" ] && [ -n "$mpsc_rate" ] && [ -n "$mpmc_rate" ]; then
		if [ -z "$PLATFORM" ]; then
			PLATFORM="$platform"
		fi
		mtime="$(stat -c %Y "$file")"
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"$family" "$label" "$policy" "$cpu_pin" "$items" "$capacity" "$mpsc_producers" "$mpmc_producers" "$mpmc_consumers" "$spsc_rate" "$mpsc_rate" "$mpmc_rate" "$mtime" >> "$TMP_ROWS"
	else
		SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
		SKIPPED_LIST+=("$(basename "$file")")
	fi
done

format_rate() {
	awk -v value="$1" 'BEGIN { printf "%.1f", value }'
}

append_family_table() {
	local family="$1"
	local title="$2"
	local sort_cmd="$3"
	local rows=""

	rows="$(
		awk -F '\t' -v family="$family" '$1 == family { print }' "$TMP_ROWS" | eval "$sort_cmd"
	)"
	if [ -z "$rows" ]; then
		return
	fi

	REPORT+="## ${title}"$'\n'$'\n'
	REPORT+="| Label | Policy | CPU pin | Items/producer | Capacity | MPSC P | MPMC P | MPMC C | SPSC items/s | MPSC items/s | MPMC items/s |"$'\n'
	REPORT+="|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"$'\n'
	while IFS=$'\t' read -r row_family label policy cpu_pin items capacity mpsc_p mpmc_p mpmc_c spsc_rate mpsc_rate mpmc_rate _mtime; do
		[ -n "$label" ] || continue
		REPORT+="| ${label} | ${policy} | ${cpu_pin} | ${items} | ${capacity} | ${mpsc_p} | ${mpmc_p} | ${mpmc_c} | $(format_rate "$spsc_rate") | $(format_rate "$mpsc_rate") | $(format_rate "$mpmc_rate") |"$'\n'
	done <<< "$rows"
	REPORT+=$'\n'
}

REPORT=""
REPORT+="# Queue Bench Sweep Summary"$'\n'$'\n'
REPORT+="Generated: $(date '+%Y-%m-%d %H:%M:%S %z')"$'\n'$'\n'
REPORT+="- Platform: ${PLATFORM}"$'\n'
RAW_ROW_COUNT="$(wc -l < "$TMP_ROWS" | tr -d ' ')"
DEDUPED_ROWS="$(
	sort -t $'\t' -k1,1 -k2,2 -k3,3 -k4,4 -k5,5n -k6,6n -k7,7n -k8,8n -k9,9n -k13,13n "$TMP_ROWS" |
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
	'
)"
USED_ROW_COUNT="$(printf '%s\n' "$DEDUPED_ROWS" | awk 'NF { count++ } END { print count + 0 }')"
DEDUPED_COUNT=$((RAW_ROW_COUNT - USED_ROW_COUNT))
printf '%s\n' "$DEDUPED_ROWS" > "$TMP_ROWS"
POLICIES_SEEN="$(
	printf '%s\n' "$DEDUPED_ROWS" |
	awk -F '\t' 'NF { print $3 " (cpu " $4 ")" }' |
	sort -u |
	awk 'BEGIN { sep = "" } NF { printf "%s%s", sep, $0; sep = ", " } END { if (sep == "") printf "none" }'
)"

REPORT+="- Source dir: ${INPUT_DIR}"$'\n'
REPORT+="- Logs found: ${#FILES[@]}"$'\n'
REPORT+="- Logs used: ${USED_ROW_COUNT}"$'\n'
REPORT+="- Duplicate samples pruned: ${DEDUPED_COUNT}"$'\n'
REPORT+="- Logs skipped: ${SKIPPED_COUNT}"$'\n'
REPORT+="- Policies seen: ${POLICIES_SEEN}"$'\n'$'\n'

append_family_table "capacity" "Capacity Sweep" "sort -t $'\t' -k6,6n -k2,2"
append_family_table "mpsc_producers" "MPSC Producer Sweep" "sort -t $'\t' -k7,7n -k2,2"
append_family_table "mpmc_pairs" "MPMC Pair Sweep" "sort -t $'\t' -k8,8n -k9,9n -k2,2"

if [ "${#SKIPPED_LIST[@]}" -gt 0 ]; then
	REPORT+="Skipped logs:"$'\n'
	for name in "${SKIPPED_LIST[@]}"; do
		REPORT+="- ${name}"$'\n'
	done
fi

if [ -n "$OUT_FILE" ]; then
	printf '%s' "$REPORT" > "$OUT_FILE"
fi

printf '%s' "$REPORT"
