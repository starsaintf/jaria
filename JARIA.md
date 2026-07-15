# Jaria

Jaria is a fork of aria2 focused on modern HTTP transport behavior while preserving aria2's command-line workflow and GPL-2.0 licensing. The license remains the same as upstream aria2; see `COPYING`.

## Transport Improvements

- HTTP/2 over TLS can be enabled with `--enable-http2=true` when built with OpenSSL and libnghttp2.
- HTTP/3 configuration surfaces are available behind `--enable-http3=true` and dependency-gated build detection.
- OpenSSL ALPN advertises `h2` before `http/1.1`, while keeping `http/1.1` available as the fallback protocol.
- HTTP/2 response headers are converted into aria2's existing HTTP response model.
- HTTP/2 informational responses are skipped until the final response headers arrive.
- HTTP/2 forbidden connection-specific headers are stripped before request submission.
- HTTP/2 response bodies use an offset-backed buffer to avoid byte-by-byte deque churn.

## Fallback Behavior

If an HTTP/2 stream fails in a way that should be retried, Jaria disables HTTP/2 for that HTTPS origin and retries through the existing HTTP/1.1 path. That one-time protocol fallback does not consume the normal retry budget.

The fallback is scoped by HTTPS origin, so a reset on one host or port does not disable HTTP/2 globally.

HTTP/3 fallback state is also scoped by HTTPS origin. Until a QUIC transport is compiled in, `--enable-http3=true` is accepted but does not alter the TCP TLS ALPN list or HTTP/2/HTTP/1.1 behavior.

## TLS Backend Caveats

The HTTP/2 path is currently implemented for OpenSSL builds with libnghttp2. Other TLS backends still have backend-specific behavior and limitations inherited from aria2:

- GnuTLS behavior depends on the distro/package TLS stack.
- WinTLS support is limited by Windows TLS capabilities, especially on older Windows releases and TLS 1.3-only sites.
- AppleTLS support depends on platform Security framework behavior and may differ across macOS versions.

For the most predictable Jaria HTTP/2 behavior, build with OpenSSL and libnghttp2.

HTTP/3 requires a QUIC-capable build with libngtcp2 and libnghttp3 development headers. The current MSYS2/UCRT64 validation environment has HTTP/2 support, but does not expose UCRT64 pkg-config metadata for the QUIC development libraries.

## Tested Targets

The current Windows/MSYS2 UCRT64 validation path uses OpenSSL and libnghttp2 and has been tested with:

- `https://www.cloudflare.com/robots.txt`
- `https://www.google.com/robots.txt`
- `https://nghttp2.org/httpbin/bytes/1024`

The live smoke test is available as `test/live-http2-smoke.sh`. It is optional in CI because it depends on external network services.
