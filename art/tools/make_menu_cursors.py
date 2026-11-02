# Gera os cursores dos menus (art/cursors/menu.html, a direção 3 escolhida em 01/11): cada um em 48, 64 e 96 px, como .cur
# (o jogo, Game/Content/Cursors/<px>/menu_*.cur) e como .png (o protótipo HTML do menu: cursor: url(menu_pointer.png) 7 5, auto).
# Usa o mesmo .cur e a mesma conferência no Windows do make_cursors.py.
# Uso: python art/tools/make_menu_cursors.py
import html, io, json, os, re, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_cursors import EDGE, ROOT, OUT, edge, cur_bytes, ani_bytes, windows_accepts

PAGE = "file:///" + os.path.join(ROOT, "art", "cursors", "menu.html").replace("\\", "/")
PNG = os.path.join(ROOT, "art", "cursors", "menu_png")


def main():
    dom = edge(["--dump-dom", PAGE]).stdout.decode("utf-8", "replace")
    man = json.loads(html.unescape(re.search(r'<pre id="manifest"[^>]*>(.*?)</pre>', dom, re.S).group(1)))
    width = len(man["cursors"]) * 112 + 16
    sheets = {}
    for t in sorted({round(k / c["fps"], 4) for c in man["cursors"] for k in range(c["frames"])}):
        shot = os.path.join(ROOT, "captures", "concept", "cursores", f"menu_export_{t:.4f}.png")
        edge(["--default-background-color=00000000", f"--window-size={width},264", f"--screenshot={shot}", PAGE + f"#t={t:.4f}"])
        sheets[t] = Image.open(shot).convert("RGBA")
    bad = []
    for b in man["bands"]:
        px = b["px"]
        os.makedirs(os.path.join(OUT, str(px)), exist_ok=True)
        os.makedirs(os.path.join(PNG, str(px)), exist_ok=True)
        for c in man["cursors"]:
            hot = (round(c["hot"][0] * px / 64), round(c["hot"][1] * px / 64))
            curs = []
            for k in range(c["frames"]):
                im = sheets[round(k / c["fps"], 4)].crop((c["x"], b["top"], c["x"] + px, b["top"] + px))
                if k == 0:
                    im.save(os.path.join(PNG, str(px), c["name"] + ".png"))
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                curs.append(cur_bytes(buf.getvalue(), px, hot))
            ext = ".ani" if len(curs) > 1 else ".cur"
            path = os.path.join(OUT, str(px), c["name"] + ext)
            open(path, "wb").write(ani_bytes(curs, 3) if len(curs) > 1 else curs[0])
            if not windows_accepts(path):
                bad.append(path)
            print(f"{px} {c['name']}: clique em {hot}")
    if bad:
        print("O WINDOWS RECUSOU:", *bad, sep="\n  ")
        sys.exit(1)
    print("o Windows aceitou todos")


if __name__ == "__main__":
    main()
