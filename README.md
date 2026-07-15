# Rahul's Systems Blog

**Systems programming, worked out by hand.**

This repository contains the source text for the Systems Blog, designed with extreme pedagogical constraints:
- **Advanced Systems Only:** No low-hanging fruit. Deep technical dives into memory, threading, and system architecture.
- **Uncompromising Pedagogy:** Written to survive scrutiny from the Linux Kernel Mailing List (LKML) and C++ Committee, while remaining conceptually accessible to a 5-year-old or a 1980s mathematician with just a pencil.
- **Zero Markdown Headings:** Section transitions are handled entirely by ASCII diagrams and structural spacing, forcing visual clarity without relying on typographic crutches.
- **Evidence First:** No "magic happens here." Every assertion is backed by a concrete, runnable C++ proof.

## Architecture & DevOps

This is a static site generated via Python. 
- **Source of Truth:** All content lives as pure `.txt` files in `txt/posts/`. 
- **Deployment:** A local `ops.py` script stages content, runs strict audits (via `systems_audit.py` to enforce the above rules), and pushes to `main`. 
- **Hosting:** GitHub Actions automatically builds the HTML via `build.py` and deploys to GitHub Pages on every push.

*Note: You will not find standard markdown files in the posts directory. The extreme constraints of this blog mandate pure text layout over formatted markdown.*
