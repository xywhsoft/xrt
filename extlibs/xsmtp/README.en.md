# xsmtp

`xsmtp` is an `xrt` extension library for SMTP mail sending.

It is designed for lightweight integration in native projects that already use
`xrt`, without adding an external SMTP dependency.

## Feature Summary

- plain SMTP
- SMTPS / implicit TLS (`465`)
- STARTTLS (`587`)
- `AUTH PLAIN`
- `AUTH LOGIN`
- UTF-8 subject and mailbox name encoding
- `text/plain`
- `text/html`
- `multipart/alternative`
- synchronous send API
- future-based asynchronous send API
- coroutine-friendly async wait API

## Directory Layout

- [xsmtp.h](D:/Git/xrt/extlibs/xsmtp/xsmtp.h)
  The implementation and public API surface.
- [xsmtp_test.c](D:/Git/xrt/extlibs/xsmtp/xsmtp_test.c)
  A formal example program with all SMTP settings declared at the top of the
  file.
- [build_test.bat](D:/Git/xrt/extlibs/xsmtp/build_test.bat)
  The local build script for the example program.

## Quick Start

1. Open [xsmtp_test.c](D:/Git/xrt/extlibs/xsmtp/xsmtp_test.c).
2. Edit the `XSMTP_EXAMPLE_*` constants at the top of the file.
3. Build the example:

```bat
build_test.bat
```

4. Run the example:

```bat
.\bin\xsmtp_test.exe
```

The example is intentionally self-contained. It does not read configuration
from `xadmin` or any external JSON file.

## Example Configuration

The example program starts with a single editable block:

```c
#define XSMTP_EXAMPLE_HOST            "smtp.example.com"
#define XSMTP_EXAMPLE_PORT            587
#define XSMTP_EXAMPLE_SECURE_MODE     XSMTP_SECURE_STARTTLS
#define XSMTP_EXAMPLE_AUTH            1
#define XSMTP_EXAMPLE_AUTH_MODE       XSMTP_AUTH_AUTO
#define XSMTP_EXAMPLE_VERIFY_PEER     1
#define XSMTP_EXAMPLE_TIMEOUT_MS      15000
#define XSMTP_EXAMPLE_HELO_NAME       "localhost"

#define XSMTP_EXAMPLE_USERNAME        "user@example.com"
#define XSMTP_EXAMPLE_PASSWORD        "replace-with-app-password"

#define XSMTP_EXAMPLE_FROM_EMAIL      "user@example.com"
#define XSMTP_EXAMPLE_FROM_NAME       "xsmtp example"
#define XSMTP_EXAMPLE_REPLY_TO_EMAIL  "user@example.com"
#define XSMTP_EXAMPLE_REPLY_TO_NAME   "xsmtp example"

#define XSMTP_EXAMPLE_TO_EMAIL        "receiver@example.com"
#define XSMTP_EXAMPLE_TO_NAME         "Receiver"

#define XSMTP_EXAMPLE_USE_ASYNC       1
#define XSMTP_EXAMPLE_USE_COROUTINE   0
```

Recommended defaults:

- use port `465` with `XSMTP_SECURE_SSL`, or
- use port `587` with `XSMTP_SECURE_STARTTLS`

Do not commit real SMTP passwords or provider app tokens.

## Build

From [build_test.bat](D:/Git/xrt/extlibs/xsmtp/build_test.bat):

```bat
build_test.bat
```

Current build command:

```bat
gcc -m64 xsmtp_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g -DXSMTP_DEBUG -o .\bin\xsmtp_test.exe
```

Output:

```text
.\bin\xsmtp_test.exe
```

## Supported Security Modes

- `XSMTP_SECURE_NONE`
  Plain SMTP without TLS.
- `XSMTP_SECURE_SSL`
  Implicit TLS, typically port `465`.
- `XSMTP_SECURE_STARTTLS`
  Plain connect first, then upgrade via `STARTTLS`, typically port `587`.
- `XSMTP_SECURE_AUTO`
  Chooses by port:
  - `465` -> implicit TLS
  - `587` -> STARTTLS
  - otherwise -> plain SMTP

## Supported Auth Modes

- `XSMTP_AUTH_NONE`
  No SMTP auth.
- `XSMTP_AUTH_PLAIN`
  Force `AUTH PLAIN`.
- `XSMTP_AUTH_LOGIN`
  Force `AUTH LOGIN`.
- `XSMTP_AUTH_AUTO`
  Select based on server capabilities.

Recommended default:

- `XSMTP_AUTH_AUTO`

## Core Types

### `xsmtpconfig`

Connection and sender configuration.

Important fields:

- `sHost`
- `iPort`
- `iTimeoutMs`
- `iSecureMode`
- `bAuth`
- `iAuthMode`
- `bVerifyPeer`
- `sUser`
- `sPass`
- `sHeloName`
- `tFrom`
- `tReplyTo`

### `xsmtpmessage`

Mail content and recipient data.

Important fields:

- `tFrom`
- `tReplyTo`
- `arrTo`, `iToCount`
- `arrCc`, `iCcCount`
- `arrBcc`, `iBccCount`
- `sSubject`
- `sTextBody`
- `sHtmlBody`
- `arrHeaderNames`
- `arrHeaderValues`
- `iHeaderCount`

### `xsmtpresult`

Send result and diagnostics.

Important fields:

- `bSuccess`
- `bUsedTLS`
- `bUsedStartTLS`
- `iServerCode`
- `iAuthMode`
- `iCapabilities`
- `sError`
- `sLastReply`

### `xsmtpasyncopts`

Async send options.

Important fields:

- `iTimeoutMs`
- `pEngine`
- `sDebugName`

## Public API

Initialization helpers:

- `xrtSmtpConfigInit(...)`
- `xrtSmtpMessageInit(...)`
- `xrtSmtpResultInit(...)`
- `xrtSmtpAsyncOptsInit(...)`
- `xrtSmtpResultFree(...)`

Send APIs:

- `xrtSmtpSendMail(...)`
- `xrtSmtpSendMailFuture(...)`
- `xrtSmtpSendMailAsyncWait(...)`
- `xrtSmtpSendMailCo(...)`

## Minimal Sync Example

```c
xsmtpconfig cfg;
xsmtpmessage msg;
xsmtpresult ret;
xsmtpaddr to;

xrtSmtpConfigInit(&cfg);
cfg.sHost = "smtp.example.com";
cfg.iPort = 587;
cfg.iSecureMode = XSMTP_SECURE_STARTTLS;
cfg.bAuth = TRUE;
cfg.iAuthMode = XSMTP_AUTH_AUTO;
cfg.bVerifyPeer = TRUE;
cfg.sUser = "user@example.com";
cfg.sPass = "app-password";
cfg.tFrom.sEmail = "user@example.com";
cfg.tFrom.sName = "Example Sender";

memset(&to, 0, sizeof(to));
to.sEmail = "receiver@example.com";
to.sName = "Receiver";

xrtSmtpMessageInit(&msg);
msg.arrTo = &to;
msg.iToCount = 1;
msg.sSubject = "Hello from xsmtp";
msg.sTextBody = "Plain text body";
msg.sHtmlBody = "<p>HTML body</p>";

xrtSmtpResultInit(&ret);
if ( !xrtSmtpSendMail(&cfg, &msg, &ret) ) {
	printf("send failed: %s\n", ret.sError);
}
```

## Minimal Async Example

```c
xsmtpasyncopts opts;
xsmtpresult ret;

xrtSmtpAsyncOptsInit(&opts);
opts.sDebugName = "mail_send_job";

xrtSmtpResultInit(&ret);
if ( !xrtSmtpSendMailAsyncWait(&cfg, &msg, &opts, &ret) ) {
	printf("async send failed: %s\n", ret.sError);
}
```

## Future API Usage

`xrtSmtpSendMailFuture(...)` returns an `xfuture*`.

Notes:

- if `xsmtpasyncopts.pEngine == NULL`, `xsmtp` creates a private engine
- if `xsmtpasyncopts.pEngine != NULL`, `xsmtp` reuses the caller's engine
- the future resolves to `xsmtpresult*`
- release that value with `xrtSmtpResultFree(...)`

Typical flow:

```c
xfuture* fut = xrtSmtpSendMailFuture(&cfg, &msg, &opts);
xsmtpresult* ret = (xsmtpresult*)xFutureWaitValue(fut);
if ( ret ) {
	/* use result */
	xrtSmtpResultFree(ret);
}
xFutureRelease(fut);
```

## Coroutine API Usage

`xrtSmtpSendMailCo(...)` is a convenience wrapper around the async send path and
waits in coroutine style.

Use it when the surrounding system already uses `xrt` coroutine scheduling.

## Protocol Behavior

Typical send flow:

1. connect
2. read banner
3. send `EHLO`
4. optionally perform `STARTTLS`
5. send `EHLO` again after TLS upgrade
6. authenticate
7. `MAIL FROM`
8. `RCPT TO`
9. `DATA`
10. send message body
11. `QUIT`

## TLS Notes

### Implicit TLS (`465`)

TLS is attached during connect.

### STARTTLS (`587`)

The library:

1. connects in plain mode
2. reads server capabilities
3. sends `STARTTLS`
4. performs the TLS handshake
5. re-sends `EHLO`
6. continues auth and delivery inside TLS

Both sync and native async paths are currently supported.

## Message Body Rules

`xsmtp` automatically handles:

- UTF-8 subject encoding
- UTF-8 mailbox display names
- MIME headers
- plain text body encoding
- HTML body encoding
- multipart/alternative generation
- SMTP dot-stuffing in `DATA`

## Error Handling

Always inspect:

- `ret.sError`
- `ret.sLastReply`
- `ret.iServerCode`

Common examples:

- connection failed
- TLS verify failed
- server does not support `STARTTLS`
- auth failed
- recipient rejected
- unexpected SMTP reply code

## Security Recommendations

- prefer provider app passwords over account master passwords
- keep `bVerifyPeer = TRUE` in production
- only disable peer verification for controlled local debugging
- do not hardcode real secrets in committed source files
- redact passwords and auth tokens from logs

## Debug Logging

Build with `-DXSMTP_DEBUG` to enable trace logs.

The current log set focuses on:

- connection submission
- engine fallback
- stream error / close reasons
- sync protocol milestones

It avoids the verbose bring-up tracing that was used during initial protocol
debugging.

## Validation Status

The local example has been verified with real delivery for:

- `465` + implicit TLS
- `587` + STARTTLS
- sync send
- async wait send

## Suggested Next Integration

For upper-layer systems such as `xserver` or `xadmin`, the recommended next
step is:

1. import the `xsmtp` API into the host runtime
2. call `xrtSmtpSendMailFuture(...)` from queue workers
3. reuse a shared `xnetengine` when batching mail tasks

## Integrating Into xserver

If `xsmtp` is hosted by `xserver`, the usual pattern is:

1. place `xsmtp.h` under the host-side library directory
2. add an import bridge in the script host
3. expose only the symbols the script layer actually needs

Typical import file:

- [import_xsmtp.h](D:/Git/xserver/src/script/import_c/import_xsmtp.h)

Example import pattern:

```c
static inline void ImportXSMTP(TCCState* s)
{
	if ( s == NULL ) {
		return;
	}

	tcc_add_symbol(s, "xrtSmtpConfigInit", xrtSmtpConfigInit);
	tcc_add_symbol(s, "xrtSmtpMessageInit", xrtSmtpMessageInit);
	tcc_add_symbol(s, "xrtSmtpResultInit", xrtSmtpResultInit);
	tcc_add_symbol(s, "xrtSmtpAsyncOptsInit", xrtSmtpAsyncOptsInit);
	tcc_add_symbol(s, "xrtSmtpSendMail", xrtSmtpSendMail);
	tcc_add_symbol(s, "xrtSmtpSendMailFuture", xrtSmtpSendMailFuture);
	tcc_add_symbol(s, "xrtSmtpSendMailAsyncWait", xrtSmtpSendMailAsyncWait);
	tcc_add_symbol(s, "xrtSmtpSendMailCo", xrtSmtpSendMailCo);
	tcc_add_symbol(s, "xrtSmtpResultFree", xrtSmtpResultFree);
}
```

Recommended export policy:

- always export the three init helpers
- export `xrtSmtpSendMail(...)` for the simplest script usage
- export `xrtSmtpSendMailAsyncWait(...)` if script jobs need a blocking async wrapper
- export `xrtSmtpSendMailFuture(...)` only when the script layer already manages futures
- export `xrtSmtpResultFree(...)` whenever future results are returned to script code

## Integrating Into xadmin

For `xadmin`, `xsmtp` is best used as a queue executor instead of inline page
request logic.

Recommended execution flow:

1. the admin page creates a `mail_task`
2. the queue scanner loads `pending` tasks
3. each task is converted into `xsmtpconfig + xsmtpmessage`
4. the worker sends mail
5. the task row is updated to `success` or `fail`
6. a `mail_log` row is written with the provider reply and error text

This keeps page interactions short and makes retry logic much easier to manage.

Suggested field mapping:

- SMTP host, port, secure mode, auth mode, timeout:
  from global option storage
- `cfg.tFrom` / `cfg.tReplyTo`:
  from the configured sender identity
- `msg.arrTo`:
  from the resolved recipient list
- `msg.sSubject`, `msg.sTextBody`, `msg.sHtmlBody`:
  from the rendered task payload or template output

Suggested queue policy:

- default to async send when the queue runner already owns an `xnetengine`
- use sync send only for simple diagnostic tools or one-off admin actions
- limit batch size per scan round
- keep retry count and next retry time in the task table
- persist the last SMTP reply for later troubleshooting

## Queue Worker Example

Minimal worker-shaped example:

```c
xsmtpconfig cfg;
xsmtpmessage msg;
xsmtpresult ret;
xsmtpasyncopts opts;
xsmtpaddr to;

xrtSmtpConfigInit(&cfg);
xrtSmtpMessageInit(&msg);
xrtSmtpResultInit(&ret);
xrtSmtpAsyncOptsInit(&opts);

cfg.sHost = smtp_host;
cfg.iPort = (uint16)smtp_port;
cfg.iSecureMode = smtp_secure_mode;
cfg.bAuth = TRUE;
cfg.iAuthMode = XSMTP_AUTH_AUTO;
cfg.bVerifyPeer = TRUE;
cfg.sUser = smtp_user;
cfg.sPass = smtp_pass;
cfg.tFrom.sEmail = from_email;
cfg.tFrom.sName = from_name;

memset(&to, 0, sizeof(to));
to.sEmail = task_to_email;
to.sName = task_to_name;

msg.arrTo = &to;
msg.iToCount = 1;
msg.sSubject = task_subject;
msg.sTextBody = task_text_body;
msg.sHtmlBody = task_html_body;

opts.pEngine = shared_engine;
opts.sDebugName = "mail_queue";

if ( !xrtSmtpSendMailAsyncWait(&cfg, &msg, &opts, &ret) ) {
	/* mark task failed and persist ret.sError / ret.sLastReply */
} else {
	/* mark task success and persist ret.sLastReply */
}
```

## Common Integration Decisions

### Sync vs Async

Use sync send when:

- writing a tiny standalone tool
- validating the library locally
- implementing a one-off SMTP diagnostics page

Use async send when:

- processing a task queue
- sending in batches
- reusing an existing `xnetengine`
- running inside a service process with other async infrastructure

### Shared Engine vs Private Engine

Use a shared engine when:

- multiple mail tasks are sent in the same worker loop
- the host already owns a long-lived network engine

Use a private engine when:

- sending only one mail
- writing test tools
- isolating SMTP from the rest of the runtime is more important than reuse

## Troubleshooting

### `SMTP socket connect failed`

Usually means TCP connect failed before SMTP auth began.

Check:

- host and port are correct
- the current network can reach the provider
- the chosen mode matches the port:
  - `465` -> `XSMTP_SECURE_SSL`
  - `587` -> `XSMTP_SECURE_STARTTLS`
- local firewall or outbound policy is not blocking the port

### TLS verify failed

Usually means the server certificate does not match the configured host or the
certificate chain cannot be validated.

Check:

- `cfg.sHost` matches the actual SMTP certificate name
- the local trust store is available
- `bVerifyPeer` is not being disabled accidentally in production

### Auth failed

Usually means username, password, or auth mode is wrong.

Check:

- provider app password vs account password
- whether SMTP auth is enabled for the account
- whether `XSMTP_AUTH_AUTO` negotiated the expected method

### `STARTTLS` not supported

Usually means the server did not advertise `STARTTLS` in the `EHLO` reply, or
the connection is hitting the wrong port.

Check:

- port `587`
- secure mode is `XSMTP_SECURE_STARTTLS`
- upstream proxy is not altering the SMTP session

## Production Notes

- Keep example credentials out of source control.
- Prefer queue-based delivery over inline request delivery.
- Retain SMTP reply text in logs for support and postmortem analysis.
- Start with one provider and one sender identity before adding multi-provider routing.
- Add a small `test SMTP config` tool in the admin UI before enabling automated queue delivery.
