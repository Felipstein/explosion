#!/usr/bin/env python3
"""Servidor do FXLab: serve a raiz do repositório (a página em /FXLab/web/ lê o
cozimento em captures/ e o tanque) com cache desligado e tipos certos pra
módulos ES no Windows (o registro às vezes diz text/plain pra .js).

POST /save?name=X.png grava o corpo em captures/fxlab/shots/X.png: a página
fotografa o próprio canvas, sem depender da janela estar visível.
POST /preset grava o ajuste em FXLab/preset.json (o que a Unreal lê e recarrega sozinha).

Uso: python FXLab/web/devserver.py [porta=8131]
"""
import json
import os
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SHOTS = os.path.join(ROOT, 'captures', 'fxlab', 'shots')
PRESET = os.path.join(ROOT, 'FXLab', 'preset.json')


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.js': 'text/javascript', '.mjs': 'text/javascript',
                      '.json': 'application/json', '.exr': 'application/octet-stream', '.glb': 'model/gltf-binary',
                      '.mp4': 'video/mp4'}

    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        super().end_headers()

    def do_POST(self):
        u = urlparse(self.path)
        if u.path == '/preset':
            n = int(self.headers.get('Content-Length', 0))
            data = json.loads(self.rfile.read(n).decode('utf-8'))      # só grava JSON válido
            with open(PRESET, 'w', encoding='utf-8') as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            self.send_response(200)
            self.end_headers()
            return
        if u.path != '/save':
            self.send_error(404)
            return
        name = os.path.basename((parse_qs(u.query).get('name') or ['shot.png'])[0]) or 'shot.png'
        os.makedirs(SHOTS, exist_ok=True)
        n = int(self.headers.get('Content-Length', 0))
        with open(os.path.join(SHOTS, name), 'wb') as f:
            f.write(self.rfile.read(n))
        self.send_response(200)
        self.end_headers()

    def log_message(self, fmt, *args):
        if '/save' in str(args[0] if args else ''):
            super().log_message(fmt, *args)


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8131
    print(f'FXLab em http://localhost:{port}/FXLab/web/ (raiz {ROOT})', flush=True)
    ThreadingHTTPServer(('127.0.0.1', port), Handler).serve_forever()
