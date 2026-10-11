#!/bin/bash
# Trace parity: C++ build/s3trace vs the simref Rust `trace` binary (local only, never published).
#   tools/parity.sh            the five simref scripts on KenneyPark (+ hex compare)
#   tools/parity.sh --extra    also tools/parity_scripts/*.txt on KenneyPark and test-world (2400 ticks)
# Needs ~/Documents/haloskate-ref/{simref/target/release/trace,cleanpack/{root,core.pack,KenneyPark.skate}}.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
REF=${SIMREF:-$HOME/Documents/haloskate-ref/simref}
PACK=${CLEANPACK:-$HOME/Documents/haloskate-ref/cleanpack}
OUT=${PARITY_OUT:-$HERE/build/parity}
mkdir -p "$OUT"
fail=0
run() { # map script ticks [args]
  local m=$1 s=$2 k=$3; shift 3
  local n; n=$(basename "$s" .txt)_$m; local cm=$PACK/KenneyPark.skate
  [ "$m" = test-world ] && cm=test-world
  "$REF/target/release/trace" "$PACK/root" "$m" "$s" "$k" "$@" > "$OUT/$n.rust.csv" 2>/dev/null
  "$HERE/build/s3trace" "$PACK/core.pack" "$cm" "$s" "$k" "$@" > "$OUT/$n.cpp.csv" 2>/dev/null
  "$REF/target/release/trace" "$PACK/root" "$m" "$s" "$k" --hex "$@" > "$OUT/$n.rust.hex" 2>/dev/null
  "$HERE/build/s3trace" "$PACK/core.pack" "$cm" "$s" "$k" --hex "$@" > "$OUT/$n.cpp.hex" 2>/dev/null
  if cmp -s "$OUT/$n.rust.csv" "$OUT/$n.cpp.csv" && cmp -s "$OUT/$n.rust.hex" "$OUT/$n.cpp.hex"; then
    echo "$n ($k ticks): IDENTICAL (decimal and hex)"
  else
    fail=1; echo "$n ($k ticks): DIFFERS"
    python3 "$HERE/tools/trace_diff.py" "$OUT/$n.rust.csv" "$OUT/$n.cpp.csv" --columns 10 | sed 's/^/    /'
  fi
}
for s in idle:300 push:400 ollie:360 kickflip:360 carve:360; do
  run KenneyPark "$REF/scripts/${s%%:*}.txt" "${s##*:}"
done
if [ "${1:-}" = --extra ]; then
  for s in "$HERE"/tools/parity_scripts/*.txt; do
    run KenneyPark "$s" 2400; run test-world "$s" 2400
  done
  run KenneyPark "$REF/scripts/kickflip.txt" 360 --difficulty normal
  run KenneyPark "$REF/scripts/carve.txt" 360 --difficulty hardcore
fi
exit $fail
