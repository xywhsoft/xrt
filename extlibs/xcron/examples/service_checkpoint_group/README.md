# Recover an entire cron group into a live service

Select XCRON_MODULE_XCRON_SERVICE_CHECKPOINT. Run the example without
arguments to create a temporary group, restore it into a live service,
advance both jobs, and reopen the committed result in a fresh service.

The example also accepts show or advance followed by a private directory
initialized with the [checkpoint group example](../checkpoint_group/README.md).
It loads the committed array, supplies the expected stable identities and
configurations, and publishes the live service with every job held.
Advance resumes one job at a time and waits for its physical completion.
One service request reserves both jobs, captures their successor records, and
commits the group on a dedicated storage executor. The request allows up to
ten seconds to reach storage admission. The owner restores previous dispatch
eligibility after the write and reports the durable generation.

Use advance-periodic with the same directory to arm a 500 ms cadence after
advancing both jobs. The example waits for two successful writes, stops and
collects the schedule, then reports the last durable generation.
[Startup contract](../../docs/service_checkpoint.md).
