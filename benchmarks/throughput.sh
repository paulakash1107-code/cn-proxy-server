#!/bin/bash
# Throughput and concurrency test with ApacheBench: direct vs. through the proxy.
# Needs: ./proxy_release 8888 running, and a web server on port 9000.
REQS=${1:-2000}
CONC=${2:-50}
URL=http://localhost:9000/

echo "== Direct ($REQS requests, $CONC concurrent) =="
ab -n "$REQS" -c "$CONC" "$URL" 2>/dev/null | grep -E "Requests per second|Time per request.*mean\)|Failed requests"

echo
echo "== Through proxy ($REQS requests, $CONC concurrent) =="
ab -n "$REQS" -c "$CONC" -X localhost:8888 "$URL" 2>/dev/null | grep -E "Requests per second|Time per request.*mean\)|Failed requests"
