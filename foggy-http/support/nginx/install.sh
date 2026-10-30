#!/usr/bin/env bash
# Build only the fixed course module against the installed Debian/Ubuntu NGINX.
set -euo pipefail
support_root=$(cd "$(dirname "$0")" && pwd)
nginx_bin=/usr/sbin/nginx
module_dir=/usr/lib/nginx/modules
module_name=ngx_stream_foggy_http_module.so
metadata="$module_dir/ngx_stream_foggy_http_module.build-id"
if [[ ! -x "$nginx_bin" ]]; then
    printf '%s\n' 'Install nginx and libnginx-mod-stream first.' >&2
    exit 1
fi
runtime_version=$("$nginx_bin" -v 2>&1 | sed -n 's#.*nginx/\([0-9.]*\).*#\1#p')
source_archive="$support_root/nginx-$runtime_version.tar.gz"
if [[ -f /usr/share/nginx/src/configure ]]; then
    source_kind=nginx-dev
elif [[ -n "$runtime_version" && -f "$source_archive" ]]; then
    source_kind=bundled-source
    (cd "$support_root" && sha256sum -c SOURCE-SHA256SUMS)
else
    printf '%s\n' "No matching development headers for NGINX $runtime_version. Install matching nginx-dev." >&2
    exit 1
fi
build_id=$({
    "$nginx_bin" -V 2>&1
    dpkg-query -W nginx libnginx-mod-stream
    printf '%s\n' "$source_kind"
    if [[ "$source_kind" == nginx-dev ]]; then dpkg-query -W nginx-dev; fi
    uname -m
    sha256sum "$support_root/module/config" \
        "$support_root/module/ngx_stream_foggy_http_module.c" "$support_root/install.sh" "$support_root/SOURCE-SHA256SUMS"
} | sha256sum | cut -d' ' -f1)
if [[ -f "$module_dir/$module_name" && -f "$metadata" ]] && \
   [[ $(cat "$metadata") == "$build_id" ]]; then
    printf '%s\n' 'Course NGINX module already matches this runtime and source.'
    exit 0
fi
build_root=$(mktemp -d /tmp/foggy-nginx-module-XXXXXXXX)
trap 'rm -rf "$build_root"' EXIT
before=$(sha256sum "$nginx_bin" | cut -d' ' -f1)
if [[ "$source_kind" == nginx-dev ]]; then
    mkdir "$build_root/source"
    cp -a /usr/share/nginx/src/. "$build_root/source/"
    source_root="$build_root/source"
else
    tar -xzf "$source_archive" -C "$build_root"
    source_root="$build_root/nginx-$runtime_version"
fi
cd "$source_root"
./configure --with-compat --with-stream \
    --add-dynamic-module="$support_root/module" > configure.log 2>&1 || \
    { cat configure.log; exit 1; }
make -f objs/Makefile modules > module-build.log 2>&1 || \
    { cat module-build.log; exit 1; }
mkdir -p "$module_dir"
compiled_module="$source_root/objs/$module_name"
mkdir -p "$build_root/check"
cat > "$build_root/check/nginx.conf" <<EOF
load_module "$module_dir/ngx_stream_module.so";
load_module "$compiled_module";
pid "$build_root/check/nginx.pid";
error_log stderr;
events { worker_connections 16; }
stream { map \$foggy_http_path \$backend { default ""; } }
EOF
"$nginx_bin" -t -p "$build_root/check/" -c "$build_root/check/nginx.conf"
after=$(sha256sum "$nginx_bin" | cut -d' ' -f1)
[[ "$before" == "$after" ]] || { printf '%s\n' 'Unexpected NGINX binary change.' >&2; exit 1; }
install -m 644 "$compiled_module" "$module_dir/$module_name"
printf '%s\n' "$build_id" > "$metadata"
printf '%s\n' "Installed $module_dir/$module_name; the NGINX executable is unchanged."
