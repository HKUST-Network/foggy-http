#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y gcc g++ make python3 tzdata perl ca-certificates libpcre2-dev libssl-dev libcap2-bin iproute2 util-linux tcpdump unzip
project_root=$(cd "$(dirname "$0")/.." && pwd)
bash "$project_root/foggy-http/support/nginx/install.sh"
