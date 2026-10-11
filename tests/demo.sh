#!/bin/bash
# Live demo. Start these first:
#   terminal 1: ./proxy 8888 --config config/proxy.conf   (blocks blocked.example, rate 20/s burst 40)
#   terminal 2: cd ~ && python3 -m http.server 9000     (plain test web server, only a target)
PROXY=http://localhost:8888
step() { echo; echo "=== $1 ==="; }
fetch() { curl -s -o /dev/null -x "$PROXY" -w "  status=%{http_code}  time=%{time_total}s\n" "$@"; }

step "1. Normal request forwarding (HTTP)"
fetch http://localhost:9000/

step "2. Cache: same URL twice (watch the proxy log for MISS, then HIT)"
U2="http://localhost:9000/?demo=$(date +%s)"
fetch "$U2"
fetch "$U2"

step "3. Access control: blocked domain"
fetch http://blocked.example/

step "4. HTTPS through a CONNECT tunnel"
curl -s -I -x "$PROXY" https://example.com | head -1

step "5. Error handling"
echo -n "  unreachable host:  "; fetch http://thisdoesnotexist12345.invalid/
echo -n "  malformed request: "; printf 'garbage\r\n\r\n' | nc -w 2 localhost 8888 | head -1

step "6. Concurrency: 30 simultaneous requests"
ok=$(seq 30 | xargs -P 30 -I{} curl -s -o /dev/null -w "%{http_code}\n" -x "$PROXY" http://localhost:9000/ | grep -c 200)
echo "  $ok of 30 succeeded"

step "7. Rate limiting: 80 rapid requests from one client (burst 40, refill 20/s)"
sleep 3   # let the token bucket refill after the earlier steps
codes=$(for i in $(seq 80); do curl -s -o /dev/null -w "%{http_code}\n" -x "$PROXY" http://localhost:9000/; done)
echo "  200 OK:                $(echo "$codes" | grep -c 200)"
echo "  429 Too Many Requests: $(echo "$codes" | grep -c 429)"
