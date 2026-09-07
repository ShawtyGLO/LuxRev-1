"""Assemble les maquettes autonomes depuis src/ vers maquettes/.

⚠️ Pourquoi un assembleur plutot qu'un simple <link href="commun.css"> :
les maquettes sont envoyees et ouvertes comme des fichiers isoles (panneau
d'apercu, navigateur, partage). Une ressource relative ne s'y resout pas —
la page arrive alors sans style, ce qui est pire qu'une duplication de code.

On garde donc UNE source (commun.css, commun.js, mesures.js) et on produit
des fichiers autonomes. Relancer apres toute modification d'un commun :

    python maquettes/assembler.py
"""

from pathlib import Path
import re

ICI = Path(__file__).parent
SRC = ICI / "src"


def inline(html):
    css = (ICI / "commun.css").read_text(encoding="utf-8")
    html = html.replace('<link rel="stylesheet" href="commun.css">',
                        "<style>\n" + css + "\n</style>")
    for nom in ("mesures.js", "commun.js"):
        js = (ICI / nom).read_text(encoding="utf-8")
        html = html.replace(f'<script src="{nom}"></script>',
                            "<script>\n" + js + "\n</script>")
    reste = re.findall(r'<(?:script src|link[^>]*href)="([^"]+)"', html)
    if reste:
        raise SystemExit(f"ressource non inlinee : {reste}")
    return html


if __name__ == "__main__":
    for f in sorted(SRC.glob("*.html")):
        (ICI / f.name).write_text(inline(f.read_text(encoding="utf-8")), encoding="utf-8")
        print(f"  {f.name}  ->  maquettes/{f.name}")
