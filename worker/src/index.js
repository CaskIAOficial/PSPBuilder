/**
 * PSP Builder API — Cloudflare Worker
 * Runtime configuration is supplied through Worker secrets, never committed.
 */
export default {
  async fetch(request, env) {
    const cors = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type,Authorization",
    };
    if (request.method === "OPTIONS") return new Response(null, { headers: cors });
    const url = new URL(request.url);
    const json = (data, status = 200) => new Response(JSON.stringify(data), {
      status, headers: { ...cors, "Content-Type": "application/json; charset=utf-8" }
    });

    if (url.pathname === "/" || url.pathname === "/health") {
      return json({
        service: "PSP Builder API",
        status: "online",
        version: "0.1.0",
        configured: Boolean(env.GITHUB_APP_ID && env.GITHUB_INSTALLATION_ID && env.GITHUB_PRIVATE_KEY),
        message: "Servicio desplegado. Falta configurar la GitHub App para habilitar compilaciones."
      });
    }

    if (url.pathname === "/compile" && request.method === "POST") {
      if (!env.GITHUB_APP_ID || !env.GITHUB_INSTALLATION_ID || !env.GITHUB_PRIVATE_KEY) {
        return json({ error: "SERVER_NOT_CONFIGURED", message: "El servidor aún no tiene configuradas las credenciales privadas de GitHub." }, 503);
      }
      return json({ error: "BUILD_PIPELINE_PENDING", message: "La conexión segura con GitHub está pendiente de configuración." }, 501);
    }

    return json({ error: "NOT_FOUND" }, 404);
  }
};
