#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

mkdir -p release/linux

gcc test.c -I . -pthread -O0 -g -o release/linux/test_queue_linux

./release/linux/test_queue_linux queue_core
./release/linux/test_queue_linux thread_core
./release/linux/test_queue_linux xnet2_engine

gcc test_trim.c -I . -pthread -O0 -g -o release/linux/test_trim_linux
./release/linux/test_trim_linux

gcc singlehead/test_singlehead.c -I singlehead -pthread -O0 -g -o release/linux/test_singlehead_queue_linux
./release/linux/test_singlehead_queue_linux

gcc singlehead/test_singlehead_trim.c -I singlehead -pthread -O0 -g -o release/linux/test_singlehead_trim_linux
./release/linux/test_singlehead_trim_linux

run_noqueue_probe() {
	local probe_file="$1"
	local probe_header="$2"
	local probe_name="$3"

	cat >"$probe_file" <<EOF
#define XRT_NO_QUEUE
#define XRT_NO_NETWORK
#include "$probe_header"
int main(void) { xqueue_config cfg; return (int)sizeof(cfg); }
EOF

	if gcc "$probe_file" -I . -c -o /dev/null >"${probe_file}.log" 2>&1; then
		printf '%s\n' "[ERROR] ${probe_name} no-queue probe compiled unexpectedly."
		cat "${probe_file}.log"
		rm -f "$probe_file" "${probe_file}.log"
		exit 1
	fi

	if ! grep -q "xqueue_config" "${probe_file}.log"; then
		printf '%s\n' "[ERROR] ${probe_name} no-queue probe failed for an unexpected reason."
		cat "${probe_file}.log"
		rm -f "$probe_file" "${probe_file}.log"
		exit 1
	fi

	rm -f "$probe_file" "${probe_file}.log"
}

run_noqueue_probe release/linux/__trim_noqueue_probe.c xrt.h normal
run_noqueue_probe release/linux/__singlehead_trim_noqueue_probe.c singlehead/xrt.h singlehead

printf '\n%s\n' "build_GCC_QUEUE_REGRESSION_linux.sh: PASS"
