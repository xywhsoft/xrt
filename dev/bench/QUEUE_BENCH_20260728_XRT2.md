# XRT 2 Queue Throughput Baseline (2026-07-28)

This report records the first local Windows throughput baseline for the
refactored XRT 2 pointer-queue family.


## Sources

- `dev/bench/queue/bench_queue_pointer.c`
- `dev/bench/run_queue_bench_windows.ps1`
- `single/xrt.h`


## Environment

- OS: Windows 10 Pro 10.0.19045 x64
- CPU: AMD Ryzen 5 5600, 6 cores / 12 logical processors
- compiler: GCC 16.1.0
- build: C11, `-O2`, `-Wall -Wextra -Werror`
- policy: serial runs, process unpinned


## Matrix

- items per producer: `500000`
- capacity: `4096`
- MPSC producers: `4`
- MPMC producers: `4`
- MPMC consumers: `4`
- batch size: `32`
- samples: `3`


## Results

| Lane | Items | Samples (items/s) | Median (items/s) |
|---|---:|---:|---:|
| `SPSC` | `500000` | `28,156,799.586`; `28,811,967.339`; `30,437,879.332` | `28,811,967.339` |
| `MPSC` | `2000000` | `8,135,048.310`; `9,488,260.176`; `8,070,233.629` | `8,135,048.310` |
| `MPSC batch 32` | `2000000` | `98,299,903.175`; `98,070,463.628`; `117,955,825.543` | `98,299,903.175` |
| `MPMC` | `2000000` | `6,591,453.785`; `6,627,232.425`; `6,689,527.010` | `6,627,232.425` |
| `MPMC batch 32` | `2000000` | `125,923,967.109`; `136,885,043.940`; `134,432,091.629` | `134,432,091.629` |


## Interpretation

- The single-item SPSC, MPSC, and MPMC paths remain in the expected throughput
  range after adding object/storage alias checks and damaged-state guards.
- Batch reservation amortizes validation and atomic coordination effectively.
  It is the preferred high-throughput path when the caller can naturally group
  items.
- The three runs are intentionally unpinned. Scheduler variation is visible,
  especially in the MPSC sample, so the median is the comparison value.
- The legacy reports dated 2026-03 were produced by the old tree. They remain
  useful implementation evidence but are not interchangeable with this XRT 2
  baseline.


## Reproduction

Run three serial samples:

```powershell
$env:PATH = 'E:\software\w64devkit\bin;' + $env:PATH

for ( $run = 1; $run -le 3; ++$run ) {
	powershell.exe -NoProfile -ExecutionPolicy Bypass `
		-File dev/bench/run_queue_bench_windows.ps1
}
```
