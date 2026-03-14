#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INPUT_DIR="${1:-$ROOT/dev/bench/sweeps/linux}"
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

TMP_METRICS="$(mktemp)"
trap 'rm -f "$TMP_METRICS"' EXIT

declare -A INCOMPLETE=()
declare -a USED_FILES=()
declare -a SKIPPED_FILES=()

PLATFORM=""
MATRIX_TCP=""
MATRIX_TLS=""
MATRIX_UDP=""
MATRIX_PRESSURE=""

for file in "${FILES[@]}"; do
	section=""
	has_matrix=0
	saw_metric=0
	TMP_FILE_METRICS="$(mktemp)"
	declare -A FILE_INCOMPLETE=()

	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			xnet\ compare\ matrix\ \(*\))
				has_matrix=1
				if [ -z "$PLATFORM" ]; then
					PLATFORM="${line#xnet compare matrix (}"
					PLATFORM="${PLATFORM%)}"
				fi
				continue
				;;
			tcp:\ *)
				if [ -z "$MATRIX_TCP" ]; then
					MATRIX_TCP="${line#tcp: }"
				fi
				continue
				;;
			tls:\ *)
				if [ -z "$MATRIX_TLS" ]; then
					MATRIX_TLS="${line#tls: }"
				fi
				continue
				;;
			udp:\ *)
				if [ -z "$MATRIX_UDP" ]; then
					MATRIX_UDP="${line#udp: }"
				fi
				continue
				;;
			pressure:\ *)
				if [ -z "$MATRIX_PRESSURE" ]; then
					MATRIX_PRESSURE="${line#pressure: }"
				fi
				continue
				;;
			xnet-v1\ bench_echo_tcp)
				section="v1_tcp"
				continue
				;;
			xnet2\ bench_echo_tcp)
				section="v2_tcp"
				continue
				;;
			xnet-v1\ bench_echo_tls)
				section="v1_tls"
				continue
				;;
			xnet2\ bench_echo_tls)
				section="v2_tls"
				continue
				;;
			xnet-v1\ bench_udp_burst)
				section="v1_udp"
				continue
				;;
			xnet2\ bench_udp_burst)
				section="v2_udp"
				continue
				;;
			xnet2\ bench_send_queue_pressure)
				section="v2_pressure"
				continue
				;;
			benchmark_incomplete:\ *)
				name="${line#benchmark_incomplete: }"
				if [ -n "${FILE_INCOMPLETE[$name]:-}" ]; then
					FILE_INCOMPLETE["$name"]=$((FILE_INCOMPLETE["$name"] + 1))
				else
					FILE_INCOMPLETE["$name"]=1
				fi
				continue
				;;
		esac

		case "$section|$line" in
			v1_tcp\|messages_per_sec:\ *)
				printf 'v1_tcp_msgps\t%s\n' "${line#messages_per_sec: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_tcp\|messages_per_sec_total:\ *)
				printf 'v2_tcp_total_msgps\t%s\n' "${line#messages_per_sec_total: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_tcp\|messages_per_sec_steady:\ *)
				printf 'v2_tcp_steady_msgps\t%s\n' "${line#messages_per_sec_steady: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v1_tls\|messages_per_sec:\ *)
				printf 'v1_tls_msgps\t%s\n' "${line#messages_per_sec: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_tls\|messages_per_sec_total:\ *)
				printf 'v2_tls_total_msgps\t%s\n' "${line#messages_per_sec_total: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_tls\|messages_per_sec_steady:\ *)
				printf 'v2_tls_steady_msgps\t%s\n' "${line#messages_per_sec_steady: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v1_udp\|send_packets_per_sec:\ *)
				printf 'v1_udp_send_pps\t%s\n' "${line#send_packets_per_sec: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_udp\|send_packets_per_sec:\ *)
				printf 'v2_udp_send_pps\t%s\n' "${line#send_packets_per_sec: }" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
			v2_pressure\|drain_elapsed_ns:\ *)
				value_ns="${line#drain_elapsed_ns: }"
				value_ms="$(awk -v ns="$value_ns" 'BEGIN { printf "%.6f", ns / 1000000.0 }')"
				printf 'v2_pressure_drain_ms\t%s\n' "$value_ms" >> "$TMP_FILE_METRICS"
				saw_metric=1
				;;
		esac
	done < "$file"

	if { [ "$has_matrix" -eq 1 ] && [ "$saw_metric" -eq 1 ]; } || {
		grep -q '^v2_tcp_total_msgps' "$TMP_FILE_METRICS" &&
		grep -q '^v2_tls_total_msgps' "$TMP_FILE_METRICS" &&
		grep -q '^v2_udp_send_pps' "$TMP_FILE_METRICS" &&
		grep -q '^v2_pressure_drain_ms' "$TMP_FILE_METRICS"
	}; then
		cat "$TMP_FILE_METRICS" >> "$TMP_METRICS"
		for name in "${!FILE_INCOMPLETE[@]}"; do
			if [ -n "${INCOMPLETE[$name]:-}" ]; then
				INCOMPLETE["$name"]=$((INCOMPLETE["$name"] + FILE_INCOMPLETE["$name"]))
			else
				INCOMPLETE["$name"]="${FILE_INCOMPLETE["$name"]}"
			fi
		done
		USED_FILES+=("$(basename "$file")")
	else
		SKIPPED_FILES+=("$(basename "$file")")
	fi
	rm -f "$TMP_FILE_METRICS"
done

format_number() {
	awk -v value="$1" 'BEGIN { printf "%.1f", value }'
}

metric_row() {
	local metric_name="$1"
	local label="$2"
	local row

	row="$(awk -F '\t' -v key="$metric_name" '
		function push(v) {
			count += 1
			sum += v
			sumsq += v * v
			if (count == 1 || v < min) {
				min = v
			}
			if (count == 1 || v > max) {
				max = v
			}
		}
		$1 == key {
			push($2 + 0.0)
		}
		END {
			if (count == 0) {
				exit 1
			}
			mean = sum / count
			var = (sumsq / count) - (mean * mean)
			if (var < 0.0) {
				var = 0.0
			}
			cv = (mean != 0.0) ? (sqrt(var) / mean) * 100.0 : 0.0
			printf "%d\t%.6f\t%.6f\t%.6f\t%.6f", count, mean, min, max, cv
		}
	' "$TMP_METRICS" 2>/dev/null)" || return 0

	IFS=$'\t' read -r samples avg min max cv <<< "$row"
	printf '| %s | %s | %s | %s | %s | %s |\n' \
		"$label" \
		"$samples" \
		"$(format_number "$avg")" \
		"$(format_number "$min")" \
		"$(format_number "$max")" \
		"$(format_number "$cv")"
}

append_metric_row() {
	local row
	row="$(metric_row "$1" "$2")"
	if [ -n "$row" ]; then
		REPORT+="$row"$'\n'
	fi
}

REPORT=""
REPORT+="# XNET Compare Sweep Summary"$'\n'$'\n'
REPORT+="Generated: $(date '+%Y-%m-%d %H:%M:%S %z')"$'\n'$'\n'
REPORT+="- Platform: ${PLATFORM}"$'\n'
REPORT+="- Source dir: ${INPUT_DIR}"$'\n'
REPORT+="- Logs found: ${#FILES[@]}"$'\n'
REPORT+="- Logs used: ${#USED_FILES[@]}"$'\n'
REPORT+="- Logs skipped: ${#SKIPPED_FILES[@]}"$'\n'
REPORT+="- Matrix TCP: ${MATRIX_TCP}"$'\n'
REPORT+="- Matrix TLS: ${MATRIX_TLS}"$'\n'
REPORT+="- Matrix UDP: ${MATRIX_UDP}"$'\n'
REPORT+="- Matrix pressure: ${MATRIX_PRESSURE}"$'\n'$'\n'
REPORT+="| Metric | Samples | Avg | Min | Max | CV % |"$'\n'
REPORT+="|---|---:|---:|---:|---:|---:|"$'\n'
append_metric_row "v1_tcp_msgps" "xnet-v1 TCP msg/s"
append_metric_row "v2_tcp_total_msgps" "xnet-v2 TCP total msg/s"
append_metric_row "v2_tcp_steady_msgps" "xnet-v2 TCP steady msg/s"
append_metric_row "v1_tls_msgps" "xnet-v1 TLS msg/s"
append_metric_row "v2_tls_total_msgps" "xnet-v2 TLS total msg/s"
append_metric_row "v2_tls_steady_msgps" "xnet-v2 TLS steady msg/s"
append_metric_row "v1_udp_send_pps" "xnet-v1 UDP send pps"
append_metric_row "v2_udp_send_pps" "xnet-v2 UDP send pps"
append_metric_row "v2_pressure_drain_ms" "xnet-v2 queue drain ms"

if [ "${#INCOMPLETE[@]}" -gt 0 ]; then
	REPORT+=$'\n'"| Incomplete benchmark | Count |"$'\n'
	REPORT+="|---|---:|"$'\n'
	while IFS= read -r name; do
		[ -n "$name" ] || continue
		REPORT+="| ${name} | ${INCOMPLETE[$name]} |"$'\n'
	done < <(printf '%s\n' "${!INCOMPLETE[@]}" | sort)
fi

if [ "${#SKIPPED_FILES[@]}" -gt 0 ]; then
	REPORT+=$'\n'"Skipped logs:"$'\n'
	for name in "${SKIPPED_FILES[@]}"; do
		REPORT+="- ${name}"$'\n'
	done
fi

if [ -n "$OUT_FILE" ]; then
	printf '%s' "$REPORT" > "$OUT_FILE"
fi

printf '%s' "$REPORT"
