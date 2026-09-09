# Memory SQLite Policy

`xllm_memory` uses SQLite as the local durable store when `xllm_memory_options.sSqlitePath` is set.

Default policy for file-backed databases:

- Open with `SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX`.
- Set `sqlite3_busy_timeout` from `xllm_memory_options.uSqliteBusyTimeoutMs`; default is `5000`.
- Enable `PRAGMA foreign_keys = ON` on every connection.
- Enable `PRAGMA journal_mode = WAL` unless `bDisableSqliteWal` is set.
- Use `PRAGMA synchronous = NORMAL` when WAL is enabled.
- Persist record/chunk/index writes with `BEGIN IMMEDIATE TRANSACTION`, followed by `COMMIT` or `ROLLBACK`.
- Crash-consistency behavior is documented in `docs/CRASH_CONSISTENCY.md` and covered by `memory_crash_consistency`.
- Profile migration and rollback policy is documented in `docs/MEMORY_PROFILE_MIGRATION.md`.

Rationale:

- WAL allows IDE/search readers to continue while a writer commits memory updates.
- The busy timeout avoids immediate failures during short writer contention.
- `BEGIN IMMEDIATE` makes write lock acquisition explicit before deleting and replacing record rows.
- `synchronous=NORMAL` is the pragmatic WAL default for local agent memory; it favors throughput while keeping committed transactions consistent under normal process crashes.

Scope:

- This policy applies to local SQLite-backed memory.
- In-memory SQLite paths do not request WAL.
- Encryption at rest is not implemented here; host products should provide filesystem or platform encryption if required.
