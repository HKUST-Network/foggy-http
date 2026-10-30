#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get -o Acquire::Retries=3 update
apt-get -o Acquire::Retries=3 install -y gcc g++ make python3 ca-certificates tzdata nginx libnginx-mod-stream iproute2 util-linux tcpdump unzip
if apt-cache show nginx-dev >/dev/null 2>&1; then
    apt-get -o Acquire::Retries=3 install -y nginx-dev
else
    # Ubuntu 22.04 has no nginx-dev; use the bundled matching source headers.
    apt-get -o Acquire::Retries=3 install -y libpcre3-dev zlib1g-dev libssl-dev
fi
project_root=$(cd "$(dirname "$0")/.." && pwd)
bash "$project_root/foggy-http/support/nginx/install.sh"
