#!/bin/sh
# Stress / hardening harness for appinstalld2. Runs ON DEVICE (LuneOS/webOS).
#
# Exercises the com.webos.appInstallService LS2 API with valid, malformed and
# hostile payloads, churns subscriptions, runs concurrent install/remove
# cycles of a generated dummy ipk, and watches the daemon for crashes,
# restarts and memory growth.
#
# Usage: stress-appinstalld.sh [-n ITERATIONS] [-c CONCURRENCY] [-i IPK]
#   -n  install/remove cycles (default 10)
#   -c  parallel malformed-payload workers (default 4)
#   -i  path to an ipk to use; a dummy one is generated if omitted

set -u

ITER=10
CONC=4
IPK=""
while getopts "n:c:i:" opt; do
    case $opt in
        n) ITER="$OPTARG" ;;
        c) CONC="$OPTARG" ;;
        i) IPK="$OPTARG" ;;
        *) echo "usage: $0 [-n iter] [-c conc] [-i ipk]"; exit 2 ;;
    esac
done

SRV=com.webos.appInstallService
LUNA="luna-send"
WORK=$(mktemp -d /tmp/appinstalld-stress.XXXXXX)
FAILURES=0
TESTED=0

log()  { echo "[stress] $*"; }
fail() { FAILURES=$((FAILURES+1)); echo "[stress] FAIL: $*"; }

daemon_pid() { pidof appinstalld; }

check_alive() {
    # daemon must be running and answering
    if ! daemon_pid >/dev/null; then
        fail "appinstalld not running after: $1"
        # give systemd a chance to restart it before continuing
        sleep 3
        return 1
    fi
    OUT=$($LUNA -n 1 luna://$SRV/status '{}' 2>&1)
    case "$OUT" in
        *'"returnValue":true'*) return 0 ;;
        *) fail "status call failed after: $1 -> $OUT"; return 1 ;;
    esac
}

rss_kb() {
    P=$(daemon_pid) || return 1
    awk '/VmRSS/ {print $2}' "/proc/$P/status" 2>/dev/null
}

call() {
    # call <label> <method> <payload> [expected-substring]
    TESTED=$((TESTED+1))
    LABEL=$1; METHOD=$2; PAYLOAD=$3; EXPECT=${4:-}
    OUT=$($LUNA -n 1 "luna://$SRV/$METHOD" "$PAYLOAD" 2>&1)
    if [ -n "$EXPECT" ]; then
        case "$OUT" in
            *"$EXPECT"*) : ;;
            *) fail "$LABEL: expected '$EXPECT' in reply, got: $OUT" ;;
        esac
    fi
    check_alive "$LABEL" >/dev/null
}

make_dummy_ipk() {
    # A minimal, well-formed ipk (ar archive: debian-binary, control, data)
    ID=$1
    D="$WORK/pkg"
    rm -rf "$D"; mkdir -p "$D/control" "$D/data/usr/palm/applications/$ID"
    printf '2.0\n' > "$D/debian-binary"
    cat > "$D/control/control" <<EOF
Package: $ID
Version: 1.0.0
Section: misc
Priority: optional
Architecture: all
Description: appinstalld stress dummy
EOF
    cat > "$D/data/usr/palm/applications/$ID/appinfo.json" <<EOF
{"id":"$ID","version":"1.0.0","vendor":"stress","type":"web","main":"index.html","title":"Stress Dummy"}
EOF
    echo '<html><body>stress</body></html>' > "$D/data/usr/palm/applications/$ID/index.html"
    ( cd "$D/control" && tar czf ../control.tar.gz ./control )
    ( cd "$D/data" && tar czf ../data.tar.gz . )
    ( cd "$D" && ar rc "$WORK/$ID.ipk" debian-binary control.tar.gz data.tar.gz )
    echo "$WORK/$ID.ipk"
}

log "=== appinstalld2 stress harness starting (iter=$ITER conc=$CONC) ==="
check_alive "startup" || { echo "[stress] daemon not healthy at start, aborting"; exit 1; }
RSS_START=$(rss_kb)
log "initial RSS: ${RSS_START} kB"

# --- 1. malformed / hostile payload battery -------------------------------
log "--- malformed payload battery ---"
call "empty install"        install '{}'                                        '"returnValue":false'
call "bad json"             install '{"id":'                                    'returnValue'
call "empty id"             install '{"id":"","ipkUrl":"/tmp/x.ipk"}'           '"returnValue":false'
call "empty ipkUrl"         install '{"id":"com.test.x","ipkUrl":""}'           '"returnValue":false'
call "nonexistent ipk"      install '{"id":"com.test.x","ipkUrl":"/tmp/definitely-missing.ipk"}' '"returnValue":false'
call "traversal id"         install '{"id":"../../../etc/passwd","ipkUrl":"/tmp/x.ipk"}' '"returnValue":false'
call "traversal id 2"       install '{"id":"com.test/../../evil","ipkUrl":"/tmp/x.ipk"}' '"returnValue":false'
call "quote-inject id"      remove  '{"id":"a\",\"evil\":\"1"}'                 '"returnValue":false'
call "shell-meta id"        remove  '{"id":"x;reboot;"}'                        '"returnValue":false'
call "huge id"              install "{\"id\":\"$(awk 'BEGIN{s="";for(i=0;i<4096;i++)s=s"a";print s}')\",\"ipkUrl\":\"/tmp/x.ipk\"}" '"returnValue":false'
call "wrong types"          install '{"id":123,"ipkUrl":true}'                  '"returnValue":false'
call "not-an-ipk suffix"    install '{"id":"com.test.x","ipkUrl":"/etc/passwd"}' '"returnValue":false'
call "empty remove"         remove  '{}'                                        '"returnValue":false'
call "status ok"            status  '{}'                                        '"returnValue":true'

# --- 2. subscription churn -------------------------------------------------
log "--- subscription churn ---"
i=0
while [ $i -lt 25 ]; do
    $LUNA -n 2 -t 1 luna://$SRV/status '{"subscribe":true}' >/dev/null 2>&1 &
    i=$((i+1))
done
wait
check_alive "subscription churn"

# --- 3. concurrent malformed-payload workers ------------------------------
log "--- concurrent hostile workers ---"
w=0
while [ $w -lt "$CONC" ]; do
    (
        j=0
        while [ $j -lt 20 ]; do
            $LUNA -n 1 luna://$SRV/install '{"id":"","ipkUrl":""}' >/dev/null 2>&1
            $LUNA -n 1 luna://$SRV/remove  '{"id":"no.such.app.'$w'.'$j'"}' >/dev/null 2>&1
            $LUNA -n 1 luna://$SRV/status  '{}' >/dev/null 2>&1
            j=$((j+1))
        done
    ) &
    w=$((w+1))
done
wait
check_alive "concurrent workers"

# --- 4. real install/remove cycles ----------------------------------------
APPID="org.webosports.stressdummy"
if [ -z "$IPK" ]; then
    if command -v ar >/dev/null 2>&1; then
        IPK=$(make_dummy_ipk $APPID)
        log "generated dummy ipk: $IPK"
    else
        log "no 'ar' on device and no -i given: skipping real install cycles"
        IPK=""
    fi
fi
if [ -n "$IPK" ]; then
    log "--- $ITER install/remove cycles of $APPID ---"
    n=0
    while [ $n -lt "$ITER" ]; do
        $LUNA -n 1 luna://$SRV/install "{\"id\":\"$APPID\",\"ipkUrl\":\"$IPK\"}" >/dev/null 2>&1
        # poll until install settles (max ~15s)
        t=0
        while [ $t -lt 15 ]; do
            S=$($LUNA -n 1 luna://$SRV/status '{}' 2>/dev/null)
            case "$S" in *"$APPID"*) sleep 1; t=$((t+1)) ;; *) break ;; esac
        done
        $LUNA -n 1 luna://$SRV/remove "{\"id\":\"$APPID\"}" >/dev/null 2>&1
        sleep 2
        check_alive "install/remove cycle $n" || break
        n=$((n+1))
    done
    # concurrent duplicate installs of the same id must not crash or wedge
    log "--- duplicate concurrent installs ---"
    $LUNA -n 1 luna://$SRV/install "{\"id\":\"$APPID\",\"ipkUrl\":\"$IPK\"}" >/dev/null 2>&1 &
    $LUNA -n 1 luna://$SRV/install "{\"id\":\"$APPID\",\"ipkUrl\":\"$IPK\"}" >/dev/null 2>&1 &
    wait
    sleep 15
    $LUNA -n 1 luna://$SRV/remove "{\"id\":\"$APPID\"}" >/dev/null 2>&1
    sleep 2
    check_alive "duplicate installs"
fi

# --- 5. results ------------------------------------------------------------
RSS_END=$(rss_kb)
log "final RSS: ${RSS_END} kB (start: ${RSS_START} kB)"
if [ -n "$RSS_START" ] && [ -n "$RSS_END" ] && [ "$RSS_END" -gt $((RSS_START * 2)) ] && [ $((RSS_END - RSS_START)) -gt 20000 ]; then
    fail "RSS more than doubled (+$((RSS_END - RSS_START)) kB): possible leak"
fi
rm -rf "$WORK"
log "=== done: $TESTED checks, $FAILURES failures ==="
[ "$FAILURES" -eq 0 ]
