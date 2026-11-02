# Gera os cursores do jogo a partir do mestre (art/cursors/cursors.html, docs/CURSORES.md): cada cursor em 48, 64 e 96 px,
# quadro a quadro pelo Edge headless (o modo #export&t=..), vira um .cur (parado) ou um .ani (animado, o Windows anima
# sozinho) em Game/Content/Cursors/<px>/. Os quadros vão em PNG dentro do arquivo (o Windows aceita desde o Vista) e cada
# arquivo é conferido no próprio Windows (LoadImage). O 1º quadro de cada um também sai em PNG em captures/ (pra prova).
# (01/11) Também as variantes da fila do Alt (q_<ordem>_<1-9|plus>): a ordem com a ficha do número da vez, paradas.
# Uso: python art/tools/make_cursors.py
import ctypes, html, io, json, os, re, struct, subprocess, sys
from PIL import Image

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BOARD = "file:///" + os.path.join(ROOT, "art", "cursors", "cursors.html").replace("\\", "/")
OUT = os.path.join(ROOT, "Game", "Content", "Cursors")
WORK = os.path.join(ROOT, "captures", "concept", "cursores", "export")


def edge(args):
    return subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=1"] + args,
                          capture_output=True)


def manifest():
    dom = edge(["--dump-dom", BOARD + "#export"]).stdout.decode("utf-8", "replace")
    m = re.search(r'<pre id="manifest"[^>]*>(.*?)</pre>', dom, re.S)
    return json.loads(html.unescape(m.group(1)))


def shot(t, width, height):
    path = os.path.join(WORK, "frames", f"t{t:.4f}.png")
    if not os.path.exists(path):
        edge(["--default-background-color=00000000", f"--window-size={width},{height}", f"--screenshot={path}", BOARD + f"#export&t={t:.4f}"])
    return Image.open(path).convert("RGBA")


def cur_bytes(png, px, hot):
    # um .cur com uma imagem: o cabeçalho, a entrada (o clique vai em planes/bitcount) e o PNG
    head = struct.pack("<HHH", 0, 2, 1)
    entry = struct.pack("<BBBBHHII", px % 256, px % 256, 0, 0, hot[0], hot[1], len(png), 22)
    return head + entry + png


def chunk(cid, data):
    return cid + struct.pack("<I", len(data)) + data + (b"\0" if len(data) & 1 else b"")


def ani_bytes(curs, jiffies):
    anih = struct.pack("<9I", 36, len(curs), len(curs), 0, 0, 0, 0, jiffies, 1)   # AF_ICON: cada quadro é um .cur inteiro
    fram = b"fram" + b"".join(chunk(b"icon", c) for c in curs)
    body = b"ACON" + chunk(b"anih", anih) + chunk(b"LIST", fram)
    return b"RIFF" + struct.pack("<I", len(body)) + body


def windows_accepts(path):
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.LoadImageW.restype = ctypes.c_void_p
    h = user32.LoadImageW(None, path, 2, 0, 0, 0x10)   # IMAGE_CURSOR, LR_LOADFROMFILE
    if h:
        user32.DestroyCursor(ctypes.c_void_p(h))
    return bool(h)


def main():
    os.makedirs(os.path.join(WORK, "frames"), exist_ok=True)
    man = manifest()
    bands, cursors = man["bands"], man["cursors"]
    width = man.get("cols", len(cursors)) * 112 + 16
    height = man.get("rows", 1) * man.get("rowh", 264) + 16
    times = sorted({round(k / c["fps"], 4) for c in cursors for k in range(c["frames"])})
    print(f"{len(cursors)} cursores, {len(times)} instantes")
    sheets = {t: shot(t, width, height) for t in times}
    total, bad = 0, []
    for b in bands:
        px = b["px"]
        os.makedirs(os.path.join(OUT, str(px)), exist_ok=True)
        os.makedirs(os.path.join(WORK, str(px)), exist_ok=True)
        for c in cursors:
            hot = (min(px - 1, round(c["hot"][0] * px / 64)), min(px - 1, round(c["hot"][1] * px / 64)))
            curs = []
            for k in range(c["frames"]):
                y0 = c.get("y", 0) + b["top"]
                im = sheets[round(k / c["fps"], 4)].crop((c["x"], y0, c["x"] + px, y0 + px))
                if k == 0:
                    im.save(os.path.join(WORK, str(px), c["name"] + ".png"))
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                curs.append(cur_bytes(buf.getvalue(), px, hot))
            if c["frames"] > 1:
                data, ext = ani_bytes(curs, round(60 / c["fps"])), ".ani"
            else:
                data, ext = curs[0], ".cur"
            path = os.path.join(OUT, str(px), c["name"] + ext)
            other = os.path.join(OUT, str(px), c["name"] + (".cur" if ext == ".ani" else ".ani"))
            if os.path.exists(other):
                os.remove(other)
            open(path, "wb").write(data)
            total += len(data)
            if not windows_accepts(path):
                bad.append(path)
    print(f"{total / 1024 / 1024:.1f} MB em {OUT}")
    if bad:
        print("O WINDOWS RECUSOU:", *bad, sep="\n  ")
        sys.exit(1)
    print("o Windows aceitou todos")


if __name__ == "__main__":
    main()
