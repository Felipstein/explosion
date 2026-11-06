# (04/11) Os cursores de girar a obra (a B, que gira junto com o arraste): art/cursors/rotate.html desenha as 24 posições de 15°
# nas faixas de 48, 64 e 96 px; aqui viram Game/Content/Cursors/<px>/rotate_00..23.cur (o ponto do clique no meio, 32/64 da grade).
# Os escritores do .cur e a conferência do Windows são os do make_cursors.py.
import html, io, json, os, re, sys
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_cursors import EDGE, OUT, ROOT, cur_bytes, edge, windows_accepts

BOARD = "file:///" + os.path.join(ROOT, "art", "cursors", "rotate.html").replace("\\", "/")
WORK = os.path.join(ROOT, "captures", "concept", "cursores", "export_rotate")


def main():
    os.makedirs(WORK, exist_ok=True)
    dom = edge(["--dump-dom", BOARD]).stdout
    dom = dom.decode("utf-8") if isinstance(dom, bytes) else dom
    m = json.loads(html.unescape(re.search(r'<pre id="manifest"[^>]*>(.*?)</pre>', dom, re.S).group(1)))
    width = max(c["x"] for c in m["cursors"]) + 120
    shot = os.path.join(WORK, "rotate.png")
    edge(["--default-background-color=00000000", f"--window-size={width},260", f"--screenshot={shot}", BOARD])
    sheet = Image.open(shot).convert("RGBA")
    count = 0
    for band in m["bands"]:
        px, top = band["px"], band["top"]
        os.makedirs(os.path.join(OUT, str(px)), exist_ok=True)
        for c in m["cursors"]:
            img = sheet.crop((c["x"], top, c["x"] + px, top + px))
            buf = io.BytesIO()
            img.save(buf, "PNG")
            hot = (round(c["hot"][0] * px / 64), round(c["hot"][1] * px / 64))
            path = os.path.join(OUT, str(px), c["name"] + ".cur")
            with open(path, "wb") as fh:
                fh.write(cur_bytes(buf.getvalue(), px, hot))
            if not windows_accepts(path):
                raise SystemExit(f"o Windows recusou {path}")
            count += 1
    print(f"{count} cursores de girar; o Windows aceitou todos")


if __name__ == "__main__":
    main()
