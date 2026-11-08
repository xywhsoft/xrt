# Explicit nominal label selection

From the repository root, run:

```console
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_delivery
```

The offline example creates explicit owned rules, selects both ends of a repeated
01:30 label with FIRST/LAST and asserts one hour difference. It then explicitly
skips a missing 02:30 label and releases its zone. No installed timezone data,
network, service or environment change is required. Expected output:
`Selected first/last repeated label one hour apart; missing label skipped.`

This example exercises one-label conversion. The existing scheduler still uses
all actual instants; recurring policy integration and gap compensation remain
later stages. See [selection semantics and costs](../../docs/delivery.md).
