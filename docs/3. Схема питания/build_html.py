#!/usr/bin/env python3
"""Generate static HTML page from Схема питания.md."""

import re
import shutil
import subprocess
from pathlib import Path

BASE = Path(__file__).resolve().parent
MD_FILE = BASE / "Схема питания.md"
HTML_DIR = BASE / "html"
MEDIA_SRC = BASE / "media"
COMPONENTS_SRC = BASE.parent / "Список компонентов" / "components"

COMPONENT_IMAGES = [
    "battery.jpg",
    "buzzer.jpg",
    "dc_dc_buck_10a.jpg",
    "imax_b6ac.jpg",
    "off_on_switch.jpg",
    "xt_plugs.jpg",
]


def extract_toc(md_text: str) -> list[tuple[int, str, str]]:
    """Parse <!-- TOC --> block into (level, title, anchor) tuples."""
    match = re.search(r"<!-- TOC -->(.*?)<!-- TOC -->", md_text, re.DOTALL)
    if not match:
        return []
    items = []
    for line in match.group(1).splitlines():
        m = re.match(r"^(\s*)\*\s+\[([^\]]+)\]\((#[^)]+)\)", line)
        if m:
            level = len(m.group(1)) // 2 + 1
            anchor = m.group(3)
            if anchor.endswith("-"):
                anchor = anchor[:-1]
            items.append((level, m.group(2), anchor))
    return items


def build_nav_html(toc: list[tuple[int, str, str]]) -> str:
    def render(start: int, parent_level: int) -> tuple[list[str], int]:
        parts: list[str] = []
        i = start
        while i < len(toc):
            level, title, anchor = toc[i]
            if parent_level and level <= parent_level:
                break
            parts.append(
                f'<li class="toc-l{level}"><a href="{anchor}">{title}</a>'
            )
            i += 1
            if i < len(toc) and toc[i][0] > level:
                parts.append("<ul>")
                sub, i = render(i, level)
                parts.extend(sub)
                parts.append("</ul>")
            parts.append("</li>")
        return parts, i

    items, _ = render(0, 0)
    return (
        '<nav class="sidebar-nav" aria-label="Оглавление">\n<ul>\n'
        + "\n".join(items)
        + "\n</ul>\n</nav>"
    )


def preprocess_md(md_text: str) -> str:
    text = re.sub(r"<!-- TOC -->.*?<!-- TOC -->\s*", "", md_text, flags=re.DOTALL)
    text = text.replace(
        "../%D0%A1%D0%BF%D0%B8%D1%81%D0%BE%D0%BA%20%D0%BA%D0%BE%D0%BC%D0%BF%D0%BE%D0%BD%D0%B5%D0%BD%D1%82%D0%BE%D0%B2/components/",
        "components/",
    )
    text = text.replace(
        "../Список компонентов/components/",
        "components/",
    )
    # Pandoc needs a blank line before bullet lists.
    text = re.sub(
        r"([^\n])\n((?:\* |\- )https?://)",
        r"\1\n\n\2",
        text,
    )
    return text


def convert_md_to_html(md_body: str) -> str:
    proc = subprocess.run(
        [
            "pandoc",
            "-f",
            "markdown+autolink_bare_uris",
            "-t",
            "html5",
            "--wrap=none",
        ],
        input=md_body,
        capture_output=True,
        text=True,
        check=True,
    )
    return proc.stdout


def sync_toc_anchors(
    toc: list[tuple[int, str, str]], body_html: str
) -> list[tuple[int, str, str]]:
    """Align TOC anchors with pandoc heading ids (e.g. '3D Модель' -> d-модель)."""
    heading_re = re.compile(r'<h[1-6]\s+id="([^"]+)"[^>]*>([^<]+)</h[1-6]>')
    title_to_id = {title.strip(): hid for hid, title in heading_re.findall(body_html)}

    synced = []
    for level, title, anchor in toc:
        aid = anchor.lstrip("#")
        if f'id="{aid}"' not in body_html and title in title_to_id:
            anchor = f"#{title_to_id[title]}"
        synced.append((level, title, anchor))
    return synced


def main() -> None:
    md_text = MD_FILE.read_text(encoding="utf-8")
    body_html = convert_md_to_html(preprocess_md(md_text))
    toc = sync_toc_anchors(extract_toc(md_text), body_html)
    nav_html = build_nav_html(toc)

    if HTML_DIR.exists():
        shutil.rmtree(HTML_DIR)
    HTML_DIR.mkdir(parents=True)
    (HTML_DIR / "media").mkdir()
    (HTML_DIR / "components").mkdir()

    if MEDIA_SRC.is_dir():
        shutil.copytree(MEDIA_SRC, HTML_DIR / "media", dirs_exist_ok=True)

    for name in COMPONENT_IMAGES:
        src = COMPONENTS_SRC / name
        if src.is_file():
            shutil.copy2(src, HTML_DIR / "components" / name)

    css = """\
:root {
  --sidebar-width: 280px;
  --bg: #f8f9fa;
  --sidebar-bg: #1e293b;
  --sidebar-text: #e2e8f0;
  --sidebar-hover: #334155;
  --accent: #3b82f6;
  --text: #1e293b;
  --border: #e2e8f0;
}

* {
  box-sizing: border-box;
}

html {
  scroll-behavior: smooth;
}

body {
  margin: 0;
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
  font-size: 16px;
  line-height: 1.6;
  color: var(--text);
  background: var(--bg);
}

.layout {
  display: flex;
  min-height: 100vh;
}

.sidebar {
  position: fixed;
  top: 0;
  left: 0;
  width: var(--sidebar-width);
  height: 100vh;
  overflow-y: auto;
  background: var(--sidebar-bg);
  color: var(--sidebar-text);
  z-index: 100;
  box-shadow: 2px 0 8px rgba(0, 0, 0, 0.15);
}

.sidebar-header {
  padding: 1.25rem 1rem 1rem;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
  font-weight: 600;
  font-size: 1.05rem;
  line-height: 1.35;
}

.sidebar-nav {
  padding: 0.75rem 0 1.5rem;
}

.sidebar-nav ul {
  list-style: none;
  margin: 0;
  padding: 0;
}

.sidebar-nav > ul > li > a {
  font-weight: 600;
}

.sidebar-nav a {
  display: block;
  padding: 0.35rem 1rem;
  color: var(--sidebar-text);
  text-decoration: none;
  font-size: 0.875rem;
  line-height: 1.4;
  border-left: 3px solid transparent;
  transition: background 0.15s, color 0.15s;
}

.sidebar-nav a:hover,
.sidebar-nav a:focus {
  background: var(--sidebar-hover);
  color: #fff;
}

.sidebar-nav .toc-l2 a { padding-left: 1.5rem; font-weight: 500; }
.sidebar-nav .toc-l3 a { padding-left: 2rem; font-size: 0.8125rem; }
.sidebar-nav .toc-l4 a { padding-left: 2.75rem; font-size: 0.8rem; opacity: 0.9; }
.sidebar-nav .toc-l5 a { padding-left: 3.5rem; font-size: 0.75rem; opacity: 0.85; }

.content {
  margin-left: var(--sidebar-width);
  flex: 1;
  padding: 2rem 2.5rem 3rem;
  max-width: 960px;
}

.content h1,
.content h2,
.content h3,
.content h4,
.content h5,
.content h6 {
  scroll-margin-top: 1.5rem;
  line-height: 1.3;
}

.content h2 {
  margin-top: 2.5rem;
  padding-bottom: 0.35rem;
  border-bottom: 2px solid var(--accent);
}

.content h3 { margin-top: 1.75rem; }
.content h4 { margin-top: 1.25rem; }

.content table {
  width: 100%;
  border-collapse: collapse;
  margin: 1rem 0 1.5rem;
  font-size: 0.9rem;
  background: #fff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
}

.content th,
.content td {
  border: 1px solid var(--border);
  padding: 0.5rem 0.75rem;
  text-align: left;
}

.content th {
  background: #f1f5f9;
  font-weight: 600;
}

.content tr:nth-child(even) td {
  background: #fafbfc;
}

.content img {
  max-width: 100%;
  height: auto;
  margin: 1rem 0;
  border-radius: 4px;
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.1);
}

.content p {
  margin: 0.75rem 0;
}

.content ul,
.content ol {
  margin: 0.75rem 0;
  padding-left: 1.5rem;
}

.content a {
  color: var(--accent);
}

.content > :first-child {
  margin-top: 0;
}

@media (max-width: 900px) {
  .sidebar {
    position: relative;
    width: 100%;
    height: auto;
    max-height: 40vh;
  }
  .content {
    margin-left: 0;
    padding: 1.5rem 1rem 2rem;
  }
  .layout {
    flex-direction: column;
  }
}
"""

    (HTML_DIR / "style.css").write_text(css, encoding="utf-8")

    page = f"""<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Схема питания — Turtle Bot</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="layout">
    <aside class="sidebar">
      <div class="sidebar-header">Схема питания</div>
      {nav_html}
    </aside>
    <main class="content">
      {body_html}
    </main>
  </div>
</body>
</html>
"""

    (HTML_DIR / "index.html").write_text(page, encoding="utf-8")
    print(f"Created {HTML_DIR / 'index.html'}")


if __name__ == "__main__":
    main()
