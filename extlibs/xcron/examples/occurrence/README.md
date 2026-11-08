# Budgeted selected recurring occurrence

Run from the repository root:

```console
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_occurrence
```

This offline example creates explicit owned POSIX rules, compiles a daily01:30
expression and searches the2024 autumn fold with global LAST selection. It
resumes with one unit in each independent traversal dimension and verifies the
second actual instant, one hour after the first, with complete multiplicity2.
The retained zone stays live through the last Step and is released on every exit.
The cursor borrows it and owns no cleanup resource. No installed timezone data,
network, scheduler, background thread or environment mutation is needed.

Expected output: `Selected global last fold through budgeted recurring search.`

See the [full contract, gap rules and costs](../../docs/occurrence.md). Existing
kernel/service jobs still use their original actual-occurrence semantics until
explicit policy integration; this example does not represent physical delivery
or gap compensation.
