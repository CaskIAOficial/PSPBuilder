# PSP Builder API

Cloudflare Worker backend for PSP Builder.

## Deploy with Cloudflare Workers Builds

Set the connected Worker build settings:
- Root directory: `worker`
- Build command: leave empty
- Deploy command: `npx wrangler deploy`

The Worker exposes:
- `GET /health` — health/configuration status
- `POST /compile` — reserved for the authenticated build pipeline (returns 503 until secrets are configured)

## Required Worker secrets (not repository files)

The GitHub App integration will require:
- `GITHUB_APP_ID`
- `GITHUB_INSTALLATION_ID`
- `GITHUB_PRIVATE_KEY`

Never put private keys or tokens in Git or in the Android APK.

The compile route intentionally remains disabled until the GitHub App installation and workflow pipeline are configured.
