#!/usr/bin/env python3
"""Generate Stream-only NGINX configuration using the fixed course parser."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--root',required=True)
p.add_argument('--student-config',required=True)
a=p.parse_args()
r=Path(a.root).resolve(); r.mkdir(parents=True,exist_ok=True)
lua=Path(__file__).resolve().parents[1]/'foggy-http/support/nginx/lua'
config=f'''worker_processes 1;
pid "{r}/nginx.pid";
error_log "{r}/error.log" info;
events {{ worker_connections 128; }}
stream {{
    lua_package_path "{lua}/?.lua;;";
    log_format foggy '$remote_addr:$remote_port -> $server_port backend=$foggy_backend $bytes_received/$bytes_sent';
    access_log "{r}/stream.log" foggy;
    include "{Path(a.student_config).resolve()}";
}}
'''
(r/'nginx.conf').write_text(config)
