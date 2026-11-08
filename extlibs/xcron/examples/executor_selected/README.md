# Selected lease callback

From the repository root:

```text
python tools/build.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --suite xcron_executor_selected
```

The example resolves a repeated New York label, dispatches the selected lease to
a real XRT worker, reads its immutable nominal view, returns physical completion
to the scheduler, reaps the job and joins the borrowed executor before cleanup.
