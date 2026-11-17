# PSP Builder API

Cloudflare Worker backend for the Android PSP Builder app.

## Automatic deployment

The GitHub Actions workflow `.github/workflows/deploy-worker.yml` deploys this Worker when files under `worker/` change, and can also be started manually.

Configure these **GitHub Actions repository secrets** once:

- `CLOUDFLARE_API_TOKEN`: Cloudflare API token with permission to edit/deploy Workers for this account.
- `CLOUDFLARE_ACCOUNT_ID`: Cloudflare account ID.

The Wrangler configuration deploys the Worker under the existing name `pspbuilder`, preserving the public URL:
`https://pspbuilder.caskiaoficial.workers.dev`.

## Required Cloudflare Worker secrets

These secrets must remain configured on the Cloudflare Worker itself:

- `GITHUB_APP_ID`
- `GITHUB_INSTALLATION_ID`
- `GITHUB_PRIVATE_KEY`

Never put private keys or tokens in Git or in the Android APK. The `/health` endpoint only confirms whether the three secret values exist; it does not validate GitHub authentication. A real compile request is required to verify the integration.

## API routes

- `GET /health` — service status and presence of required secrets.
- `POST /compile` — uploads the project source and dispatches the PSPDEV workflow.
- `GET /status/:id` — checks the compilation status.
- `GET /result/:id` — downloads the build artifact.
