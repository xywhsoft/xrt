# WebSocket physical retirement

```sh
python tools/build.py --suite websocket_stream_retire --exclude-test '*' --no-single --cflag=-O2
```

This public-API-only loopback fixture directly attaches two already connected TCP
streams; it does not perform an HTTP Upgrade and is not a deployment recipe.
It aborts a connection, observes that CLOSED cannot bypass a held native TCP
alias, releases that alias, waits for both physical barriers, then destroys the
stream owners. Owned application Data is released exactly once per endpoint.
The host Engine joins separately before stack callbacks/Data leave scope.
