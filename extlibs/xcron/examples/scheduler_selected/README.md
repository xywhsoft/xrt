# Policy-aware scheduling

From the repository root:

```text
python tools/build.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --suite xcron_scheduler_selected
```

The offline example dispatches only the globally last New York fall-back label,
using one unit in each search dimension per poll. It checks complete multiplicity,
physically completes the lease, closes and reaps the borrowed job before destroy.
No wall clock, executor, network or time-zone installation is needed.
