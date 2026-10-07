#!/bin/bash
# Robustness tests. Needs: proxy on 8888 (terminal A) and ./tests/echo_server on 9001 (terminal B).
PROXY=localhost:8888
pass=0; fail=0

check() {   # name expected actual
    if [ "$2" = "$3" ]; then echo "PASS  $1"; pass=$((pass+1))
    else echo "FAIL  $1 (expected $2, got $3)"; fail=$((fail+1)); fi
}
code() { curl -s -o /dev/null -w "%{http_code}" -x "http://$PROXY" "$@"; }

echo "== malformed request =="
check "garbage request -> 400" 400 "$(printf 'garbage\r\n\r\n' | nc -w 2 localhost 8888 | head -1 | cut -d' ' -f2)"

echo "== oversized headers =="
check "9 KB header -> 431" 431 "$(code -H "X-Big: $(head -c 9000 /dev/zero | tr '\0' a)" http://localhost:9001/)"

echo "== unreachable host =="
check "unknown domain -> 502" 502 "$(code http://thisdoesnotexist12345.invalid/)"
check "closed port -> 502" 502 "$(code http://localhost:9999/)"

echo "== 50 simultaneous clients =="
ok=$(seq 50 | xargs -P 50 -I{} curl -s -o /dev/null -w "%{http_code}\n" -x "http://$PROXY" -d x http://localhost:9001/ | grep -c 200)
check "50 parallel POSTs all succeed" 50 "$ok"

echo "== slow origin (takes ~10 s) =="
nc -l 9002 > /dev/null 2>&1 &
NCPID=$!
sleep 1
check "origin never replies -> 504" 504 "$(code http://localhost:9002/)"
kill $NCPID 2>/dev/null

echo "== proxy still alive after all of the above =="
check "normal request still works" 200 "$(code http://localhost:9001/)"

echo
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
