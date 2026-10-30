#!/usr/bin/env bash
# Install a pinned NGINX/OpenResty with Stream Lua and the fixed course adapter.
set -euo pipefail
support_root=$(cd "$(dirname "$0")" && pwd)
install_root=${FOGGY_NGINX_PREFIX:-/opt/foggy-nginx}
version=1.27.1.2
adapter_hash=$(cat "$support_root/module/ngx_stream_foggy_module.c" "$support_root/install.sh" "$support_root/SOURCE-SHA256SUMS" | sha256sum | cut -d' ' -f1)
if [[ -x "$install_root/nginx/sbin/nginx" && -f "$install_root/course-adapter.sha256" ]] && \
   [[ $(cat "$install_root/course-adapter.sha256") == "$adapter_hash" ]]; then
    exit 0
fi
build_root=$(mktemp -d /tmp/foggy-nginx-build-XXXXXXXX)
trap 'rm -rf "$build_root"' EXIT
(cd "$support_root" && sha256sum -c SOURCE-SHA256SUMS)
cp "$support_root/openresty-$version.tar.gz" "$build_root/openresty.tar.gz"
tar -xzf "$build_root/openresty.tar.gz" -C "$build_root"
cd "$build_root/openresty-$version"
./configure --prefix="$install_root" --with-stream \
    --without-http_gzip_module \
    --add-module="$support_root/module" > "$build_root/configure.log" 2>&1 || \
    { cat "$build_root/configure.log"; exit 1; }
make -j"${FOGGY_BUILD_JOBS:-2}" > "$build_root/make.log" 2>&1 || \
    { tail -100 "$build_root/make.log"; exit 1; }
make install > "$build_root/install.log" 2>&1
if command -v setcap >/dev/null && [[ $(id -u) == 0 ]]; then
    setcap cap_net_bind_service=+ep "$install_root/nginx/sbin/nginx"
fi
printf '%s\n' "$adapter_hash" > "$install_root/course-adapter.sha256"
"$install_root/nginx/sbin/nginx" -V
