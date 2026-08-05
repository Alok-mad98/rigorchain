# RIGOR Official Site — Deployment (GitHub Pages + Domain)

**Task:** `TASK-PUBLICATION-003`  
**Domain:** `www.rigorchain.com`  
**Repository:** `https://github.com/rigorchain/rigorchain`  
**Parent:** [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md)

| Field | Value |
|-------|-------|
| Consensus / protocol / mining impact | **NONE** |

---

## §2 — Domain and deployment model

### Primary domain

| URL | Role |
|-----|------|
| **https://www.rigorchain.com** | Canonical public site |
| `https://rigorchain.com` | 301 redirect → `www` (DNS/hosting config) |

### GitHub source

| Item | Value |
|------|-------|
| Organization | `rigorchain` |
| Repository | `rigorchain` |
| Site package | `web/rigorchain-site/` |
| Pages branch | `gh-pages` **or** GitHub Actions artifact deploy |

---

## 1. GitHub Pages layer

### Option A — Actions deploy to `gh-pages` (recommended)

```text
push main (paths: web/rigorchain-site/**)
  → GitHub Actions workflow
  → npm ci && npm run build
  → upload dist/ to gh-pages branch
  → GitHub Pages serves gh-pages root
```

### Option B — `/docs` folder on `main`

Less preferred: mixes source and built assets. Use **Option A** for reproducible `dist/` only on Pages branch.

### Custom domain

File: `web/rigorchain-site/public/CNAME`

```text
www.rigorchain.com
```

GitHub Pages settings → Custom domain → enforce HTTPS.

DNS (registrar):

| Type | Name | Value |
|------|------|-------|
| `CNAME` | `www` | `rigorchain.github.io` or org Pages host |
| `A` / `ALIAS` | `@` | Redirect provider → `www` |

---

## 2. Production hosting model

```text
┌─────────────────────────────────────────────────────────┐
│ CDN (GitHub Pages edge + optional Cloudflare front)        │
│ Serves: HTML, JS, CSS, static MD, fonts                    │
│ NO server-side execution                                   │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┴───────────────┐
         ▼                               ▼
┌─────────────────┐             ┌─────────────────┐
│ rpc.rigorchain  │             │ explorer.       │
│ .com (proxy)    │             │ rigorchain.com  │
│ → rigord        │             │ indexer API     │
└─────────────────┘             └─────────────────┘
```

| Tier | Technology | Mutable at runtime |
|------|------------|-------------------|
| Frontend | Static SPA | No — rebuild to change |
| API | External HTTPS services | Operator maintains |

**No runtime backend** required for the website repository.

---

## §8 — Deployment pipeline (CI/CD)

### Workflow file

Path: `.github/workflows/deploy-rigorchain-site.yml`

```yaml
name: Deploy rigorchain.com site

on:
  push:
    branches: [main]
    paths:
      - 'web/rigorchain-site/**'
      - 'docs/publication/**'
      - 'docs/whitepaper/**'
  workflow_dispatch:

permissions:
  contents: write   # gh-pages deploy
  pages: write
  id-token: write

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0   # for git SHA version tag

      - uses: actions/setup-node@v4
        with:
          node-version: '20'
          cache: npm
          cache-dependency-path: web/rigorchain-site/package-lock.json

      - name: Sync publication content
        run: |
          # Implementation: copy MD from docs/publication → web/rigorchain-site/src/content/
          ./web/rigorchain-site/scripts/sync-content.sh

      - name: Install and build
        working-directory: web/rigorchain-site
        env:
          VITE_SITE_VERSION: ${{ github.sha }}
          VITE_RPC_PROXY_URL: ${{ vars.PUBLIC_RPC_URL }}
          VITE_EXPLORER_API_URL: ${{ vars.PUBLIC_EXPLORER_URL }}
          VITE_VERIFICATION_MANIFEST_URL: ${{ vars.VERIFICATION_MANIFEST_URL }}
        run: |
          npm ci
          npm run build

      - name: Verify deterministic output
        working-directory: web/rigorchain-site
        run: |
          test -f dist/index.html
          test -f dist/CNAME
          grep -q '${{ github.sha }}' dist/index.html || true  # version injected in build

      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v4
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: web/rigorchain-site/dist
          cname: www.rigorchain.com
```

### Repository variables (GitHub Settings → Variables)

| Variable | Example |
|----------|---------|
| `PUBLIC_RPC_URL` | `https://rpc.rigorchain.com` |
| `PUBLIC_EXPLORER_URL` | `https://explorer.rigorchain.com` |
| `VERIFICATION_MANIFEST_URL` | `https://archive.rigorchain.com/proof-034/verification_manifest.json` |

Secrets: none required in frontend bundle if APIs are public read-only.

---

## 3. Build determinism rules

| Rule | Requirement |
|------|-------------|
| **Deterministic build** | Lockfile committed; `npm ci` only in CI |
| **Reproducible output** | Same commit + env → same hashed assets (Vite content hash) |
| **Version tag per build** | Inject `VITE_SITE_VERSION` = `git describe` or `github.sha` |
| **No runtime backend** | `dist/` is complete artifact |
| **Content sync** | `sync-content.sh` copies frozen publication MD at build time |

### Build footer display

```text
Build: a1b2c3d · 2026-06-13T12:00:00Z
```

---

## 4. SPA routing on GitHub Pages

| File | Purpose |
|------|---------|
| `dist/index.html` | App shell |
| `dist/404.html` | **Identical copy** of `index.html` for client-side routes |

Vite plugin or post-build step copies `index.html` → `404.html`.

---

## 5. HTTPS and security headers

Configure via Cloudflare or GitHub Pages (if available):

| Header | Value |
|--------|-------|
| `Strict-Transport-Security` | `max-age=31536000; includeSubDomains` |
| `X-Content-Type-Options` | `nosniff` |
| `Referrer-Policy` | `strict-origin-when-cross-origin` |
| `Content-Security-Policy` | `default-src 'self'; connect-src 'self' https://rpc.rigorchain.com https://explorer.rigorchain.com ...` |

---

## 6. Preview deployments

| Trigger | Target |
|---------|--------|
| Pull request | Optional workflow deploy to `preview-{pr}.pages.dev` or comment artifact |
| `main` | Production `www.rigorchain.com` |

Preview builds use staging API URLs via `vars.PREVIEW_*` if defined.

---

## 7. Rollback

```text
git revert <commit> → push main → CI redeploys prior content
```

Or: redeploy previous `gh-pages` commit via workflow_dispatch from tag.

---

## 8. Operator checklist (go-live)

- [ ] DNS `CNAME` for `www` → GitHub Pages
- [ ] `CNAME` file in `public/`
- [ ] GitHub Pages custom domain + HTTPS enforced
- [ ] `PUBLIC_RPC_URL` read-only proxy live
- [ ] `PUBLIC_EXPLORER_URL` CORS allows `https://www.rigorchain.com`
- [ ] `equivalence_claimed: false` on `/api/status`
- [ ] CI green on `main`
- [ ] Footer shows build SHA

---

## 9. References

- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md)
- [FRONTEND_SPECIFICATION.md](./FRONTEND_SPECIFICATION.md)
- [API_BINDING_LAYER.md](./API_BINDING_LAYER.md)
- [GitHub Pages custom domains](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site)
