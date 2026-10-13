#!/usr/bin/env python3
"""Hub: o painel das sessões do Felipe (quem está fazendo o quê, a fila da placa, a fila do EmberGen).

Servidor mínimo, sem dependências: serve a página (Hub/index.html) e /state (JSON) lendo o que já existe:
  - C:\\Users\\luisf\\.gpu_queue\\tickets\\*.json    fila da placa (gpu_queue.ps1)
  - captures\\.gpu_lock                            trava antiga (quem usa a placa por fora da fila)
  - C:\\Users\\luisf\\.ember_queue\\tickets\\*.json  fila do EmberGen (ember_queue.ps1)
  - C:\\Users\\luisf\\.hub\\sessions\\*.json        o status que cada sessão escreve com Hub\\hub.ps1
  - ~\\.claude\\projects\\...\\*.jsonl              a atividade (mtime) e o título de cada sessão
  - nvidia-smi / processos                         a placa de verdade (VRAM, uso) e as Unreal/EmberGen abertas

As sessões NÃO mudam nada do fluxo delas: as filas seguem por comando. Só o status é novo (hub.ps1 set).
Uso: python Hub/server.py [porta=8140]
"""
import glob
import json
import os
import re
import subprocess
import sys
import threading
import time
from datetime import datetime
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..'))
HOME = os.path.expanduser('~')
GPU_TICKETS = os.path.join(HOME, '.gpu_queue', 'tickets')
EMBER_TICKETS = os.path.join(HOME, '.ember_queue', 'tickets')
GPU_LOCK = os.path.join(REPO, 'captures', '.gpu_lock')
HUB_DIR = os.path.join(HOME, '.hub')
HUB_SESSIONS = os.path.join(HUB_DIR, 'sessions')
EVENTS_FILE = os.path.join(HUB_DIR, 'events.jsonl')
TRANSCRIPTS = glob.glob(os.path.join(HOME, '.claude', 'projects', 'C--Users-luisf-*'))
WAIT_STALE_MIN, HOLD_STALE_MIN = 3.0, 20.0     # os mesmos das filas
SESSION_WINDOW_H = 36                          # sessões sem atividade há mais que isso somem do painel
CREATE_NO_WINDOW = 0x08000000

try:
    import psutil
except ImportError:                             # sem psutil: lista processos pelo PowerShell (mais lento)
    psutil = None


def now():
    return datetime.now().astimezone()


def minutes_since(ts):
    return max(0.0, (time.time() - ts) / 60.0)


def read_json(path):
    try:
        with open(path, 'r', encoding='utf-8-sig') as f:
            return json.load(f)
    except Exception:
        return None


def split_owner(owner):
    """'sessão | tarefa' -> (sessão, tarefa). Os tickets já vêm assim."""
    parts = [p.strip() for p in (owner or '').split('|')]
    if len(parts) >= 2:
        session = parts[0]
        task = ' | '.join(p for p in parts[1:] if p and p != 'via fila' and not p.startswith('20'))
        if session and task.lower().startswith(session.lower()):     # "BlastLab | BlastLab A/B..." -> "A/B..."
            task = task[len(session):].lstrip(' :-·|')
        return session, task
    return (parts[0] if parts else '?'), ''


def read_queue(folder):
    """Mesma leitura da fila: ordem do nome = chegada; ticket abandonado é ignorado (não apagamos: isso é das filas)."""
    out = []
    for f in sorted(glob.glob(os.path.join(folder, '*.json'))):
        t = read_json(f)
        if not t or not t.get('owner'):
            continue
        try:
            age = minutes_since(os.path.getmtime(f))
        except OSError:
            continue
        limit = HOLD_STALE_MIN if t.get('state') == 'holding' else WAIT_STALE_MIN
        if age > limit:
            continue
        session, task = split_owner(t['owner'])
        try:
            created = datetime.fromisoformat(t['created'])
            since_min = max(0.0, (now() - created).total_seconds() / 60.0)
        except Exception:
            since_min = age
        out.append({'owner': t['owner'], 'session': session, 'task': task, 'tag': t.get('tag', ''),
                    'state': t.get('state'), 'since_min': round(since_min, 1), 'touched_min': round(age, 1),
                    'stale_min': limit})
    return out


def read_lock():
    if not os.path.exists(GPU_LOCK):
        return None
    try:
        age = minutes_since(os.path.getmtime(GPU_LOCK))
        with open(GPU_LOCK, 'r', encoding='utf-8-sig') as f:
            txt = f.read().strip()
    except OSError:
        return None
    if age > HOLD_STALE_MIN:
        return None
    session, task = split_owner(txt)
    return {'text': txt, 'session': session, 'task': task, 'age_min': round(age, 1), 'via_queue': 'via fila' in txt}


def read_gpu():
    try:
        r = subprocess.run(['nvidia-smi', '--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu',
                            '--format=csv,noheader,nounits'], capture_output=True, text=True, timeout=4,
                           creationflags=CREATE_NO_WINDOW)
        name, util, used, total, temp = [x.strip() for x in r.stdout.strip().splitlines()[0].split(',')]
        return {'name': name.replace('NVIDIA GeForce ', ''), 'util': int(util), 'mem_used': int(used),
                'mem_total': int(total), 'temp': int(temp)}
    except Exception:
        return None


PROJECT_RE = re.compile(r'([A-Za-z0-9_]+)\.uproject', re.I)


def classify_process(name, cmd):
    n = (name or '').lower()
    if n.startswith('unrealeditor'):
        m = PROJECT_RE.search(cmd or '')
        return {'kind': 'unreal', 'label': m.group(1) if m else 'Unreal', 'nullrhi': '-nullrhi' in (cmd or '').lower()}
    if n.startswith('embergen'):
        return {'kind': 'ember', 'label': 'EmberGen', 'nullrhi': False}
    if n.startswith('blender'):
        return {'kind': 'blender', 'label': 'Blender', 'nullrhi': False}
    return None


def read_processes():
    out = []
    if psutil:
        for p in psutil.process_iter(['pid', 'name', 'cmdline', 'create_time']):
            try:
                c = classify_process(p.info['name'], ' '.join(p.info['cmdline'] or []))
                if c:
                    c.update(pid=p.info['pid'], age_min=round(minutes_since(p.info['create_time']), 1))
                    out.append(c)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return out
    try:
        ps = ("Get-CimInstance Win32_Process -Filter \"Name LIKE 'UnrealEditor%' OR Name='EmberGen.exe' OR Name='blender.exe'\" "
              "| Select-Object ProcessId,Name,CommandLine | ConvertTo-Json -Compress")
        r = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True, timeout=20,
                           creationflags=CREATE_NO_WINDOW)
        data = json.loads(r.stdout or '[]')
        if isinstance(data, dict):
            data = [data]
        for p in data:
            c = classify_process(p.get('Name'), p.get('CommandLine'))
            if c:
                c.update(pid=p.get('ProcessId'), age_min=None)
                out.append(c)
    except Exception:
        pass
    return out


def read_hub_sessions():
    out = []
    for f in glob.glob(os.path.join(HUB_SESSIONS, '*.json')):
        d = read_json(f)
        if d and d.get('session'):
            for key in ('updated', 'since'):
                try:
                    d[key + '_min'] = round(max(0.0, (now() - datetime.fromisoformat(d[key])).total_seconds() / 60), 1)
                except Exception:
                    d[key + '_min'] = None
            out.append(d)
    return out


TITLE_CACHE = {}                                # transcript -> (tamanho visto, título)
TITLE_RE = re.compile(rb'"customTitle"\s*:\s*"((?:[^"\\]|\\.)*)"')


def transcript_title(path, size):
    """Título que aparece na barra lateral. Procura no fim do arquivo (barato); se não achar, varre uma vez."""
    cached = TITLE_CACHE.get(path)
    if cached and cached[0] == size:
        return cached[1]
    title = cached[1] if cached else None
    try:
        with open(path, 'rb') as f:
            f.seek(max(0, size - 4 * 1024 * 1024))
            m = TITLE_RE.findall(f.read())
            if m:
                title = m[-1]
            elif not cached:                    # varredura única, em pedaços
                f.seek(0)
                carry = b''
                while True:
                    chunk = f.read(8 * 1024 * 1024)
                    if not chunk:
                        break
                    found = TITLE_RE.findall(carry + chunk)
                    if found:
                        title = found[-1]
                    carry = chunk[-256:]
    except OSError:
        pass
    if isinstance(title, bytes):
        try:
            title = json.loads(b'"' + title + b'"')
        except Exception:
            title = title.decode('utf-8', 'replace')
    TITLE_CACHE[path] = (size, title)
    return title


def read_transcripts():
    out = []
    for folder in TRANSCRIPTS:
        for f in glob.glob(os.path.join(folder, '*.jsonl')):
            try:
                st = os.stat(f)
            except OSError:
                continue
            idle = minutes_since(st.st_mtime)
            if idle > SESSION_WINDOW_H * 60:
                continue
            sid = os.path.splitext(os.path.basename(f))[0]
            out.append({'id': sid, 'title': transcript_title(f, st.st_size) or sid[:8],
                        'idle_min': round(idle, 1), 'project': os.path.basename(folder).replace('C--Users-luisf-', ''),
                        'size_mb': round(st.st_size / 1e6, 1)})
    out.sort(key=lambda s: s['idle_min'])
    return out


def norm(s):
    return re.sub(r'[^a-z0-9]+', ' ', (s or '').lower()).strip()


NAMES_FILE = os.path.join(HUB_DIR, 'names.json')


def read_names():
    """Nomes curtos escolhidos pelo Felipe: { "<título da barra lateral>": {"name": "BlastLab", "aliases": [...]} }.
    Os aliases são os nomes que a sessão usa no -Owner das filas."""
    return read_json(NAMES_FILE) or {}


def match_session(name, transcripts, hub, names):
    """Liga um nome livre ('BlastLab', 'render noturno') ao título da sessão: names.json, apelido do hub, igual ou contido."""
    n = norm(name)
    if not n:
        return None
    for title, cfg in names.items():
        aliases = [cfg.get('name')] + list(cfg.get('aliases') or [])
        if any(norm(a) == n for a in aliases if a):
            return title
    for h in hub:                                      # a sessão disse quem ela é (hub.ps1 set ... -Alias ...)
        aliases = [h.get('session')] + list(h.get('aliases') or [])
        if any(norm(a) == n for a in aliases if a):
            return h.get('title') or h.get('session')
    for t in transcripts:
        tt = norm(t['title'])
        if tt == n or (len(n) >= 4 and (n in tt or tt in n)):
            return t['title']
    return None


class Collector(threading.Thread):
    """Lê tudo num ritmo fixo (a página só lê o último estado) e anota os acontecimentos."""
    daemon = True

    def __init__(self):
        super().__init__()
        self.lock = threading.Lock()
        self.state = {'ready': False}
        self.events = []
        self.prev = None
        self.proc_cache, self.proc_at = [], 0.0
        os.makedirs(HUB_SESSIONS, exist_ok=True)
        self.load_events()

    def load_events(self):
        try:
            with open(EVENTS_FILE, 'r', encoding='utf-8') as f:
                lines = f.readlines()[-120:]
            self.events = [json.loads(l) for l in lines if l.strip()]
        except Exception:
            self.events = []

    def event(self, kind, text, who=''):
        e = {'t': now().isoformat(timespec='seconds'), 'kind': kind, 'text': text, 'who': who}
        self.events.append(e)
        self.events = self.events[-200:]
        try:
            with open(EVENTS_FILE, 'a', encoding='utf-8') as f:
                f.write(json.dumps(e, ensure_ascii=False) + '\n')
        except OSError:
            pass

    def diff(self, prev, cur):
        for key, label in (('unreal', 'Placa'), ('ember', 'EmberGen')):
            ph, ch = prev[key]['holder'], cur[key]['holder']
            if (ph or {}).get('owner') != (ch or {}).get('owner'):
                if ch:
                    self.event(key, f'{ch["session"]} pegou: {ch["task"] or "sem tarefa"}', ch['session'])
                elif ph:
                    self.event(key, f'{ph["session"]} soltou', ph['session'])
            pw = {t['owner'] for t in prev[key]['waiting']}
            cw = {t['owner'] for t in cur[key]['waiting']}
            for t in cur[key]['waiting']:
                if t['owner'] not in pw:
                    self.event(key, f'{t["session"]} entrou na fila: {t["task"] or "sem tarefa"}', t['session'])
            for t in prev[key]['waiting']:
                if t['owner'] not in cw and t['owner'] != (ch or {}).get('owner'):
                    self.event(key, f'{t["session"]} saiu da fila sem usar', t['session'])
        pp = {p['pid']: p for p in prev['processes']}
        cp = {p['pid']: p for p in cur['processes']}
        for pid, p in cp.items():
            if pid not in pp:
                self.event('proc', f'{p["label"]} abriu (PID {pid}{", sem janela" if p["nullrhi"] else ""})')
        for pid, p in pp.items():
            if pid not in cp:
                self.event('proc', f'{p["label"]} fechou (PID {pid})')
        ps = {h['session']: h for h in prev['hub']}
        for h in cur['hub']:
            o = ps.get(h['session'])
            if not o or o.get('doing') != h.get('doing') or o.get('state') != h.get('state'):
                self.event('status', f'{h["session"]} · {h.get("doing", "")}', h['session'], )

    def snapshot(self):
        gq, eq = read_queue(GPU_TICKETS), read_queue(EMBER_TICKETS)
        if time.time() - self.proc_at > (6 if psutil else 15):
            self.proc_cache, self.proc_at = read_processes(), time.time()
        procs = self.proc_cache
        transcripts = read_transcripts()
        hub = read_hub_sessions()
        lock = read_lock()
        names = read_names()
        short = lambda title: (names.get(title) or {}).get('name') or title

        def queue_block(q, kind):
            holder = next((t for t in q if t['state'] == 'holding'), None)
            waiting = [t for t in q if t['state'] != 'holding']
            return {'holder': holder, 'waiting': waiting, 'count': len(q),
                    'processes': [p for p in procs if p['kind'] == kind]}

        unreal, ember = queue_block(gq, 'unreal'), queue_block(eq, 'ember')
        unreal['outside_lock'] = lock if (lock and not lock['via_queue']) else None   # placa pela regra antiga
        unreal['lock'] = lock

        # as sessões: título da barra lateral + status do hub + o que as filas dizem delas
        cards = {}
        for t in transcripts:
            cards[t['title']] = {'title': t['title'], 'name': short(t['title']), 'id': t['id'], 'idle_min': t['idle_min'],
                                 'project': t['project'], 'status': None, 'unreal': None, 'ember': None, 'source': 'transcript'}
        for h in hub:
            key = h.get('title') or match_session(h['session'], transcripts, [], names) or h['session']
            c = cards.setdefault(key, {'title': key, 'name': short(key), 'id': None, 'idle_min': None, 'project': '',
                                       'status': None, 'unreal': None, 'ember': None, 'source': 'hub'})
            c['status'] = h
        for key, block in (('unreal', unreal), ('ember', ember)):
            ordered = ([block['holder']] if block['holder'] else []) + block['waiting']
            for i, t in enumerate(ordered):
                pos = 0 if t['state'] == 'holding' else (i if block['holder'] else i + 1)
                t['pos'] = pos
                title = match_session(t['session'], transcripts, hub, names)
                t['matched'] = title
                t['name'] = short(title) if title else t['session']   # a linha do recurso mostra o nome curto da sessão
                if not title or title not in cards:       # nome só da fila, sem sessão real: fica só na linha do recurso
                    continue
                cards[title][key] = {'state': t['state'], 'pos': pos, 'task': t['task'], 'since_min': t['since_min'],
                                     'touched_min': t['touched_min'], 'alias': t['session']}
        if lock:
            lt = match_session(lock['session'], transcripts, hub, names)
            lock['name'] = short(lt) if lt else lock['session']
        sessions = sorted(cards.values(), key=lambda c: norm(c['name']))   # ordem fixa: o olho não perde a referência
        return {'ready': True, 'started': STARTED, 'now': now().isoformat(timespec='seconds'), 'gpu': read_gpu(), 'unreal': unreal,
                'ember': ember, 'processes': procs, 'hub': hub, 'sessions': sessions}

    def run(self):
        while True:
            try:
                cur = self.snapshot()
                if self.prev:
                    self.diff(self.prev, cur)
                self.prev = cur
                with self.lock:
                    self.state = cur
            except Exception as e:           # nunca derruba o painel
                with self.lock:
                    self.state = dict(self.state, error=str(e))
            time.sleep(2.0)

    def get(self):
        with self.lock:
            return dict(self.state, events=list(reversed(self.events[-60:])))


STARTED = int(time.time())            # a página recarrega sozinha quando o servidor sobe de novo (versão nova)
COLLECTOR = Collector()


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=HERE, **k)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def do_GET(self):
        if self.path.startswith('/state'):
            body = json.dumps(COLLECTOR.get(), ensure_ascii=False).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path in ('/', ''):
            self.path = '/index.html'
        super().do_GET()

    def log_message(self, fmt, *args):
        pass


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8140
    COLLECTOR.start()
    ThreadingHTTPServer.allow_reuse_address = False   # no Windows, "reuse" deixava dois servidores na mesma porta
    try:
        srv = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    except OSError:
        print(f'já tem um Hub na porta {port}; não subi outro.')
        sys.exit(0)
    print(f'Hub em http://localhost:{port}  (Ctrl+C fecha)')
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
