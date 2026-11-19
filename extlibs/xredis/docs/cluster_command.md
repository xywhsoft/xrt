# APP metadata-checked Cluster submission

The runnable [Cluster command example](../examples/cluster_command/README.md)
composes an immutable [command catalogue](command_keys.md) with the library's
explicit-slot asynchronous client. It checks every routing key before submitting
the command. The application chooses the metadata source, refresh schedule,
and any server-assisted extraction it authorizes. The
[cache example](../examples/cluster_command_cache/README.md) shows how to fetch
and replace a snapshot using an explicit primary slot.

Unknown, incomplete, cross-slot, invalid extraction and work-limit decisions
return before business admission. Keyless commands require an explicit fallback
slot. Variable access flags require separate application policy for replica
routing or replay. The native client owns overload, close, redirects, Future
results and request retirement.

The example requires a finite timeout in milliseconds. It charges catalogue
evaluation to that budget and rounds the remainder down before native admission.
An exhausted budget is submitted as immediately due. Callers hold a catalogue
reference through the adapter call and can release it immediately afterward.
Only ACCEPTED publishes an owned Future. Native admission copies the command
and preserves the existing lifetime and allocation bounds.

The APP adds lookup, extraction, CRC, output validation and two clock reads.
The local benchmark exercises explicit-slot calls, manual metadata-then-slot
calls, and the example adapter with identical native peers and binary payloads.
The [synchronous example](../examples/cluster_command_sync/README.md) uses the
same APP policy and calls the native blocking explicit-slot API.
