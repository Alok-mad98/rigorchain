# Manual Deployment — rigorchain.com

**Task:**
**Constraint:** No GitHub Actions or automated CI/CD YAML. Build locally; upload manually.

**Target:** https://www.rigorchain.com  
**Source repo:** https://github.com/rigorchain/rigorchain (`web/rigorchain-site/`)

## Prerequisites

- Node.js 18+ and npm
- Access to static host (GitHub Pages, nginx, S3+CloudFront, etc.)
- Public HTTPS endpoints for:
  - `VITE_RPC_PROXY_URL` — read-only rigord JSON-RPC proxy
  - `VITE_EXPLORER_API_URL` — explorer API base (e.g. `https://explorer.example.com`)

## 1. Configure environment

Create `.env.production` in `web/rigorchain-site/` (do not commit secrets; proxy URLs are public endpoints):

```bash
VITE_RPC_PROXY_URL=https://rpc.rigorchain.com/
VITE_EXPLORER_API_URL=https://explorer.rigorchain.com
VITE_VERIFICATION_MANIFEST_URL=https://raw.githubusercontent.com/rigorchain/rigorchain/main/docs/verification/manifest.json
VITE_SITE_VERSION=1.0.0
```

Adjust URLs to your deployed infrastructure. Without these variables the site still builds but shows **offline** / **degraded** states (no mock data).

## 2. Build

```bash
cd web/rigorchain-site
npm ci
npm run build
```

Artifacts land in `dist/`:

```
dist/
├── index.html
├── assets/
│   ├── index-*.js
│   └── index-*.css
└── CNAME          # www.rigorchain.com (from public/)
```

Verify locally:

```bash
npm run preview
# open http://localhost:4173
```

## 3. Deploy options

### Option A — Static upload (FTP / SFTP / web panel)

1. Upload entire contents of `dist/` to the web root for `www.rigorchain.com`.
2. Ensure `index.html` is the default document.
3. For SPA routing, configure the host to serve `index.html` for unknown paths (see Option B nginx example).

### Option B — rsync to server

```bash
rsync -avz --delete \
  dist/ \
  user@your-server:/var/www/rigorchain.com/
```

Example nginx snippet (`/etc/nginx/sites-available/rigorchain`):

```nginx
server {
    listen 443 ssl http2;
    server_name www.rigorchain.com rigorchain.com;
    root /var/www/rigorchain.com;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

Reload nginx after deploy: `sudo nginx -t && sudo systemctl reload nginx`.

### Option C — CDN manual push (S3 + CloudFront)

```bash
aws s3 sync dist/ s3://rigorchain-www/ --delete
aws cloudfront create-invalidation --distribution-id DISTRIBUTION_ID --paths "/*"
```

Set CloudFront alternate domain `www.rigorchain.com` and ACM certificate in us-east-1.

### GitHub Pages (manual, no Actions)

1. Build with `npm run build` as above.
2. Push `dist/` contents to `gh-pages` branch or to `docs/` on `main` (Pages source = branch/folder).
3. `public/CNAME` contains `www.rigorchain.com` — copied into `dist/CNAME` on build.
4. DNS: `CNAME www → <user>.github.io` or A records per GitHub docs.

**SPA 404 fix (GitHub Pages):** after build, copy index for client routing:

```bash
cp dist/index.html dist/404.html
```

Upload both `index.html` and `404.html`.

## 4. Post-deploy checks

| Check | Expected |
|-------|----------|
| https://www.rigorchain.com/ | Landing loads |
| `/protocol`, `/lab`, `/verification`, `/explorer` | Routes work (no 404 on refresh) |
| Status bar | `ONLINE` or explicit `OFFLINE`/`DEGRADED` — never fake data |
| `equivalence_claimed` badge | `false` |
| RPC proxy CORS | Browser can POST JSON-RPC from site origin |
| Explorer CORS | GET `/api/status` succeeds from browser |

## 5. Rollback

Keep previous `dist/` tarball:

```bash
tar czf rigorchain-site-$(date +%Y%m%d).tar.gz dist/
```

Restore by re-uploading the previous archive.

## 6. What this document does not cover

- No `.github/workflows/*.yml` — intentional per
- rigord / explorer server provisioning — separate ops
- TLS certificate issuance — host provider

## Build command reference

| Command | Purpose |
|---------|---------|
| `npm run dev` | Local dev server (:5173) |
| `npm run build` | Production `dist/` |
| `npm run preview` | Serve `dist/` locally |

**equivalence_claimed:** always `false` on the public site.
