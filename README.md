# cn-proxy-server

Multi-threaded HTTP/HTTPS forward proxy written in C (POSIX sockets, pthreads).
Computer Networks mini project (Project 1: Designing a Proxy Server).

## Team and module ownership

| Member | Module | Files |
|---|---|---|
| Akash | Core engine: listener, HTTP parsing and forwarding, CONNECT tunnel, robustness | `src/main.c`, `src/proxy.c`, `src/http.c`, `src/tunnel.c` |
| Sania | Response cache (LRU, TTL, thread-safe) | `src/cache.c`, `src/cache.h` |
| Tarunima | Access control (blocklist, rate limiting) and config | `src/access.c`, `src/access.h` |
| Anik | Logging, statistics, benchmarks | `src/logger.c`, `src/stats.c`, `benchmarks/` |

## Build

Requires Linux or WSL with `gcc` and `make` (`sudo apt install build-essential`).

    make            # builds ./proxy with AddressSanitizer + UBSan
    make tsan       # builds ./proxy_tsan with ThreadSanitizer
    make clean

## Run

    ./proxy 8888                          # listens on port 8888 (default 8888)
    ./proxy 8888 --block example.com      # same, with a blocked domain (repeat --block for more)

Point a client at it:

    curl -x http://localhost:8888 http://example.com/ -I
    curl -x http://localhost:8888 https://example.com -I

Or set the browser's HTTP and HTTPS proxy to `localhost` port `8888`.

## What it does

- Handles many clients at once (one thread per connection, capped at 200; extra clients get `503`).
- Forwards HTTP requests (GET, HEAD, POST, PUT) with correct header handling.
- Supports HTTPS through `CONNECT` tunnelling (traffic stays encrypted end to end).
- Blocks configured domains with `403`.
- Replies with proper errors: `400` malformed, `431` oversized headers, `413` oversized body,
  `501` unsupported, `502` unreachable origin, `504` origin timeout, `503` overloaded.
- Logs one line per request: client, method, URL, status, bytes, cache status, time.

## Tests

    ./proxy 8888                      # terminal 1
    ./tests/echo_server               # terminal 2 (build: gcc -o tests/echo_server tests/post_echo_server.c)
    ./tests/robustness.sh             # terminal 3

`robustness.sh` checks malformed requests, oversized headers, unreachable hosts,
50 simultaneous clients and a hung origin.

## Git workflow

Each member works on their own branch (`feature/core`, `feature/cache`, `feature/access`,
`feature/logging`) and merges into `main` through pull requests.
