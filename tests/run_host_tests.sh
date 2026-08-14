#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

cleanup() {
  rm -f tests/test_bspatch_host tests/test_sha1_host \
        tests/old.bin tests/new.bin tests/result.bin tests/change.bsdiff
}
trap cleanup EXIT

gcc -std=c11 -Isource tests/test_sha1_host.c source/sha1.c source/util.c \
  -o tests/test_sha1_host
gcc -std=c11 -Isource tests/test_bspatch_host.c tests/host_stubs.c \
  source/bspatch.c source/util.c -lbz2 -o tests/test_bspatch_host

./tests/test_sha1_host
python3 - <<'PY'
from pathlib import Path
import random
try:
    import bsdiff4
except ImportError as exc:
    raise SystemExit("Install host test dependency: python3 -m pip install bsdiff4") from exc
rng = random.Random(12345)
old = bytearray(rng.randbytes(500_000))
new = bytearray(old)
new[10_000:20_000] = b"A" * 18_000
new[300_000:310_000] = b""
new.extend(b"WiiLink" * 5000)
Path("tests/old.bin").write_bytes(old)
Path("tests/new.bin").write_bytes(new)
Path("tests/change.bsdiff").write_bytes(bsdiff4.diff(bytes(old), bytes(new)))
PY
./tests/test_bspatch_host tests/old.bin tests/change.bsdiff tests/result.bin
cmp tests/new.bin tests/result.bin
printf 'host core tests: PASS\n'
