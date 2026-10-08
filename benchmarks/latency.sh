#!/bin/bash
# Average latency of N sequential requests: direct vs. through the proxy.
# Needs: ./proxy_release 8888 running, and a web server on port 9000.
N=${1:-100}
URL=${2:-http://localhost:9000/}
PROXY=http://localhost:8888

measure() {   # prints the average time_total in milliseconds
    for i in $(seq "$N"); do
        curl -s -o /dev/null -w "%{time_total}\n" "$@"
    done | awk '{s+=$1} END {printf "%.2f", s/NR*1000}'
}

echo "URL: $URL   requests: $N"
echo "Direct:        $(measure "$URL") ms"
echo "Via proxy:     $(measure -x "$PROXY" "$URL") ms"
