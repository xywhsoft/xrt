# Durable Inbox

Run from the repository root:

    python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_inbox_tests

The example uses a native temporary journal. It records one already accepted
PUBLISH, takes its delivery, commits the application ACK and reopens the journal
to verify that the delivery is gone. It prints the application effect before
submitting its ACK. Applications must reconcile their own external effects
against the delivery key if a crash occurs between processing and acknowledgement.
