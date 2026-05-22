# Coroutine Archive

This directory is reserved for coroutine transitional artifacts that are removed
from mainline during the foundation finalization phase.

Current archive policy for coroutine fallback code:

- production backends are the only mainline path
- historical fiber/ucontext fallback code has been removed from mainline
- unsupported targets now fail hard instead of silently selecting an archive
  backend inside `lib/coroutine.h`
- any future archival copies of removed fallback code must live only under
  `dev/archive/coroutine/`
