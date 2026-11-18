/**
 * PSP Builder API — Cloudflare Worker
 * GitHub App credentials stay in Cloudflare secrets and never reach Android.
 */
const OWNER = "CaskIAOficial";
const REPO = "PSPBuilder";
const BRANCH = "main";
const API = "https://api.github.com";

function b64url(value) {
  const bytes = typeof value === "string" ? new TextEncoder().encode(value) : value;
  let binary = "";
  for (const b of bytes) binary += String.fromCharCode(b);
  return btoa(binary).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}
function derLength(n) {
  if (n < 128) return new Uint8Array([n]);
  const bytes = [];
  while (n) { bytes.unshift(n & 255); n >>>= 8; }
  return new Uint8Array([0x80 | bytes.length, ...bytes]);
}
function joinBytes(...arrays) {
  const out = new Uint8Array(arrays.reduce((n, a) => n + a.length, 0));
  let offset = 0;
  for (const a of arrays) { out.set(a, offset); offset += a.length; }
  return out;
}
function der(tag, content) {
  return joinBytes(new Uint8Array([tag]), derLength(content.length), content);
}
function pemToPkcs8(pem) {
  const clean = pem.replace(/-----BEGIN [^-]+-----|-----END [^-]+-----|\s/g, "");
  const raw = Uint8Array.from(atob(clean), c => c.charCodeAt(0));
  if (pem.includes("BEGIN PRIVATE KEY")) return raw;
  const version = new Uint8Array([0x02, 0x01, 0x00]);
  const algorithm = new Uint8Array([0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00]);
  return der(0x30, joinBytes(version, algorithm, der(0x04, raw)));
}
async function createAppJwt(env) {
  const now = Math.floor(Date.now() / 1000);
  const header = b64url(JSON.stringify({ alg: "RS256", typ: "JWT" }));
  const payload = b64url(JSON.stringify({ iat: now - 60, exp: now + 540, iss: String(env.GITHUB_APP_ID) }));
  const input = header + "." + payload;
  const key = await crypto.subtle.importKey("pkcs8", pemToPkcs8(env.GITHUB_PRIVATE_KEY),
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["sign"]);
  const signature = await crypto.subtle.sign("RSASSA-PKCS1-v1_5", key, new TextEncoder().encode(input));
  return input + "." + b64url(new Uint8Array(signature));
}
let cachedInstallationToken = null;
let cachedInstallationTokenExpiresAt = 0;
let installationTokenPromise = null;

async function getInstallationToken(env) {
  if (cachedInstallationToken && Date.now() < cachedInstallationTokenExpiresAt - 5 * 60 * 1000) {
    return cachedInstallationToken;
  }
  if (!installationTokenPromise) {
    installationTokenPromise = (async () => {
      const jwt = await createAppJwt(env);
      const response = await fetch(API + "/app/installations/" + env.GITHUB_INSTALLATION_ID + "/access_tokens", {
        method: "POST",
        headers: { "User-Agent": "PSPBuilder-Cloudflare-Worker", Authorization: "Bearer " + jwt, Accept: "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28" }
      });
      if (!response.ok) throw new Error("GitHub App no pudo obtener el token de instalación: " + response.status + " " + await response.text());
      const data = await response.json();
      cachedInstallationToken = data.token;
      cachedInstallationTokenExpiresAt = data.expires_at ? Date.parse(data.expires_at) : Date.now() + 50 * 60 * 1000;
      return cachedInstallationToken;
    })().finally(() => { installationTokenPromise = null; });
  }
  return installationTokenPromise;
}

async function github(env, path, options = {}) {
  const token = await getInstallationToken(env);
  return fetch(API + "/repos/" + OWNER + "/" + REPO + path, {
    ...options,
    headers: {
      "User-Agent": "PSPBuilder-Cloudflare-Worker",
      Accept: "application/vnd.github+json",
      Authorization: "Bearer " + token,
      "X-GitHub-Api-Version": "2022-11-28",
      ...(options.body ? { "Content-Type": "application/json" } : {}),
      ...(options.headers || {})
    }
  });
}
function json(data, status = 200, cors = {}) {
  return new Response(JSON.stringify(data), { status, headers: { ...cors, "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store" } });
}
export default {
  async fetch(request, env) {
    const cors = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type,Authorization",
      "Access-Control-Max-Age": "86400"
    };
    if (request.method === "OPTIONS") return new Response(null, { headers: cors });
    const url = new URL(request.url);
    const configured = Boolean(env.GITHUB_APP_ID && env.GITHUB_INSTALLATION_ID && env.GITHUB_PRIVATE_KEY);
    if (url.pathname === "/" || url.pathname === "/health") {
      return json({ service: "PSP Builder API", status: "online", version: "1.1.1", configured,
        message: configured ? "Secretos detectados. La autenticación con GitHub se verificará al iniciar una compilación." : "Faltan secretos de GitHub App." }, 200, cors);
    }
    if (!configured) return json({ error: "SERVER_NOT_CONFIGURED", message: "Faltan las credenciales de GitHub App en Cloudflare." }, 503, cors);
    try {
      if (url.pathname === "/compile" && request.method === "POST") {
        const payload = await request.json();
        const files = payload.files;
        if (!Array.isArray(files) || files.length < 1 || files.length > 100)
          return json({ error: "INVALID_FILES", message: "El proyecto debe incluir entre 1 y 100 archivos." }, 400, cors);
        const requestId = crypto.randomUUID();
        const projectPath = "psp-project/mobile-" + requestId;
        let total = 0;
        for (const file of files) {
          if (!file || typeof file.path !== "string" || typeof file.content !== "string" ||
              file.path.startsWith("/") || file.path.split("/").some(p => !p || p === "." || p === "..") ||
              file.path.includes("\\") || file.path.length > 240)
            return json({ error: "INVALID_PATH", message: "El ZIP contiene una ruta de archivo no válida." }, 400, cors);
          const size = Math.floor(file.content.length * 3 / 4);
          total += size;
          if (size > 900 * 1024 || total > 20 * 1024 * 1024)
            return json({ error: "PROJECT_TOO_LARGE", message: "Límite: 20 MB en total y 900 KB por archivo." }, 413, cors);
        }
        // Subida optimizada: crear blobs en paralelo y guardar todos los archivos
        // en un único árbol/commit. Evita 90 commits y la carrera 409 de Contents API.
        const blobEntries = [];
        for (let i = 0; i < files.length; i += 8) {
          const batch = files.slice(i, i + 8);
          const created = await Promise.all(batch.map(async file => {
            const response = await github(env, "/git/blobs", {
              method: "POST",
              body: JSON.stringify({ content: file.content, encoding: "base64" })
            });
            if (!response.ok) throw new Error("No se pudo preparar " + file.path + " en GitHub (" + response.status + "): " + await response.text());
            const blob = await response.json();
            return { path: projectPath + "/" + file.path, mode: "100644", type: "blob", sha: blob.sha };
          }));
          blobEntries.push(...created);
        }

        const refResponse = await github(env, "/git/ref/heads/" + BRANCH);
        if (!refResponse.ok) throw new Error("No se pudo consultar la rama de GitHub (" + refResponse.status + "): " + await refResponse.text());
        const refData = await refResponse.json();
        const baseCommitResponse = await github(env, "/git/commits/" + refData.object.sha);
        if (!baseCommitResponse.ok) throw new Error("No se pudo consultar el commit base de GitHub (" + baseCommitResponse.status + ").");
        const baseCommit = await baseCommitResponse.json();

        const treeResponse = await github(env, "/git/trees", {
          method: "POST",
          body: JSON.stringify({ base_tree: baseCommit.tree.sha, tree: blobEntries })
        });
        if (!treeResponse.ok) throw new Error("No se pudo crear el árbol del proyecto (" + treeResponse.status + "): " + await treeResponse.text());
        const tree = await treeResponse.json();

        const commitResponse = await github(env, "/git/commits", {
          method: "POST",
          body: JSON.stringify({ message: "PSP Builder: importar proyecto " + requestId, tree: tree.sha, parents: [refData.object.sha] })
        });
        if (!commitResponse.ok) throw new Error("No se pudo crear el commit del proyecto (" + commitResponse.status + "): " + await commitResponse.text());
        const commit = await commitResponse.json();

        const updateRef = await github(env, "/git/refs/heads/" + BRANCH, {
          method: "PATCH",
          body: JSON.stringify({ sha: commit.sha, force: false })
        });
        if (!updateRef.ok) throw new Error("No se pudo publicar el proyecto en GitHub (" + updateRef.status + "): " + await updateRef.text());
        const dispatch = await github(env, "/actions/workflows/psp-build.yml/dispatches", {
          method: "POST",
          body: JSON.stringify({ ref: BRANCH, inputs: { project_path: projectPath, request_id: requestId } })
        });
        if (dispatch.status !== 204) throw new Error("GitHub no pudo iniciar la compilación (" + dispatch.status + "): " + await dispatch.text());
        return json({ job_id: requestId, status: "queued", message: "Fuentes recibidas. Compilación iniciada en GitHub Actions." }, 202, cors);
      }
      const statusMatch = url.pathname.match(/^\/status\/([a-f0-9-]{36})$/i);
      if (statusMatch && request.method === "GET") {
        const requestId = statusMatch[1];
        const response = await github(env, "/actions/workflows/psp-build.yml/runs?branch=" + BRANCH + "&event=workflow_dispatch&per_page=30");
        if (!response.ok) throw new Error("No se pudo consultar GitHub Actions: " + response.status + " " + await response.text());
        const data = await response.json();
        const title = "PSP Builder " + requestId;
        const run = (data.workflow_runs || []).find(item => item.display_title === title || item.name === title);
        if (!run) return json({ job_id: requestId, status: "queued", message: "Esperando a que GitHub registre la compilación." }, 200, cors);
        return json({ job_id: requestId, run_id: run.id, status: run.status, conclusion: run.conclusion,
          html_url: run.html_url, message: run.status === "completed" ? (run.conclusion === "success" ? "Compilación terminada." : "La compilación ha fallado.") : "PSPDEV está compilando el proyecto." }, 200, cors);
      }
      const resultMatch = url.pathname.match(/^\/result\/([a-f0-9-]{36})$/i);
      if (resultMatch && request.method === "GET") {
        const requestId = resultMatch[1];
        const runsResponse = await github(env, "/actions/workflows/psp-build.yml/runs?branch=" + BRANCH + "&event=workflow_dispatch&per_page=30");
        if (!runsResponse.ok) throw new Error("No se pudo consultar la compilación.");
        const runs = await runsResponse.json();
        const title = "PSP Builder " + requestId;
        const run = (runs.workflow_runs || []).find(item => item.display_title === title || item.name === title);
        if (!run || run.status !== "completed" || run.conclusion !== "success")
          return json({ error: "BUILD_NOT_READY", message: "La compilación aún no ha terminado correctamente." }, 409, cors);
        const artifactsResponse = await github(env, "/actions/runs/" + run.id + "/artifacts");
        if (!artifactsResponse.ok) throw new Error("No se pudieron consultar los artefactos.");
        const artifacts = await artifactsResponse.json();
        const artifact = (artifacts.artifacts || []).find(item => item.name === "PSP-EBOOT-" + requestId && !item.expired);
        if (!artifact) return json({ error: "ARTIFACT_NOT_FOUND", message: "No se encontró el EBOOT generado." }, 404, cors);
        const downloadResponse = await github(env, "/actions/artifacts/" + artifact.id + "/zip", { redirect: "manual" });
        const location = downloadResponse.headers.get("Location");
        if (![301, 302, 303, 307, 308].includes(downloadResponse.status) || !location) throw new Error("GitHub no proporcionó el enlace del artefacto.");
        const zipResponse = await fetch(location);
        if (!zipResponse.ok) throw new Error("No se pudo descargar el artefacto generado.");
        return new Response(zipResponse.body, { status: 200, headers: { ...cors, "Content-Type": "application/zip",
          "Content-Disposition": 'attachment; filename="PSP-EBOOT.zip"', "Cache-Control": "no-store" } });
      }
      return json({ error: "NOT_FOUND" }, 404, cors);
    } catch (error) {
      return json({ error: "BUILD_API_ERROR", message: String(error && error.message || error).slice(0, 1200) }, 500, cors);
    }
  }
};
