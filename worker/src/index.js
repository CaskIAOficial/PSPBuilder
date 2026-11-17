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
async function github(env, path, options = {}) {
  const jwt = await createAppJwt(env);
  const tokenResponse = await fetch(API + "/app/installations/" + env.GITHUB_INSTALLATION_ID + "/access_tokens", {
    method: "POST",
    headers: { Authorization: "Bearer " + jwt, Accept: "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28" }
  });
  if (!tokenResponse.ok) throw new Error("GitHub App no pudo obtener el token de instalación: " + tokenResponse.status + " " + await tokenResponse.text());
  const installation = await tokenResponse.json();
  return fetch(API + "/repos/" + OWNER + "/" + REPO + path, {
    ...options,
    headers: {
      Accept: "application/vnd.github+json",
      Authorization: "Bearer " + installation.token,
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
      return json({ service: "PSP Builder API", status: "online", version: "1.1.0", configured,
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
        for (const file of files) {
          const path = projectPath + "/" + file.path.split("/").map(encodeURIComponent).join("/");
          const response = await github(env, "/contents/" + path, {
            method: "PUT",
            body: JSON.stringify({ message: "PSP Builder: importar " + file.path, content: file.content, branch: BRANCH })
          });
          if (!response.ok) throw new Error("No se pudo subir " + file.path + " a GitHub (" + response.status + "): " + await response.text());
        }
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
