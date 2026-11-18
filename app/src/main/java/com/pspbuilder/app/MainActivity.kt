package com.pspbuilder.app

import android.app.Activity
import android.app.AlertDialog
import android.app.PendingIntent
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
    }

    private fun buildUi() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(22), dp(24), dp(22), dp(22))
            setBackgroundColor(0xFF101713.toInt())
        }
        root.addView(TextView(this).apply {
            text = "PSP BUILDER"; textSize = 30f
            setTextColor(0xFF9CE8B5.toInt()); typeface = android.graphics.Typeface.DEFAULT_BOLD
        })
        root.addView(TextView(this).apply {
            text = "Compila tus proyectos para PlayStation Portable"
            textSize = 14f; setTextColor(0xFFCAD7CE.toInt()); setPadding(0, dp(5), 0, dp(24))
        })
        projectLabel = TextView(this).apply {
            text = projectName; textSize = 14f; setTextColor(0xFFFFFFFF.toInt())
            setPadding(0, dp(8), 0, dp(14))
        }
        root.addView(projectLabel)
        root.addView(button("INSTALAR / REPARAR PSPDEV CON TERMUX") { prepareTermuxSetup() })
        root.addView(button("ELEGIR ZIP DEL PROYECTO") { openZipPicker() })
        root.addView(button("COMPILAR EBOOT.PBP") { compileProject() }.apply {
            setTextColor(0xFF102016.toInt()); setBackgroundColor(0xFF9CE8B5.toInt())
        })
        status = TextView(this).apply {
            text = "Elige un ZIP para comenzar."; textSize = 15f
            setTextColor(0xFF9CE8B5.toInt()); setPadding(0, dp(22), 0, dp(12))
        }
        root.addView(status)
        log = TextView(this).apply {
            text = "PSP Builder 0.4.0"; textSize = 12f
            setTextColor(0xFFCAD7CE.toInt()); setPadding(0, dp(8), 0, dp(8))
        }
        root.addView(log)
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

    private fun prepareTermuxSetup() {
        val installed = try { packageManager.getPackageInfo("com.termux", 0); true } catch (_: Exception) { false }
        if (!installed) {
            AlertDialog.Builder(this).setTitle("Instalar Termux")
                .setMessage("Termux es necesario. Se abrirá su repositorio oficial; instala la versión de GitHub y vuelve aquí.")
                .setPositiveButton("Abrir descarga") { _, _ -> startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/termux/termux-app/releases"))) }
                .setNegativeButton("Cancelar", null).show()
            return
        }
        AlertDialog.Builder(this).setTitle("Preparar Termux")
            .setMessage("Concede a PSP Builder el permiso «Ejecutar comandos en Termux». En Termux ejecuta una sola vez termux-setup-storage y acepta el permiso de archivos. Después activa allow-external-apps=true en ~/.termux/termux.properties y reinicia Termux. Cuando esté listo, pulsa CONTINUAR.")
            .setPositiveButton("Continuar") { _, _ -> launchPspdevInstaller() }
            .setNegativeButton("Cancelar", null).show()
    }

    private fun launchPspdevInstaller() {
        val script = """
pkg update -y
pkg install -y proot-distro curl tar
if ! proot-distro list | grep -q ubuntu; then proot-distro install ubuntu:24.04; fi
proot-distro login ubuntu -- bash -lc '
set -e
apt-get update
apt-get install -y git wget curl build-essential cmake make gcc g++ python3
cd /root
ARCH="${'$'}(uname -m)"
if [ ! -x /root/pspdev/bin/psp-gcc ]; then
  if [ "${'$'}ARCH" = "aarch64" ]; then ASSET="pspdev-ubuntu-24.04-arm-arm64.tar.gz";
  elif [ "${'$'}ARCH" = "x86_64" ]; then ASSET="pspdev-ubuntu-24.04-amd64.tar.gz";
  else echo "Arquitectura no compatible: ${'$'}ARCH"; exit 2; fi
  curl -fL "https://github.com/pspdev/pspdev/releases/latest/download/${'$'}ASSET" -o /tmp/pspdev.tar.gz
  tar -xzf /tmp/pspdev.tar.gz
fi
export PSPDEV=/root/pspdev
export PATH="${'$'}PATH:/root/pspdev/bin"
export PSPSDK="${'$'}(/root/pspdev/bin/psp-config --pspsdk-path)"
grep -q "PSPDEV=/root/pspdev" /root/.bashrc || cat >> /root/.bashrc <<'EOF'
export PSPDEV=/root/pspdev
export PATH="${'$'}PATH:/root/pspdev/bin"
export PSPSDK="${'$'}(/root/pspdev/bin/psp-config --pspsdk-path)"
EOF
psp-gcc --version
test -f "${'$'}PSPSDK/lib/build.mak"
echo PSPDEV_LISTO
'
""".trimIndent()
        try {
            val intent = Intent().apply {
                setClassName("com.termux", "com.termux.app.RunCommandService")
                action = "com.termux.RUN_COMMAND"
                putExtra("com.termux.RUN_COMMAND_PATH", "/data/data/com.termux/files/usr/bin/bash")
                putExtra("com.termux.RUN_COMMAND_ARGUMENTS", arrayOf("-lc", script))
                putExtra("com.termux.RUN_COMMAND_WORKDIR", "/data/data/com.termux/files/home")
                putExtra("com.termux.RUN_COMMAND_BACKGROUND", true)
                putExtra("com.termux.RUN_COMMAND_SESSION_ACTION", "0")
            }
            startService(intent)
            status.text = "Instalador enviado a Termux. Sigue la instalación en su terminal."
            log.text = "Preparando Ubuntu 24.04 y PSPDEV. No cierres Termux."
        } catch (e: Exception) {
            status.text = "No se pudo iniciar Termux: ${e.message ?: "comprueba permisos"}"
        }
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
                status.text = "ZIP preparado. Pulsa COMPILAR EBOOT.PBP."
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
        val zip = projectFile
        if (zip == null || !zip.exists()) { status.text = "Primero elige un ZIP de proyecto."; return }
        val runId = java.util.UUID.randomUUID().toString().replace("-", "")
        val zipName = "project-$runId.zip"
        val outName = "EBOOT-$runId.PBP"
        val logName = "build-$runId.log"
        val exitName = "build-$runId.exit"
        val rel = "${android.os.Environment.DIRECTORY_DOWNLOADS}/PSPBuilder/"
        val termuxShared = "/sdcard/Download/PSPBuilder"
        val ubuntuShared = "/mnt/sdcard/Download/PSPBuilder"
        status.text = "Preparando ZIP para Termux…"
        Thread {
            var zipUri: Uri? = null
            var outUri: Uri? = null
            var logUri: Uri? = null
            var exitUri: Uri? = null

            fun makeMediaFile(name: String, mime: String): Uri {
                val values = android.content.ContentValues().apply {
                    put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, name)
                    put(android.provider.MediaStore.MediaColumns.MIME_TYPE, mime)
                    put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH, rel)
                }
                return contentResolver.insert(android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                    ?: error("No se pudo crear $name en Descargas.")
            }
            fun readMediaText(uri: Uri?): String {
                if (uri == null) return ""
                return try { contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() }.orEmpty() }
                catch (_: Exception) { "" }
            }

            try {
                zipUri = makeMediaFile(zipName, "application/zip")
                contentResolver.openOutputStream(zipUri!!, "w").use { output ->
                    requireNotNull(output) { "No se pudo copiar el ZIP a Descargas." }
                    zip.inputStream().use { it.copyTo(output) }
                }
                outUri = makeMediaFile(outName, "application/octet-stream")
                logUri = makeMediaFile(logName, "text/plain")
                exitUri = makeMediaFile(exitName, "text/plain")

                val ubuntuScript = """
set -Eeuo pipefail
export PSPDEV=/root/pspdev
export PATH="/root/pspdev/bin:${'$'}PATH"
test -x "${'$'}PSPDEV/bin/psp-gcc" || { echo "No se encuentra psp-gcc en ${'$'}PSPDEV/bin"; exit 10; }
test -x "${'$'}PSPDEV/bin/psp-config" || { echo "No se encuentra psp-config en ${'$'}PSPDEV/bin"; exit 11; }
export PSPSDK="${'$'}("${'$'}PSPDEV/bin/psp-config" --pspsdk-path)"
test -d "${'$'}PSPSDK" || { echo "PSPSDK no válido: ${'$'}PSPSDK"; exit 12; }
echo "PSPDEV=${'$'}PSPDEV"
echo "PSPSDK=${'$'}PSPSDK"
echo "Compilador: ${'$'}(psp-gcc --version | head -n 1)"
test -r "$ubuntuShared/$zipName" || { echo "No se puede leer el ZIP: $ubuntuShared/$zipName"; exit 13; }
W="/tmp/pspbuilder-$runId"
rm -rf "${'$'}W"
mkdir -p "${'$'}W"
python3 - "$ubuntuShared/$zipName" "${'$'}W" <<'PY'
import sys, zipfile
archive, destination = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(archive) as z:
    for item in z.infolist():
        name = item.filename.replace("\\", "/")
        if name.startswith("/") or any(part == ".." for part in name.split("/")):
            raise SystemExit("Ruta insegura en ZIP: " + item.filename)
    z.extractall(destination)
PY
M="${'$'}(find "${'$'}W" -type f -iname Makefile -print -quit)"
if [ -z "${'$'}M" ]; then
    echo "No se encontró ningún Makefile dentro del ZIP."
    find "${'$'}W" -maxdepth 4 -type f | head -80
    exit 14
fi
PROJECT_DIR="${'$'}(dirname "${'$'}M")"
echo "Makefile: ${'$'}M"
cd "${'$'}PROJECT_DIR"
make -j2
E="${'$'}(find . -type f -name EBOOT.PBP -print -quit)"
test -n "${'$'}E" || { echo "make terminó, pero no generó EBOOT.PBP."; exit 15; }
cp "${'$'}E" "$ubuntuShared/$outName"
test -s "$ubuntuShared/$outName"
echo "COMPILACION_OK"
echo "EBOOT generado: $ubuntuShared/$outName"
""".trimIndent()

                val termuxCommand = """
set +e
rc=0
if ! mkdir -p '$termuxShared'; then
  rc=90
else
  : > '$termuxShared/$logName' || rc=91
  : > '$termuxShared/$exitName' || rc=92
  if [ "${'$'}rc" -eq 0 ]; then
    proot-distro login ubuntu -- /bin/bash -s <<'PSPBUILDER_UBUNTU_SCRIPT' > '$termuxShared/$logName' 2>&1
$ubuntuScript
PSPBUILDER_UBUNTU_SCRIPT
    rc=$?
  fi
fi
printf '%s\n' "${'$'}rc" > '$termuxShared/$exitName' 2>/dev/null || true
exit "${'$'}rc"
""".trimIndent()

                runOnUiThread {
                    status.text = "Compilando en Ubuntu local…"
                    log.text = "Enviando el proyecto a Termux. La aplicación recogerá automáticamente el registro y el EBOOT.PBP."
                }
                val resultPrefs = getSharedPreferences("termux_results", MODE_PRIVATE)
                resultPrefs.edit()
                    .remove("${runId}_received")
                    .remove("${runId}_exit_code")
                    .remove("${runId}_err")
                    .remove("${runId}_errmsg")
                    .apply()

                val resultIntent = Intent(this@MainActivity, TermuxResultService::class.java)
                    .putExtra("run_id", runId)
                val pendingFlags = PendingIntent.FLAG_ONE_SHOT or
                    (if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) PendingIntent.FLAG_MUTABLE else 0)
                val resultPendingIntent = PendingIntent.getService(
                    this@MainActivity, runId.hashCode() and 0x7fffffff, resultIntent, pendingFlags
                )

                val commandIntent = Intent().apply {
                    setClassName("com.termux", "com.termux.app.RunCommandService")
                    action = "com.termux.RUN_COMMAND"
                    putExtra("com.termux.RUN_COMMAND_PATH", "/data/data/com.termux/files/usr/bin/bash")
                    putExtra("com.termux.RUN_COMMAND_ARGUMENTS", arrayOf("-s"))
                    putExtra("com.termux.RUN_COMMAND_STDIN", termuxCommand)
                    putExtra("com.termux.RUN_COMMAND_WORKDIR", "/data/data/com.termux/files/home")
                    putExtra("com.termux.RUN_COMMAND_BACKGROUND", false)
                    putExtra("com.termux.RUN_COMMAND_SESSION_ACTION", "0")
                    putExtra("com.termux.RUN_COMMAND_COMMAND_LABEL", "Compilar proyecto PSP")
                    putExtra("com.termux.RUN_COMMAND_COMMAND_DESCRIPTION", "PSP Builder ejecuta la compilación local en Ubuntu.")
                    putExtra("com.termux.RUN_COMMAND_PENDING_INTENT", resultPendingIntent)
                }
                startService(commandIntent)

                val deadline = System.currentTimeMillis() + 900000L
                var exitCode: Int? = null
                var termuxErr: Int? = null
                var termuxErrMsg = ""
                while (System.currentTimeMillis() < deadline && exitCode == null) {
                    if (resultPrefs.getBoolean("${runId}_received", false)) {
                        exitCode = resultPrefs.getInt("${runId}_exit_code", -999)
                        termuxErr = resultPrefs.getInt("${runId}_err", 0)
                        termuxErrMsg = resultPrefs.getString("${runId}_errmsg", "").orEmpty()
                    } else {
                        exitCode = readMediaText(exitUri).trim().toIntOrNull()
                    }
                    if (exitCode == null) Thread.sleep(1000)
                }
                val buildLog = readMediaText(logUri)
                if (termuxErr != null && termuxErr != -1) {
                    error("Termux no pudo iniciar el comando (error $termuxErr). $termuxErrMsg")
                }
                if (exitCode == null) error("Termux no comunicó el final de la compilación en 15 minutos.\n${buildLog.takeLast(3500)}")
                if (exitCode != 0) error("La compilación falló (código $exitCode).\n${buildLog.takeLast(5000)}")

                val input = contentResolver.openInputStream(outUri!!)
                    ?: error("Termux terminó correctamente, pero Android no pudo abrir el EBOOT.PBP.")
                val eboot = File(cacheDir, outName)
                input.use { source -> eboot.outputStream().use { target -> source.copyTo(target) } }
                require(eboot.isFile && eboot.length() > 0L) { "El EBOOT.PBP generado está vacío." }
                pendingEboot = eboot
                runOnUiThread {
                    try {
                        val saved = saveEbootToDownloads(eboot, "PSPBuilder")
                        status.text = "¡Compilación local terminada!"
                        log.text = "EBOOT.PBP generado correctamente.\n$saved\n\n${buildLog.takeLast(1800)}"
                    } catch (e: Exception) {
                        status.text = "Compilado, pero no se pudo guardar: ${e.message}"
                        log.text = buildLog.takeLast(2500)
                    }
                }
            } catch (e: Exception) {
                runOnUiThread {
                    status.text = "Error de compilación local: ${e.message ?: "error desconocido"}"
                    log.text = readMediaText(logUri).takeLast(5000).ifBlank {
                        "No se pudo iniciar Termux. Comprueba el permiso RUN_COMMAND y el acceso al almacenamiento compartido."
                    }
                }
            } finally {
                listOf(zipUri, outUri, logUri, exitUri).forEach { uri ->
                    try { uri?.let { contentResolver.delete(it, null, null) } } catch (_: Exception) { }
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
                    var result: JSONObject? = null
                    var networkAttempts = 0
                    while (result == null) {
                        try {
                            result = oauthPost("https://github.com/login/oauth/access_token",
                                "client_id=$OAUTH_CLIENT_ID&device_code=${Uri.encode(deviceCode)}&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code",
                                allowOAuthError = true)
                        } catch (e: java.io.IOException) {
                            networkAttempts++
                            if (networkAttempts >= 6) throw e
                            runOnUiThread {
                                status.text = "GitHub ya recibió tu autorización. Recuperando la conexión… (intento $networkAttempts/6)"
                            }
                            Thread.sleep((networkAttempts * 3000L).coerceAtMost(15000L))
                        }
                    }
                    val tokenResponse = result ?: throw IllegalStateException("No se recibió respuesta de GitHub.")
                    val token = tokenResponse.optString("access_token")
                    if (token.isNotBlank()) {
                        saveToken(token)
                        runOnUiThread {
                            status.text = "GitHub autorizado. Iniciando compilación…"
                            compileProject()
                        }
                        return@Thread
                    }
                    when (tokenResponse.optString("error")) {
                        "authorization_pending" -> waitSeconds = interval
                        "slow_down" -> waitSeconds += 5
                        "expired_token" -> throw IllegalStateException("El código ha caducado. Pulsa COMPILAR para intentarlo de nuevo.")
                        "access_denied" -> throw IllegalStateException("Has rechazado la autorización de GitHub.")
                        else -> throw IllegalStateException(tokenResponse.optString("error_description", "No se pudo autorizar GitHub."))
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

    private fun getGameFolderName(projectRoot: File): String {
        val fromZip = projectName.substringBeforeLast('.', projectName).trim()
        val candidate = if (fromZip.isNotBlank() && fromZip != "Ningún proyecto seleccionado") fromZip else projectRoot.name
        val safe = candidate.map { if (it in charArrayOf('/', '\\', ':', '*', '?', '"', '<', '>', '|')) '_' else it }.joinToString("")
            .replace(Regex("\\s+"), " ")
            .trim().trim('.')
        return safe.take(64).ifBlank { "Juego PSP" }
    }

    private fun saveEbootToDownloads(source: File, gameFolderName: String): String {
        if (android.os.Build.VERSION.SDK_INT >= 29) {
            val values = android.content.ContentValues().apply {
                put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, "EBOOT.PBP")
                put(android.provider.MediaStore.MediaColumns.MIME_TYPE, "application/octet-stream")
                put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH,
                    "${android.os.Environment.DIRECTORY_DOWNLOADS}/$gameFolderName/")
                put(android.provider.MediaStore.MediaColumns.IS_PENDING, 1)
            }
            val uri = contentResolver.insert(android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                ?: error("Android no pudo crear la carpeta del juego en Descargas.")
            try {
                contentResolver.openOutputStream(uri, "w").use { output ->
                    requireNotNull(output) { "No se pudo abrir el archivo de destino." }
                    source.inputStream().use { it.copyTo(output) }
                }
                values.clear()
                values.put(android.provider.MediaStore.MediaColumns.IS_PENDING, 0)
                contentResolver.update(uri, values, null, null)
                return "Guardado en Descargas/$gameFolderName/EBOOT.PBP"
            } catch (e: Exception) {
                contentResolver.delete(uri, null, null)
                throw e
            }
        } else {
            val base = getExternalFilesDir(android.os.Environment.DIRECTORY_DOWNLOADS)
                ?: error("Almacenamiento no disponible.")
            val dir = File(base, gameFolderName).apply { mkdirs() }
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
