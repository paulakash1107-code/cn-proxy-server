# Design Document: cn-proxy-server

## 1. Overview (Akash)

A forward proxy that sits between clients and the internet. A client configures the proxy
address; for plain HTTP it sends the full URL in the request line, and for HTTPS it sends
`CONNECT host:port`. Request flow:

    client -> accept -> parse request -> access check -> cache lookup -> connect to origin
           -> forward request (+ body) -> relay response (copy kept for cache) -> log

Modules: core engine (`main.c`, `proxy.c`, `http.c`, `tunnel.c`), cache, access control, logger.
They talk through small C interfaces (`cache.h`, `access.h`, `logger.h`), so each module could be
developed and tested independently using stub implementations.

## 2. Concurrency model (Akash)

One detached POSIX thread per client connection. The main thread only runs `accept()`.

Why threads: each connection blocks on network I/O, and a thread per connection keeps the
code sequential and easy to reason about. A slow client or origin only blocks its own thread.

Safeguards:
- At most 200 simultaneous connections (guarded by a mutex-protected counter). Beyond that
  the proxy answers `503` instead of exhausting memory.
- `SO_RCVTIMEO` / `SO_SNDTIMEO` of 10 s on client and origin sockets, so no thread hangs forever.
- `SIGPIPE` is ignored and `MSG_NOSIGNAL` used, so a client that disconnects mid-response
  cannot kill the process.
- `SIGINT`/`SIGTERM` stop the accept loop and let the process exit cleanly.

Trade-off: threads cost memory (stack per connection) and do not scale to tens of thousands of
connections. An event loop (`epoll`) would scale better but makes the code much more complex.
For this project's scale, threads are the simpler and more defensible choice.

## 3. HTTP handling (Akash)

- Headers are read until the blank line (`\r\n\r\n`) into an 8 KB buffer; larger headers get `431`.
- The proxy rewrites the request for the origin: absolute URL becomes a relative path,
  hop-by-hop headers (`Connection`, `Proxy-Connection`, `Keep-Alive`, `TE`, `Upgrade`,
  `Proxy-Authorization`) are removed, `Via` and `X-Forwarded-For` are added.
- It sends `Connection: close` and `Accept-Encoding: identity` to the origin. Closing the
  connection marks the end of the response, which avoids parsing chunked encoding, and
  uncompressed bodies keep the cache simple. The cost is no connection reuse to the origin.
- Request bodies (POST/PUT) are forwarded using `Content-Length`, up to 10 MB (`413` above).
  Chunked request bodies are not supported (`501`).
- `send()` and `recv()` can transfer partial data, so all writes go through a loop (`send_all`).

## 4. HTTPS via CONNECT (Akash)

For `CONNECT host:443` the proxy opens a TCP connection to the host, answers
`200 Connection Established`, then relays bytes in both directions with `select()` until either
side closes or 30 s pass with no traffic.

HTTPS is end-to-end encrypted, so the proxy cannot read or cache it without
man-in-the-middle certificates, which we deliberately do not do. For tunnels the proxy can still
apply access control by hostname and log the host, bytes and duration.

## 5. Robustness (Akash)

Verified by `tests/robustness.sh`: malformed requests (`400`), oversized headers (`431`),
unknown host and closed port (`502`), 50 simultaneous clients (all succeed), and an origin that
accepts the connection but never replies (`504` after 10 s). The proxy keeps serving normally
after all of these. The suite passes 7 of 7 and runs without errors under AddressSanitizer,
UBSan (no leaks at exit) and ThreadSanitizer (no data races reported).

## 6. Known limitations (Akash)

- No `Transfer-Encoding: chunked` request bodies, no HTTP/2, no WebSocket upgrade.
- No persistent connections to origins (each request opens a new connection).
- HTTPS traffic cannot be cached or inspected.
- Thread-per-connection does not scale like an event loop.

## 7. Caching (Sania)

(to be written by Sania: policy, eviction, TTL rules, trade-offs)

## 8. Access control (Tarunima)

(to be written by Tarunima: blocklist, rate limiter, config format, trade-offs)

## 9. Logging and performance evaluation (Anik)

(to be written by Anik: log format, benchmark method, results with and without the proxy)
