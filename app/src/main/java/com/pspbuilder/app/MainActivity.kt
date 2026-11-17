package com.pspbuilder.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.text.InputType
import android.view.ViewGroup
import android.widget.*
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.security.KeyStore
import java.util.Base64
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import java.util.zip.ZipInputStream

class MainActivity : Activity() {
    private val pickZip = 1001
    private val saveZip = 1002
    private val saveEboot = 1003
    private val repo = "CaskIAOficial/PSPBuilder"
    private val branch = "main"
    private val keyAlias = "pspbuilder_github_token"
    private var projectName = "Ningún proyecto seleccionado"
    private var projectFile: File? = null
    private var extractedDir: File? = null
    private var pendingEboot: File? = null
    private lateinit var status: TextView
    private lateinit var projectLabel: TextView
    private lateinit var log: TextView
    private lateinit var sdSwitch: Switch
    private lateinit var tokenInput: EditText

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
        tokenInput.setText(loadToken())
    }

    private fun buildUi() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(18), dp(20), dp(20))
            setBackgroundColor(0xFF101713.toInt())
        }
        val scroll = ScrollView(this)
        val content = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        scroll.addView(content)
        root.addView(scroll, LinearLayout.LayoutParams(-1, 0, 1f))
        content.addView(TextView(this).apply {
            text = "PSP BUILDER"; textSize = 27f
            setTextColor(0xFF9CE8B5.toInt()); typeface = android.graphics.Typeface.DEFAULT_BOLD
        })
        content.addView(TextView(this).apply {
            text = "Prepara y compila proyectos para PlayStation Portable"
            textSize = 14f; setTextColor(0xFFCAD7CE.toInt()); setPadding(0, dp(4), 0, dp(18))
        })
        section(content, "1 · PROYECTO")
        projectLabel = TextView(this).apply {
            text = projectName; textSize = 14f; setTextColor(0xFFFFFFFF.toInt()); setPadding(0, dp(8), 0, dp(10))
        }
        content.addView(projectLabel)
        content.addView(button("Seleccionar ZIP del proyecto") { openZipPicker() })
        section(content, "2 · ALMACENAMIENTO")
        content.addView(Switch(this).apply {
            text = "Conservar proyecto en memoria interna"; setTextColor(0xFFFFFFFF.toInt()); isChecked = true
            isEnabled = false
        })
        sdSwitch = Switch(this).apply {
            text = "Exportar ZIP a tarjeta SD / ubicación elegida"; setTextColor(0xFFFFFFFF.toInt()); isChecked = true
        }
        content.addView(sdSwitch)
        section(content, "3 · CONEXIÓN CON GITHUB")
        content.addView(TextView(this).apply {
            text = "Crea un token Fine-grained para CaskIAOficial/PSPBuilder con Contents (Read and write) y Actions (Read and write). Se guarda cifrado en este dispositivo."
            textSize = 12f; setTextColor(0xFFCAD7CE.toInt())
        })
        tokenInput = EditText(this).apply {
            hint = "Token de GitHub (github_pat_...)"
            setTextColor(0xFFFFFFFF.toInt()); setHintTextColor(0xFF91A397.toInt())
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            setSingleLine(true)
        }
        content.addView(tokenInput, LinearLayout.LayoutParams(-1, dp(52)))
        content.addView(button("Guardar token cifrado") {
            val token = tokenInput.text.toString().trim()
            if (!token.startsWith("github_pat_") && !token.startsWith("ghp_")) {
                status.text = "Introduce un token personal de GitHub válido."
            } else try {
                saveToken(token)
                status.text = "Token guardado cifrado en Android."
                log.text = "Conexión configurada. El token no se guarda en texto plano."
            } catch (e: Exception) { status.text = "No se pudo guardar el token: ${e.message}" }
        })
        section(content, "4 · PREPARACIÓN Y COMPILACIÓN")
        content.addView(button("Analizar y extraer proyecto") { inspectAndExtract() })
        content.addView(button("Exportar ZIP del proyecto") { exportProjectZip() })
        content.addView(button("COMPILAR Y DESCARGAR EBOOT.PBP") { compileProject() })
        status = TextView(this).apply {
            text = "Selecciona un ZIP para comenzar."; textSize = 14f
            setTextColor(0xFF9CE8B5.toInt()); setPadding(0, dp(12), 0, dp(8))
        }
        content.addView(status)
        section(content, "REGISTRO")
        log = TextView(this).apply {
            text = "PSP Builder 0.3.0\nListo para preparar un proyecto."
            textSize = 12f; setTextColor(0xFFCAD7CE.toInt()); setPadding(dp(10), dp(10), dp(10), dp(10))
            setBackgroundColor(0xFF1B2820.toInt())
        }
        content.addView(log)
        setContentView(root)
    }

    private fun section(parent: LinearLayout, title: String) {
        parent.addView(TextView(this).apply {
            text = title; textSize = 13f; setTextColor(0xFF77D69A.toInt())
            typeface = android.graphics.Typeface.DEFAULT_BOLD; setPadding(0, dp(18), 0, dp(4))
        })
    }

    private fun button(label: String, action: () -> Unit): Button = Button(this).apply {
        text = label; isAllCaps = false; setOnClickListener { action() }
    }

    private fun openZipPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE); type = "application/zip"
            putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream"))
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, pickZip)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK || data?.data == null) return
        val uri = data.data!!
        if (requestCode == pickZip) {
            try {
                try { contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) } catch (_: Exception) { }
                val displayName = uri.lastPathSegment?.substringAfterLast('/') ?: "proyecto.zip"
                val target = File(cacheDir, "imported-project.zip")
                contentResolver.openInputStream(uri).use { input ->
                    requireNotNull(input) { "No se pudo abrir el archivo seleccionado." }
                    target.outputStream().use { output -> input.copyTo(output) }
                }
                require(target.length() > 0) { "El ZIP está vacío." }
                projectFile = target
                projectName = displayName
                projectLabel.text = projectName
                extractedDir = null
                status.text = "ZIP importado. Analízalo para extraer y validar su contenido."
                log.text = "Importado: $projectName\nTamaño: ${target.length()} bytes"
            } catch (e: Exception) {
                projectFile = null
                status.text = "No se pudo importar: ${e.message ?: "error desconocido"}"
            }
        } else if (requestCode == saveZip) {
            try {
                val source = projectFile ?: error("No hay ZIP importado")
                contentResolver.openOutputStream(uri, "w").use { output ->
                    requireNotNull(output) { "No se pudo abrir el destino." }
                    source.inputStream().use { it.copyTo(output) }
                }
                status.text = "ZIP exportado correctamente."
            } catch (e: Exception) { status.text = "Error al exportar: ${e.message}" }
        } else if (requestCode == saveEboot) {
            try {
                val source = pendingEboot ?: error("No hay EBOOT.PBP preparado")
                contentResolver.openOutputStream(uri, "w").use { output ->
                    requireNotNull(output) { "No se pudo abrir el destino." }
                    source.inputStream().use { it.copyTo(output) }
                }
                status.text = "EBOOT.PBP guardado correctamente. Ya puedes copiarlo a la PSP o abrirlo en PPSSPP."
            } catch (e: Exception) { status.text = "Error al guardar EBOOT.PBP: ${e.message}" }
        }
    }

    private fun inspectAndExtract() {
        val zip = projectFile
        if (zip == null || !zip.exists()) { status.text = "Selecciona primero un ZIP."; return }
        val destination = File(filesDir, "projects/${System.currentTimeMillis()}")
        try {
            destination.mkdirs()
            var files = 0
            var hasMakefile = false
            var hasMainC = false
            var totalBytes = 0L
            val maxFiles = 10000
            val maxBytes = 300L * 1024L * 1024L
            ZipInputStream(BufferedInputStream(zip.inputStream())).use { input ->
                while (true) {
                    val entry = input.nextEntry ?: break
                    val outFile = File(destination, entry.name).canonicalFile
                    val rootPath = destination.canonicalPath + File.separator
                    require(outFile.path == destination.canonicalPath || outFile.path.startsWith(rootPath)) {
                        "El ZIP contiene una ruta insegura: ${entry.name}"
                    }
                    if (entry.isDirectory) outFile.mkdirs() else {
                        files++
                        require(files <= maxFiles) { "El ZIP contiene demasiados archivos." }
                        outFile.parentFile?.mkdirs()
                        outFile.outputStream().use { output ->
                            val buffer = ByteArray(8192)
                            while (true) {
                                val read = input.read(buffer)
                                if (read < 0) break
                                totalBytes += read
                                require(totalBytes <= maxBytes) { "El contenido extraído supera 300 MB." }
                                output.write(buffer, 0, read)
                            }
                        }
                        val base = entry.name.substringAfterLast('/').lowercase()
                        if (base == "makefile") hasMakefile = true
                        if (base == "main.c") hasMainC = true
                    }
                    input.closeEntry()
                }
            }
            require(files > 0) { "El ZIP no contiene archivos." }
            extractedDir = destination
            val makefiles = destination.walkTopDown().filter { it.isFile && it.name.equals("Makefile", true) }.toList()
            log.text = "Archivos extraídos: $files\nTamaño descomprimido: ${totalBytes / 1024} KB\nMakefile: ${if (hasMakefile) "Sí" else "No"}\nmain.c: ${if (hasMainC) "Sí" else "No"}\nUbicación privada: ${destination.absolutePath}"
            status.text = if (makefiles.size == 1) "Proyecto extraído. Makefile localizado." else if (makefiles.isEmpty()) "Falta Makefile: no se puede compilar." else "Hay varios Makefiles; deja solo el correspondiente al juego."
        } catch (e: Exception) {
            destination.deleteRecursively()
            extractedDir = null
            status.text = "No se pudo analizar el ZIP: ${e.message ?: "archivo no válido"}"
        }
    }

    private fun exportProjectZip() {
        val source = projectFile
        if (source == null || !source.exists()) { status.text = "Importa primero un ZIP."; return }
        if (!sdSwitch.isChecked) { status.text = "Activa la exportación a una ubicación elegida."; return }
        val name = projectName.substringBeforeLast('.', projectName).ifBlank { "proyecto-psp" } + "-copia.zip"
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE); type = "application/zip"; putExtra(Intent.EXTRA_TITLE, name)
        }
        startActivityForResult(intent, saveZip)
    }

    private fun compileProject() {
        val folder = extractedDir
        if (folder == null || !folder.exists()) { status.text = "Primero importa y analiza el ZIP."; return }
        val token = loadToken()?.takeIf { it.isNotBlank() }
        if (token == null) { status.text = "Guarda primero tu token de GitHub."; return }
        val makefiles = folder.walkTopDown().filter { it.isFile && it.name.equals("Makefile", true) }.toList()
        if (makefiles.size != 1) { status.text = "El proyecto debe contener exactamente un Makefile."; return }
        val projectRoot = makefiles.single().parentFile ?: folder
        val files = projectRoot.walkTopDown().filter { it.isFile }.toList()
        if (files.isEmpty() || files.size > 100) { status.text = "El proyecto debe tener entre 1 y 100 archivos para esta versión."; return }
        val total = files.sumOf { it.length() }
        if (total > 20L * 1024L * 1024L || files.any { it.length() > 900L * 1024L }) {
            status.text = "Límite de esta versión: 20 MB en total y 900 KB por archivo."; return
        }
        val projectPath = "psp-project/mobile-${System.currentTimeMillis()}"
        status.text = "Preparando subida a GitHub…"
        log.text = "Proyecto: ${projectRoot.name}\nArchivos: ${files.size}\nTamaño: ${total / 1024} KB\nEnviando fuentes y solicitando compilación PSPDEV…"
        Thread {
            try {
                files.forEachIndexed { index, file ->
                    val relative = file.relativeTo(projectRoot).invariantSeparatorsPath
                    val bytes = file.readBytes()
                    val encoded = Base64.getEncoder().encodeToString(bytes)
                    val encodedPath = Uri.encode(relative, "/")
                    val body = JSONObject()
                        .put("message", "PSP Builder: subir fuente ${relative}")
                        .put("content", encoded)
                        .put("branch", branch)
                    githubRequest(token, "PUT", "/contents/$projectPath/$encodedPath", body)
                    runOnUiThread { status.text = "Subiendo archivos: ${index + 1}/${files.size}" }
                }
                val dispatchStartedAt = System.currentTimeMillis()
                val dispatch = JSONObject().put("ref", branch).put("inputs", JSONObject().put("project_path", projectPath))
                githubRequest(token, "POST", "/actions/workflows/psp-build.yml/dispatches", dispatch, expected = intArrayOf(204))
                runOnUiThread { status.text = "Fuentes subidas. GitHub Actions está compilando con PSPDEV…" }
                val runId = waitForWorkflow(token, dispatchStartedAt)
                val artifact = waitForArtifact(token, runId)
                val zipBytes = downloadArtifact(token, artifact)
                val eboot = extractEboot(zipBytes)
                pendingEboot = eboot
                runOnUiThread {
                    status.text = "¡Compilación terminada! EBOOT.PBP generado."
                    log.text = "Compilación correcta.\nEBOOT.PBP: ${eboot.length()} bytes\nElige dónde guardarlo para usarlo en PSP o PPSSPP."
                    val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE); type = "application/octet-stream"
                        putExtra(Intent.EXTRA_TITLE, "EBOOT.PBP")
                    }
                    startActivityForResult(intent, saveEboot)
                }
            } catch (e: Exception) {
                runOnUiThread {
                    status.text = "Error de compilación: ${e.message ?: "fallo desconocido"}"
                    log.text = "La compilación no ha terminado correctamente.\nRevisa el token, los permisos y el registro de GitHub Actions.\n${e.message}"
                }
            }
        }.start()
    }

    private fun githubRequest(token: String, method: String, path: String, body: JSONObject? = null, expected: IntArray = intArrayOf(200)): String {
        val connection = (URL("https://api.github.com/repos/$repo$path").openConnection() as HttpURLConnection)
        connection.requestMethod = method
        connection.connectTimeout = 20000
        connection.readTimeout = 30000
        connection.setRequestProperty("Accept", "application/vnd.github+json")
        connection.setRequestProperty("Authorization", "Bearer $token")
        connection.setRequestProperty("X-GitHub-Api-Version", "2022-11-28")
        if (body != null) {
            connection.doOutput = true
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
            connection.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
        }
        val code = connection.responseCode
        val stream = if (code in 200..299) connection.inputStream else connection.errorStream
        val response = stream?.bufferedReader()?.use { it.readText() } ?: ""
        connection.disconnect()
        if (code !in expected) {
            val message = try { JSONObject(response).optString("message", response) } catch (_: Exception) { response }
            throw IllegalStateException("GitHub API ($code): $message")
        }
        return response
    }

    private fun waitForWorkflow(token: String, startedAt: Long): Long {
        // Give GitHub a moment to register the workflow_dispatch run.
        Thread.sleep(5000)
        repeat(90) {
            val json = JSONObject(githubRequest(token, "GET", "/actions/workflows/psp-build.yml/runs?branch=$branch&event=workflow_dispatch&per_page=1"))
            val runs = json.getJSONArray("workflow_runs")
            if (runs.length() > 0) {
                val run = (0 until runs.length()).map { runs.getJSONObject(it) }.firstOrNull {
                    try { java.time.Instant.parse(it.optString("created_at")).toEpochMilli() >= startedAt - 5000 } catch (_: Exception) { false }
                }
                if (run == null) { Thread.sleep(3000); return@repeat }
                val id = run.getLong("id")
                val state = run.optString("status")
                if (state == "completed") {
                    if (run.optString("conclusion") != "success") {
                        throw IllegalStateException("GitHub Actions terminó con estado: ${run.optString("conclusion")}. Consulta Actions para ver el error de PSPDEV.")
                    }
                    return id
                }
                runOnUiThread { status.text = "Compilando en GitHub Actions: $state…" }
            }
            Thread.sleep(5000)
        }
        throw IllegalStateException("La compilación está tardando demasiado. Comprueba el workflow en GitHub Actions.")
    }

    private fun waitForArtifact(token: String, runId: Long): Long {
        repeat(12) {
            val json = JSONObject(githubRequest(token, "GET", "/actions/runs/$runId/artifacts"))
            val items = json.getJSONArray("artifacts")
            if (items.length() > 0) return items.getJSONObject(0).getLong("id")
            Thread.sleep(2000)
        }
        throw IllegalStateException("La compilación terminó, pero no se encontró el artefacto EBOOT.")
    }

    private fun downloadArtifact(token: String, artifactId: Long): ByteArray {
        val url = URL("https://api.github.com/repos/$repo/actions/artifacts/$artifactId/zip")
        val c = url.openConnection() as HttpURLConnection
        c.instanceFollowRedirects = false
        c.connectTimeout = 20000
        c.readTimeout = 60000
        c.setRequestProperty("Accept", "application/vnd.github+json")
        c.setRequestProperty("Authorization", "Bearer $token")
        val code = c.responseCode
        val location = c.getHeaderField("Location")
        c.disconnect()
        if (code !in 300..399 || location.isNullOrBlank()) throw IllegalStateException("No se pudo descargar el artefacto de GitHub (HTTP $code).")
        val signed = (URL(location).openConnection() as HttpURLConnection)
        signed.connectTimeout = 20000
        signed.readTimeout = 60000
        val bytes = signed.inputStream.use { it.readBytes() }
        signed.disconnect()
        return bytes
    }

    private fun extractEboot(zipBytes: ByteArray): File {
        val out = File(cacheDir, "EBOOT.PBP")
        var found = false
        ZipInputStream(zipBytes.inputStream()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (!entry.isDirectory && entry.name.substringAfterLast('/').equals("EBOOT.PBP", true)) {
                    val bytes = ByteArrayOutputStream()
                    zip.copyTo(bytes)
                    require(bytes.size() > 0) { "El EBOOT.PBP descargado está vacío." }
                    out.writeBytes(bytes.toByteArray())
                    found = true
                    break
                }
                zip.closeEntry()
            }
        }
        require(found) { "El artefacto no contiene EBOOT.PBP." }
        return out
    }

    private fun extractProject(zip: File): File {
        val destination = File(filesDir, "projects/${System.currentTimeMillis()}")
        destination.mkdirs()
        try {
            var count = 0
            var total = 0L
            ZipInputStream(BufferedInputStream(zip.inputStream())).use { input ->
                while (true) {
                    val entry = input.nextEntry ?: break
                    val out = File(destination, entry.name).canonicalFile
                    val root = destination.canonicalPath + File.separator
                    require(out.path == destination.canonicalPath || out.path.startsWith(root)) { "El ZIP contiene una ruta insegura." }
                    if (entry.isDirectory) out.mkdirs() else {
                        count++
                        require(count <= 10000) { "Demasiados archivos en el ZIP." }
                        out.parentFile?.mkdirs()
                        out.outputStream().use { output ->
                            val buffer = ByteArray(8192)
                            while (true) {
                                val n = input.read(buffer)
                                if (n < 0) break
                                total += n
                                require(total <= 300L * 1024L * 1024L) { "El ZIP supera 300 MB descomprimidos." }
                                output.write(buffer, 0, n)
                            }
                        }
                    }
                    input.closeEntry()
                }
            }
            require(count > 0) { "El ZIP está vacío." }
            return destination
        } catch (e: Exception) {
            destination.deleteRecursively()
            throw e
        }
    }

    private fun authorizeGitHub() {
        status.text = "Conectando con GitHub para la autorización inicial…"
        Thread {
            try {
                val device = oauthPost("https://github.com/login/device/code",
                    "client_id=$OAUTH_CLIENT_ID&scope=repo%20workflow")
                val code = device.getString("user_code")
                val deviceCode = device.getString("device_code")
                val verification = device.optString("verification_uri", "https://github.com/login/device")
                val interval = device.optLong("interval", 5L)
                runOnUiThread {
                    AlertDialog.Builder(this)
                        .setTitle("Autorizar PSP Builder")
                        .setMessage("GitHub necesita que autorices esta aplicación una sola vez.\n\nCódigo: $code\n\nSe abrirá GitHub para introducirlo.")
                        .setPositiveButton("Abrir GitHub") { _, _ ->
                            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(verification)))
                            status.text = "Introduce el código en GitHub. Esperando autorización…"
                        }
                        .setNegativeButton("Ahora no", null)
                        .show()
                }
                var waitSeconds = interval
                repeat(60) {
                    Thread.sleep(waitSeconds * 1000L)
                    val result = oauthPost("https://github.com/login/oauth/access_token",
                        "client_id=$OAUTH_CLIENT_ID&device_code=${Uri.encode(deviceCode)}&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code",
                        allowOAuthError = true)
                    val token = result.optString("access_token")
                    if (token.isNotBlank()) {
                        saveToken(token)
                        runOnUiThread {
                            status.text = "GitHub autorizado. Iniciando compilación…"
                            compileProject()
                        }
                        return@Thread
                    }
                    when (result.optString("error")) {
                        "authorization_pending" -> waitSeconds = interval
                        "slow_down" -> waitSeconds += 5
                        "expired_token" -> throw IllegalStateException("El código ha caducado. Pulsa COMPILAR para intentarlo de nuevo.")
                        "access_denied" -> throw IllegalStateException("Has rechazado la autorización de GitHub.")
                        else -> throw IllegalStateException(result.optString("error_description", "No se pudo autorizar GitHub."))
                    }
                }
                throw IllegalStateException("Se agotó el tiempo de autorización. Inténtalo de nuevo.")
            } catch (e: Exception) {
                runOnUiThread { status.text = "No se pudo conectar con GitHub: ${e.message}" }
            }
        }.start()
    }

    private fun oauthPost(endpoint: String, form: String, allowOAuthError: Boolean = false): JSONObject {
        val c = (URL(endpoint).openConnection() as HttpURLConnection)
        c.requestMethod = "POST"
        c.connectTimeout = 20000
        c.readTimeout = 30000
        c.doOutput = true
        c.setRequestProperty("Accept", "application/json")
        c.setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
        c.outputStream.use { it.write(form.toByteArray(Charsets.UTF_8)) }
        val code = c.responseCode
        val stream = if (code in 200..299) c.inputStream else c.errorStream
        val body = stream?.bufferedReader()?.use { it.readText() } ?: ""
        c.disconnect()
        if (code !in 200..299) throw IllegalStateException("OAuth de GitHub devolvió HTTP $code: $body")
        val json = JSONObject(body)
        if (!allowOAuthError && json.has("error")) throw IllegalStateException(json.optString("error_description", json.optString("error")))
        return json
    }

    private fun saveEbootToDownloads(source: File): String {
        if (android.os.Build.VERSION.SDK_INT >= 29) {
            val values = android.content.ContentValues().apply {
                put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, "EBOOT.PBP")
                put(android.provider.MediaStore.MediaColumns.MIME_TYPE, "application/octet-stream")
                put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH, android.os.Environment.DIRECTORY_DOWNLOADS)
                put(android.provider.MediaStore.MediaColumns.IS_PENDING, 1)
            }
            val uri = contentResolver.insert(android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                ?: error("Android no pudo crear el archivo en Descargas.")
            try {
                contentResolver.openOutputStream(uri, "w").use { output ->
                    requireNotNull(output) { "No se pudo abrir el archivo de destino." }
                    source.inputStream().use { it.copyTo(output) }
                }
                values.clear()
                values.put(android.provider.MediaStore.MediaColumns.IS_PENDING, 0)
                contentResolver.update(uri, values, null, null)
                return "Guardado en Descargas/EBOOT.PBP"
            } catch (e: Exception) {
                contentResolver.delete(uri, null, null)
                throw e
            }
        } else {
            val dir = getExternalFilesDir(android.os.Environment.DIRECTORY_DOWNLOADS) ?: error("Almacenamiento no disponible.")
            val target = File(dir, "EBOOT.PBP")
            source.copyTo(target, overwrite = true)
            return "Guardado en almacenamiento de PSP Builder: ${target.absolutePath}"
        }
    }

    companion object {
        private const val OAUTH_CLIENT_ID = "Ov23lizj2r6aAplWCq8f"
    }

    private fun secretKey(): SecretKey {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        val existing = ks.getKey(keyAlias, null) as? SecretKey
        if (existing != null) return existing
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        generator.init(KeyGenParameterSpec.Builder(keyAlias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
        return generator.generateKey()
    }

    private fun saveToken(token: String) {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, secretKey())
        val encrypted = cipher.doFinal(token.toByteArray(Charsets.UTF_8))
        val value = Base64.getEncoder().encodeToString(cipher.iv) + ":" + Base64.getEncoder().encodeToString(encrypted)
        getSharedPreferences("secure", MODE_PRIVATE).edit().putString("github_token", value).apply()
    }

    private fun loadToken(): String? {
        val value = getSharedPreferences("secure", MODE_PRIVATE).getString("github_token", null) ?: return null
        return try {
            val parts = value.split(":")
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, secretKey(), GCMParameterSpec(128, Base64.getDecoder().decode(parts[0])))
            String(cipher.doFinal(Base64.getDecoder().decode(parts[1])), Charsets.UTF_8)
        } catch (_: Exception) { null }
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
}
