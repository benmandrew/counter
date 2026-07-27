---
name: docs-build
description: Build the counter documentation site (Doxygen + Breathe + Sphinx public site, plus the nested internal reference). Use when running the docs target or debugging the docs build.
---

# Docs

```sh
cmake --build build --target docs   # Doxygen + Sphinx (requires both installed)
```

The `docs` target builds two things: the **curated public site** (`docs/Doxyfile.in` → XML → Breathe → Sphinx/furo, from `include/` only) and, nested under `internal/`, a **full internal reference** (`docs/Doxyfile.internal.in` → native Doxygen HTML, from `include/` + `src/`, with `EXTRACT_PRIVATE`/`EXTRACT_STATIC`/`EXTRACT_ALL`, source browsing and — when graphviz `dot` is present — call graphs). The public landing page links to it at `internal/index.html`. The internal build targets a persistent dir (`build/docs/internal`) and is copied into the site, so Doxygen's per-graph `.md5` cache survives across runs: the first/clean (CI) build renders every call graph and takes minutes, warm rebuilds take seconds.
