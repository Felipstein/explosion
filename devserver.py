#!/usr/bin/env python3
"""Servidor estático de dev com cache desligado + endpoint de captura.

- GET: serve os arquivos com no-store (o http.server padrão deixa o navegador
  reusar módulos ES do cache, e edição de shader parece "não pegar").
- POST /save?name=X.png: grava o corpo em captures/X.png. Permite inspecionar
  o render sem depender da janela do app estar visível.
- POST /asset?name=bake/<hash>.bin: grava o bake da explosão como asset em
  assets/bake/. A próxima carga baixa o arquivo em vez de simular. Assets
  com outro hash (de uma versão anterior do solver) são apagados.
"""
import os
import re
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

CAPTURES = 'captures'


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_POST(self):
        u = urlparse(self.path)
        if u.path == '/asset':
            return self._save_asset(u)
        if u.path != '/save':
            self.send_error(404)
            return
        name = (parse_qs(u.query).get('name') or ['capture.png'])[0]
        name = os.path.basename(name) or 'capture.png'
        n = int(self.headers.get('Content-Length', 0))
        data = self.rfile.read(n)
        os.makedirs(CAPTURES, exist_ok=True)
        path = os.path.join(CAPTURES, name)
        with open(path, 'wb') as f:
            f.write(data)
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(f'{path} {len(data)}'.encode())
        print(f'[capture] {path} ({len(data)} bytes)')

    def _save_asset(self, u):
        name = (parse_qs(u.query).get('name') or [''])[0]
        # bake/r<res>-<hash>.bin — um asset por resolução de bake
        m = re.fullmatch(r'bake/(r\d{2,3})-([0-9a-f]{8,32})\.bin', name)
        if not m:
            self.send_error(400, 'nome de asset inválido')
            return
        n = int(self.headers.get('Content-Length', 0))
        data = self.rfile.read(n)
        folder = os.path.join('assets', 'bake')
        os.makedirs(folder, exist_ok=True)
        keep = f'{m.group(1)}-{m.group(2)}.bin'
        # apaga as versões antigas DESTA resolução (e os do formato sem
        # prefixo); as outras resoluções ficam
        for old in os.listdir(folder):
            if not old.endswith('.bin') or old == keep:
                continue
            if old.startswith(m.group(1) + '-') or not old.startswith('r'):
                os.remove(os.path.join(folder, old))
        path = os.path.join(folder, keep)
        with open(path + '.tmp', 'wb') as f:
            f.write(data)
        os.replace(path + '.tmp', path)
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(f'{path} {len(data)}'.encode())
        print(f'[asset] {path} ({len(data)} bytes)')

    def log_message(self, fmt, *args):
        pass


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8129
    print(f'dev server em http://localhost:{port} (no-cache, POST /save, POST /asset)')
    ThreadingHTTPServer(('127.0.0.1', port), Handler).serve_forever()
