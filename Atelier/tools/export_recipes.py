# A receita das imagens de uma área: exporta os pedidos do Atelier (captures/atelier/requests, fora do git) pra um .md no
# repositório, com o prompt inteiro de cada rodada, os modelos, os anexos na ordem e o que foi escolhido. Os anexos que não estão
# no repositório são copiados (jpg leve) pra pasta de anexos da receita, pra que outra sessão consiga refazer ou melhorar.
#
#   python Atelier/tools/export_recipes.py --src Superarmas --out docs/superarmas/RECEITAS_DAS_IMAGENS.md
#
# --src é a sessão que pediu (o campo "src" do pedido); --match filtra pelo título (ex.: "superarma|colmeia").
import argparse, glob, hashlib, json, os, re, sys
from datetime import datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MODELS = {'gpt': 'GPT (conta do ChatGPT ou API)', 'pro': 'Gemini Pro', 'flash': 'Gemini Flash', 'sound': 'ElevenLabs'}

def repo_index():
    """basename (sem o número de ordem do anexo) -> caminho no repositório (docs/ e Game/Content não entram; só docs/)."""
    idx = {}
    for p in glob.glob(os.path.join(ROOT, 'docs', '**', '*.*'), recursive=True):
        idx.setdefault(os.path.basename(p).lower(), os.path.relpath(p, ROOT).replace('\\', '/'))
    return idx

def thumb(path):
    from PIL import Image
    im = Image.open(path).convert('L').resize((24, 14))
    return list(im.tobytes())

def approved_thumbs(folder):
    out = []
    for p in sorted(glob.glob(os.path.join(folder, '*.jpg')) + glob.glob(os.path.join(folder, '*.png'))):
        try: out.append((os.path.relpath(p, ROOT).replace(os.sep, '/'), thumb(p)))
        except Exception: pass
    return out

def match_approved(path, appr):
    """a cópia aprovada mais parecida (diferença média < 14 em 255), ou None."""
    if not appr or not os.path.exists(path): return None
    t = thumb(path); best = None
    for (rel, th) in appr:
        dif = sum(abs(x - y) for x, y in zip(t, th)) / len(t)
        if dif < 14 and (best is None or dif < best[1]): best = (rel, dif)
    return best[0] if best else None

def strip_order(name):
    return re.sub(r'^\d+_', '', name)

def copy_light(src, dst):
    from PIL import Image
    im = Image.open(src).convert('RGB')
    im.thumbnail((1600, 1600))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    im.save(dst, quality=82)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--match', default='')
    ap.add_argument('--approved', default='', help='pasta das cópias aprovadas (ex.: docs/aprovados/superarmas): liga cada saída à sua')
    a = ap.parse_args()
    out = os.path.join(ROOT, a.out); att_dir = os.path.splitext(out)[0] + '_anexos'
    idx = repo_index()
    appr = approved_thumbs(os.path.join(ROOT, a.approved)) if a.approved else []
    seen = {}   # o mesmo anexo (pelo conteúdo) é copiado uma vez só
    reqs = []
    for f in glob.glob(os.path.join(ROOT, 'captures', 'atelier', 'requests', '*.json')):
        d = json.load(open(f, encoding='utf-8'))
        if d.get('src') != a.src: continue
        if a.match and not re.search(a.match, d.get('title', '') + ' ' + d.get('folder', ''), re.I): continue
        reqs.append(d)
    reqs.sort(key=lambda d: d.get('created', ''))
    L = [f'# A receita das imagens: {a.src}', '',
         f'Gerado por `Atelier/tools/export_recipes.py --src {a.src}` em {datetime.now():%d/%m/%Y %H:%M}, a partir dos pedidos do Atelier',
         '(`captures/atelier/requests/`, fora do git). Cada pedido traz o prompt inteiro de cada rodada, os modelos, os anexos na ordem em',
         'que foram mandados e o que foi escolhido (✓) ou reprovado (✗). Os anexos que não estavam no repositório foram copiados pra',
         f'`{os.path.relpath(att_dir, ROOT).replace(os.sep, "/")}/`. As imagens geradas ficam em `captures/concept/` (rascunho); as aprovadas',
         'estão em `docs/aprovados/` e no `docs/APROVADOS.md`.', '']
    for d in reqs:
        folder = d.get('folder', '').rstrip('/')
        L += ['---', '', f'## {os.path.basename(folder)}: {d.get("title", "")}', '',
              f'- **Data:** {d.get("created", "")[:16].replace("T", " ")} (UTC) · **proporção:** {d.get("aspect", "")} · **pedido:** `{d.get("id")}`']
        # os anexos
        if d.get('att'):
            L.append('- **Anexos, na ordem:**')
            for i, at in enumerate(d['att'], 1):
                base = strip_order(os.path.basename(at['file']))
                hit = idx.get(base.lower())
                if hit:
                    where = f'`{hit}`'
                else:
                    srcp = os.path.join(ROOT, at['file'])
                    if os.path.exists(srcp):
                        h = hashlib.md5(open(srcp, 'rb').read()).hexdigest()
                        dst = seen.get(h) or os.path.join(att_dir, f'{os.path.basename(folder)}__{os.path.splitext(base)[0]}.jpg')
                        seen[h] = dst
                        if not os.path.exists(dst): copy_light(srcp, dst)
                        where = f'`{os.path.relpath(dst, ROOT).replace(os.sep, "/")}` (cópia)'
                    else:
                        where = f'`{at["file"]}` (não achado)'
                L.append(f'  {i}. {at.get("label", base)}: {where}')
        # o prompt da raiz e o de cada ramo e rodada
        for p in d.get('root') or []:
            L += ['', '**Prompt (a raiz):**', '', '```text', p.strip(), '```']
        for r in d.get('ramos', []):
            if r.get('prompt'):
                L += ['', f'**Prompt do ramo "{r.get("name")}":**', '', '```text', r['prompt'].strip(), '```']
            for k, rd in enumerate(r.get('rounds', []), 1):
                if rd.get('prompt'):
                    L += ['', f'**Rodada {k} do ramo "{r.get("name")}" (o pedido de ajuste):**', '', '```text', rd['prompt'].strip(), '```']
                its = []
                for it in rd.get('items', []):
                    mark = ' ✓' if it.get('pick') else (' ✗' if it.get('rej') else '')
                    f = it.get('file') or ''
                    hit = idx.get(os.path.basename(f).lower())
                    st = '' if it.get('st') == 'd' else f' ({it.get("st")}: {it.get("err", "")[:80]})'
                    ok = match_approved(os.path.join(ROOT, f), appr) if f else None
                    tail = f' → **registrada: `{ok}`**' if ok else ''
                    its.append(f'{MODELS.get(it.get("m"), it.get("m"))}{mark}{st}: `{hit or f}`{tail}')
                if its:
                    L.append(f'- **Rodada {k}, ramo "{r.get("name")}":** ' + '; '.join(its))
        if d.get('thread'):
            L += ['', '**A conversa dentro do pedido:**']
            for t in d['thread']:
                L.append(f'- {t.get("who", "")}: {str(t.get("text", ""))[:400]}')
        L.append('')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w', encoding='utf-8').write('\n'.join(L))
    print(f'{len(reqs)} pedidos -> {os.path.relpath(out, ROOT)}')

if __name__ == '__main__':
    main()
