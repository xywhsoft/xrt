# Recover an entire cron group into a live service

Select XCRON_MODULE_XCRON_SERVICE_CHECKPOINT. Run the example without
arguments to create a temporary group, restore it into a live service,
advance both jobs, and reopen the committed result in a fresh service.

The example also accepts show or advance followed by a private directory
initialized with the [checkpoint group example](../checkpoint_group/README.md).
It loads the committed array, supplies the expected stable identities and
configurations, and publishes the live service with every job held.
Advance resumes one job at a time and captures the result after its
physical completion. It commits both successor records in one generation.
[Startup contract](../../docs/service_checkpoint.md).
