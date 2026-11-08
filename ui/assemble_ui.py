"""Assemblage de la page d'un plugin Open (utilisé par le build_ui.py de chaque plugin).

    from assemble_ui import assemble
    assemble(plugin_ui_dir, slug="openfilter", scripts=["app.js", "bridge.js", "init.js"])

Produit <plugin>/ui/<slug>_ui.html à partir de :
  - <plugin>/ui/src/plugin-head.html : <head>, CSS propre au plugin et corps de la page, jusqu'à la balise <script> ouverte.
    Le marqueur  /*__OPEN_CORE_CSS__*/  est remplacé par ui/open-base.css (tokens, composants communs, police Manrope).
  - ui/bridge-core.js (si with_bridge) puis les scripts du plugin, dans l'ordre donné.

La police Manrope (SIL OFL) est intégrée en base64 : le WebView est hors ligne.
"""
import base64
from pathlib import Path

CORE_UI = Path(__file__).resolve().parent
CSS_MARKER = "/*__OPEN_CORE_CSS__*/"
FONT_MARKER = "__MANROPE__"


def check_css_comments(css, origin):
    """Un commentaire CSS ne peut pas en contenir un autre : le premier « */ » le ferme, et ce qui suit devient du CSS
    invalide que le navigateur avale avec la règle suivante (cas vécu : la règle @font-face disparaissait)."""
    pos = 0
    while True:
        start = css.find("/*", pos)
        if start < 0:
            return
        end = css.find("*/", start + 2)
        if end < 0:
            raise SystemExit(f"{origin} : commentaire CSS jamais refermé (ouvert au caractère {start})")
        if "/*" in css[start + 2:end]:
            line = css.count("\n", 0, start) + 1
            raise SystemExit(f"{origin} : commentaire CSS imbriqué (ligne {line}) : retire le « /* » écrit dans un commentaire")
        pos = end + 2


def assemble(plugin_ui_dir, slug, scripts, with_bridge=True):
    plugin_ui = Path(plugin_ui_dir)
    src = plugin_ui / "src"

    head = (src / "plugin-head.html").read_text(encoding="utf-8")
    assert CSS_MARKER in head, f"marqueur {CSS_MARKER} introuvable dans plugin-head.html"

    css = (CORE_UI / "open-base.css").read_text(encoding="utf-8")
    font = base64.b64encode((CORE_UI / "manrope-latin.woff2").read_bytes()).decode("ascii")
    assert FONT_MARKER in css, f"marqueur {FONT_MARKER} introuvable dans open-base.css"
    css = css.replace(FONT_MARKER, font)
    check_css_comments(css, "open-base.css")
    head = head.replace(CSS_MARKER, css)
    check_css_comments(head[head.index("<style>"):head.index("</style>")], "plugin-head.html (après assemblage)")

    parts = []
    if with_bridge:
        parts.append((CORE_UI / "bridge-core.js").read_text(encoding="utf-8"))
    parts += [(src / name).read_text(encoding="utf-8") for name in scripts]

    html = head + "\n".join(parts) + "\n</script>\n</body>\n</html>\n"
    out = plugin_ui / f"{slug}_ui.html"
    out.write_text(html, encoding="utf-8", newline="\n")
    print(f"ui/{slug}_ui.html :", len(html), "octets")
    return out
