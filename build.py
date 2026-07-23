#!/usr/bin/env python3
"""
Plain-text blog builder.

Source of truth = .txt files under txt/.  Each is wrapped in a minimal <pre>
HTML page so it renders like a kernel.org / LKML plain-text page in any browser,
while staying readable as raw text in a terminal (curl | less).

Conventions inside a .txt file:
  - First line may be:  @title Some Page Title   (used for <title>, then removed)
  - Optional next line: @desc One-line description (for search engines / shares).
                        If absent, a description is pulled from the index blurb
                        (for posts) or the first prose paragraph (other pages).
  - Links:              [label](target.txt)  or  [label](https://...)  or
                        [label](mailto:a@b.c)
                        A .txt target is rewritten to .html automatically.
  - Everything else is literal text. Authored to <= 100 columns.

Run:  python3 build.py
Outputs, next to where each .txt maps (txt/index.txt -> index.html,
txt/posts/foo.txt -> posts/foo.html), plus, at the repo root:
  sitemap.xml   every page, for search engines
  feed.xml      RSS 2.0 feed of the posts, in reading order
No JavaScript, no external CSS, no web fonts, no frameworks -- on purpose: one
HTTP request per page, a few hundred bytes of inline style, readable on GPRS.
"""

import html
import re
from datetime import datetime, timezone
from email.utils import format_datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "txt"

# EDIT THIS if your GitHub repo is not named "systems_blog" or your user is not
# "learnrahulrai-ui": the URL is https://<user>.github.io/<repo-name>.
BASE_URL = "https://learnrahulrai-ui.github.io/systems_blog"
SITE_NAME = "Rahul's Systems Blog"
AUTHOR = "Rahul Rai"
SITE_DESC = (
    "Systems programming worked out by hand: sockets, epoll, mmap and the kernel, "
    "built one failing version at a time until it holds. Every crash reproduced, "
    "every fix earned -- no hand-waving, no frameworks, no JavaScript."
)

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
DATE_RE = re.compile(r"\b(\d{4}-\d{2}-\d{2})\b")

# The entire stylesheet. One rule: cap and centre the 100-column frame so it
# does not hug the left edge on a wide screen. Dark mode is HTML, not CSS (the
# color-scheme meta below), so it costs zero bytes. Everything else -- monospace,
# preserved whitespace, no wrapping -- is already the browser default for <pre>.
# Inlined, so the page is a single HTTP request: readable on a GPRS connection.
STYLE = ("pre{max-width:100ch;margin:0 auto;padding:1em;font-size:1.5em}"
         "code.hl{display:block;background:rgba(128,128,128,.12);"
         "border-left:3px solid rgba(128,128,128,.5);padding:.35em .7em;margin:.4em 0}")

TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="canonical" href="{url}">
<link rel="alternate" type="application/rss+xml" title="{site_name}" href="{feed}">
<style>{style}</style>
</head>
<body>
<main>
<pre>{body}</pre>
</main>
</body>
</html>
"""


def render_links(text: str) -> str:
    """Replace [label](target) with <a> tags. .txt targets -> .html."""
    def repl(m: "re.Match[str]") -> str:
        label, target = m.group(1), m.group(2)
        if target.endswith(".txt"):
            target = target[:-4] + ".html"
        return f'<a href="{target}">{label}</a>'
    return LINK_RE.sub(repl, text)


def strip_markup(text: str) -> str:
    """[label](target) -> label, for plain-text descriptions."""
    return LINK_RE.sub(lambda m: m.group(1), text)


def first_prose(body_text: str) -> str:
    """First real sentence-ish paragraph, skipping rules/nav/path/headers."""
    para: list[str] = []
    for line in body_text.split("\n"):
        s = line.strip()
        if not s:
            if para:
                break
            continue
        # skip structural lines
        if set(s) <= set("=-_ "):
            continue
        if s.startswith(("@", "PATH", "[home]", ">>", "!!", "**")):
            continue
        if "[home]" in s or s.startswith(("<- prev", "prev:", "next:")):
            continue
        if s.isupper():          # all-caps banner lines
            continue
        if DATE_RE.search(s) and ("Posted:" in s or " . " in s):
            continue
        para.append(strip_markup(s))
    text = " ".join(para)
    text = re.sub(r"\s+", " ", text).strip()
    if len(text) > 200:
        text = text[:200].rsplit(" ", 1)[0] + "..."
    return text


def parse_index_blurbs() -> dict[str, dict]:
    """Map posts/foo.html -> {desc, date} using the curated index listing."""
    idx = SRC / "index.txt"
    if not idx.is_file():
        return {}
    lines = idx.read_text(encoding="utf-8").split("\n")
    out: dict[str, dict] = {}
    i = 0
    link_to_post = re.compile(r"\[[^\]]+\]\((posts/[^)]+\.txt)\)")
    while i < len(lines):
        m = link_to_post.search(lines[i])
        if not m:
            i += 1
            continue
        key = m.group(1)[:-4] + ".html"        # posts/foo.html
        i += 1
        date = ""
        blurb: list[str] = []
        # next non-blank line is the metadata line (post N/M . date . tags)
        if i < len(lines):
            dm = DATE_RE.search(lines[i])
            if dm:
                date = dm.group(1)
            i += 1
        # collect blurb lines until a blank line
        while i < len(lines) and lines[i].strip():
            if link_to_post.search(lines[i]):
                break
            blurb.append(lines[i].strip())
            i += 1
        text = re.sub(r"\s+", " ", " ".join(blurb)).strip()
        if len(text) > 280:
            text = text[:280].rsplit(" ", 1)[0] + "..."
        out[key] = {"desc": text, "date": date}
    return out


def build_one(txt_path: Path, blurbs: dict[str, dict]) -> dict:
    raw = txt_path.read_text(encoding="utf-8")
    lines = raw.split("\n")

    title = txt_path.stem
    if lines and lines[0].startswith("@title "):
        title = lines[0][len("@title "):].strip()
        lines = lines[1:]
    desc = ""
    if lines and lines[0].startswith("@desc "):
        desc = lines[0][len("@desc "):].strip()
        lines = lines[1:]
    body_text = "\n".join(lines)

    rel = txt_path.relative_to(SRC)
    out_rel = rel.with_suffix(".html").as_posix()
    is_post = out_rel.startswith("posts/")

    info = blurbs.get(out_rel, {})
    if not desc:
        desc = info.get("desc") or first_prose(body_text) or SITE_DESC

    # Escape HTML special chars first, then lay links over the escaped text.
    escaped = html.escape(body_text, quote=False)
    # Wrap ```lang ... ``` fenced blocks in a tinted <code> so code stands out.
    escaped = re.sub(
        r"(?ms)^```[^\n]*\n(.*?)\n```[ \t]*$",
        lambda m: '<code class="hl">' + m.group(1) + "</code>",
        escaped,
    )
    body = render_links(escaped)

    out_path = ROOT / rel.with_suffix(".html")
    depth = len(rel.parts) - 1
    prefix = "../" * depth
    url = f"{BASE_URL}/{out_rel}"

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        TEMPLATE.format(
            title=html.escape(title, quote=True),
            desc=html.escape(desc, quote=True),
            site_name=html.escape(SITE_NAME, quote=True),
            url=html.escape(url, quote=True),
            feed=prefix + "feed.xml",
            style=STYLE,
            body=body,
        ),
        encoding="utf-8",
    )
    return {
        "rel": out_rel,
        "url": url,
        "title": title,
        "desc": desc,
        "date": info.get("date", ""),
        "is_post": is_post,
    }


def write_sitemap(pages: list[dict]) -> None:
    today = datetime.now(timezone.utc).date().isoformat()
    rows = []
    for p in pages:
        if p["rel"] == "404.html":      # error page: keep out of the index
            continue
        lastmod = p["date"] or today
        rows.append(
            "  <url>\n"
            f"    <loc>{html.escape(p['url'])}</loc>\n"
            f"    <lastmod>{lastmod}</lastmod>\n"
            "  </url>"
        )
    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        + "\n".join(rows)
        + "\n</urlset>\n"
    )
    (ROOT / "sitemap.xml").write_text(xml, encoding="utf-8")


def write_feed(pages: list[dict]) -> None:
    """RSS 2.0 feed of the posts, newest first."""
    posts = [p for p in pages if p["is_post"] and p["date"]]

    def keyfn(p: dict):
        return p["date"]

    posts.sort(key=keyfn, reverse=True)
    now = format_datetime(datetime.now(timezone.utc))
    items = []
    for p in posts:
        try:
            dt = datetime.strptime(p["date"], "%Y-%m-%d").replace(
                tzinfo=timezone.utc
            )
            pub = format_datetime(dt)
        except ValueError:
            pub = now
        items.append(
            "    <item>\n"
            f"      <title>{html.escape(p['title'])}</title>\n"
            f"      <link>{html.escape(p['url'])}</link>\n"
            f"      <guid isPermaLink=\"true\">{html.escape(p['url'])}</guid>\n"
            f"      <pubDate>{pub}</pubDate>\n"
            f"      <description>{html.escape(p['desc'])}</description>\n"
            "    </item>"
        )
    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<rss version="2.0">\n'
        "  <channel>\n"
        f"    <title>{html.escape(SITE_NAME)}</title>\n"
        f"    <link>{BASE_URL}/</link>\n"
        f"    <description>{html.escape(SITE_DESC)}</description>\n"
        "    <language>en</language>\n"
        f"    <lastBuildDate>{now}</lastBuildDate>\n"
        f'    <atom:link xmlns:atom="http://www.w3.org/2005/Atom" '
        f'href="{BASE_URL}/feed.xml" rel="self" type="application/rss+xml"/>\n'
        + "\n".join(items)
        + "\n  </channel>\n</rss>\n"
    )
    (ROOT / "feed.xml").write_text(xml, encoding="utf-8")


def main() -> None:
    if not SRC.is_dir():
        raise SystemExit(f"no source dir: {SRC}")
    blurbs = parse_index_blurbs()
    pages = [build_one(p, blurbs) for p in sorted(SRC.rglob("*.txt"))]
    for p in pages:
        print(f"built {p['rel']}")
    write_sitemap(pages)
    print("built sitemap.xml")
    write_feed(pages)
    print("built feed.xml")
    print(f"\n{len(pages)} page(s) built + sitemap + feed.")


if __name__ == "__main__":
    main()
