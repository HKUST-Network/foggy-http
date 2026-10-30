#!/usr/bin/env python3
"""Generate isolated ordinary NGINX configuration with the fixed C parser."""
import argparse
import os
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--root',required=True)
p.add_argument('--student-config',required=True)
p.add_argument('--module',default=os.environ.get('FOGGY_HTTP_MODULE',
               '/usr/lib/nginx/modules/ngx_stream_foggy_http_module.so'))
a=p.parse_args()
r=Path(a.root).resolve();r.mkdir(parents=True,exist_ok=True)
config=f'''load_module "/usr/lib/nginx/modules/ngx_stream_module.so";
load_module "{Path(a.module).resolve()}";
worker_processes 1;
pid "{r}/nginx.pid";
error_log "{r}/error.log" info;
events {{ worker_connections 128; }}
stream {{
    log_format foggy '$remote_addr:$remote_port path=$foggy_http_path backend=$backend $bytes_received/$bytes_sent';
    access_log "{r}/stream.log" foggy;
    include "{Path(a.student_config).resolve()}";
}}
'''
(r/'nginx.conf').write_text(config)
