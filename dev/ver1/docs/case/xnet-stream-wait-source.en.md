# XRT Case Study: xnet-v2 Stream Wait-Source

> A walkthrough of the current stream waiting mainline: `stream -> wait-source -> future -> thread/co wait`.

[中文](xnet-stream-wait-source.md) | [Back to Case Studies](README.en.md)

---

## What This Case Shows

The stream object in the current mainline is not only about sending and receiving bytes.

It can also be understood as a family of wait surfaces:

- readable
- writable
- drain
- close

This case focuses on why the current mainline prefers one unified waiting model instead of several unrelated helper families.

---

## Recommended Understanding

Think in this order:

1. `stream wait-kind`
2. `wait-source`
3. `future`
4. thread or coroutine wait

That way:

- stream waits
- dgram waits
- future waits

can all converge into one async mainline instead of growing apart.

---

## Related Reading

- [XNet V2](../api/api-xnet-v2.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Guide: xnet-v2 Stream Wait-Source Intro](../guide/xnet-stream-wait-source-intro.en.md)
