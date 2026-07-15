#!/usr/bin/env bash
set -euo pipefail

aria2c="${1:-./src/aria2c.exe}"
out="${TMPDIR:-/tmp}/jaria-live-http2-smoke"

rm -rf "$out"
mkdir -p "$out"

"$aria2c" --enable-http2=true --allow-overwrite=true \
  --log="$out/cloudflare.log" --log-level=debug \
  -d "$out" -o cloudflare-robots.txt \
  https://www.cloudflare.com/robots.txt

"$aria2c" --enable-http2=true --allow-overwrite=true \
  --log="$out/google.log" --log-level=debug \
  -d "$out" -o google-robots.txt \
  https://www.google.com/robots.txt

"$aria2c" --enable-http2=true --max-tries=2 --allow-overwrite=true \
  --log="$out/nghttp2-bytes.log" --log-level=debug \
  -d "$out" \
  https://nghttp2.org/httpbin/bytes/1024

test "$(wc -c < "$out/1024")" -eq 1024
wc -c "$out"/*
