# Recover an entire cron group into a live service

Select XCRON_MODULE_XCRON_SERVICE_CHECKPOINT. Run the example without
arguments to create a temporary group, restore it into a live service,
advance both jobs, and reopen the committed result in a fresh service.

The example also accepts show or advance followed by a private directory
initialized with the [checkpoint group example](../checkpoint_group/README.md).
It loads the committed array, supplies the expected stable identities and
configurations, and publishes the live service with every job held.
Advance resumes one job at a time and waits for its physical completion.
One quiescence request captures both successor records and holds their
dispatch. The request allows up to ten seconds for previously issued leases
and recurrence searches. The caller submits those records to a dedicated storage executor. The live
owner can answer a job query while storage handles the group commit. The
reply identifies the durable generation.
[Startup contract](../../docs/service_checkpoint.md).
