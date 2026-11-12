# Command key specifications

Run the documented offline example from the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_command_keys --exclude-test "*" --no-single
```

It owns a parsed COMMAND INFO snapshot, evaluates a binary GET key and prints
slot12739. Real applications fetch metadata through an authorized connection,
build a candidate catalogue, and publish that immutable snapshot only on OK.
Keep a reference while evaluating or replacing a catalogue. Only OK has a slot;
NONE requires an explicit keyless-command routing policy. UNKNOWN/UNSUPPORTED
require metadata/server assistance; CROSS_SLOT must not submit the business call.
This example performs no network or Redis writes.
