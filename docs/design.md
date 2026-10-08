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

## 9. Logging, statistics and performance evaluation (Anik)

### 9.1 Log format

Every finished request produces one line, written to the terminal and appended to `logs/proxy.log`:

    [2026-10-08 10:22:45] 127.0.0.1 GET http://localhost:9000/ 200 2768 MISS 4.5ms

The fields are: timestamp, client IP, method, URL, status code, bytes sent to the client,
cache status (`HIT`, `MISS`, `TUNNEL`, `BLOCKED` or `-`), and time taken. The layout is close to
an Apache access log, so a network administrator can read it or process it with `grep`/`awk`.

Design choices in `logger.c`:
- **Mutex.** The proxy runs one thread per client, so many threads call `log_request` at once.
  A mutex around the write ensures each line is written whole. Without it, text from two threads
  can interleave inside one line.
- **`localtime_r`.** The plain `localtime` returns a pointer to a shared static buffer, which is a
  data race between threads. `localtime_r` writes into a caller-supplied struct instead.
- **`fflush` after each line.** The log is on disk even if the proxy crashes. The cost is one
  small write per request, which is acceptable at this scale.
- **Lazy open.** The log file is opened once on the first call, inside the mutex, so two threads
  cannot both open it. If the file cannot be opened, logging continues on the terminal only.

### 9.2 Statistics

`stats.c` keeps counters for total requests, cache hits, cache misses, blocked requests, HTTPS
tunnels, error responses (status 400 or above, excluding blocked) and bytes served. `log_request`
calls `stats_record` after writing each line, so all counters are fed from one place and no other
module needed changes. All counters sit behind one mutex: without it, two threads doing
`total++` at the same time can lose updates (read, add and write can interleave). A summary,
including the hit ratio (hits / (hits + misses)), is printed when the proxy shuts down.

Example from a test run with one blocked domain, one unreachable host and one HTTPS tunnel:

    Total requests : 5
    Cache hits     : 0
    Cache misses   : 2
    Blocked        : 1
    HTTPS tunnels  : 1
    Errors (>=400) : 1
    Bytes served   : 11141

### 9.3 Benchmark method

Scripts are in `benchmarks/`:
- `latency.sh`: average of 100 sequential requests, measured with `curl -w "%{time_total}"`, once
  directly and once through the proxy.
- `throughput.sh`: `ab -n 2000 -c 50` (ApacheBench, 2000 requests, 50 concurrent), once directly and
  once through the proxy (`-X localhost:8888`).

To keep the comparison fair:
- The proxy is built with `make release` (`-O2`, no sanitizers). The normal build uses
  AddressSanitizer and UBSan, which slow the program down and would distort the numbers.
- The origin is the small C server `tests/echo_server`, running locally, so internet latency and
  DNS do not affect the results.
- All tests use `127.0.0.1` rather than `localhost` (see 9.5).

### 9.4 Results (cold cache, localhost, tiny response)

| Metric | Direct | Via proxy |
|---|---|---|
| Average latency (100 sequential requests) | 0.45 ms | 1.09 ms |
| Throughput (2000 requests, 50 concurrent) | 11,328 req/s | 5,622 req/s |
| Mean time per request (50 concurrent) | 4.4 ms | 8.9 ms |
| Failed requests | 0 | 0 |

Warm-cache results: (to be added after the cache module is merged)

The proxy adds about 0.6 ms per request and reaches roughly half the direct throughput, with no
failed requests under 50 concurrent clients.

### 9.5 Measurement lessons

Two problems produced misleading numbers before the final results:
1. **Python's `http.server` as origin.** Its listen backlog is only 5, so under 50 concurrent
   clients extra connections were dropped and retried about a second later. Direct throughput
   dropped to about 36-72 req/s and the proxy run showed random failed requests. The benchmark was
   measuring the test server, not the proxy. Switching to the C echo server (backlog 128) fixed it.
2. **`localhost` and IPv6.** `localhost` can resolve to the IPv6 address `::1` first. If the server
   only listens on IPv4, each connection fails once before falling back, adding a fixed delay.
   Using `127.0.0.1` avoids this.

The lesson is that a result that does not change when the setup changes (here, 72 req/s both
times) usually means something other than the thing under test is the bottleneck.

### 9.6 Discussion

The proxy is slower than a direct connection on localhost because each request does extra work:
a new TCP connection to the origin (we send `Connection: close`), a DNS lookup, header rewriting,
a new thread, and logging and statistics under mutexes. On localhost the origin answers almost
instantly, so this fixed cost is a large share of the total. With a real internet origin, where
the origin round trip takes tens or hundreds of milliseconds, the same overhead is a small
fraction of the total.

Ways to reduce it: serving repeated requests from the cache (removes the origin trip entirely),
reusing connections to origins (keep-alive), a thread pool instead of a thread per request, or an
`epoll`-based event loop.
