package com.pspbuilder.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Bundle
import android.widget.*
import java.io.BufferedInputStream
import java.io.File
import java.util.zip.ZipInputStream

class MainActivity : Activity() {
    private val pickZip = 1001
    private val saveZip = 1002
    private var projectName = "Ningún proyecto seleccionado"
    private var projectFile: File? = null
    private var extractedDir: File? = null
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
            text = "Prepara proyectos para PlayStation Portable"
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
        section(content, "3 · PREPARACIÓN")
        content.addView(button("Analizar y extraer proyecto") { inspectAndExtract() })
        content.addView(button("Exportar ZIP del proyecto") { exportProjectZip() })
        content.addView(button("COMPILAR EBOOT.PBP") { compileNotice() })
        status = TextView(this).apply {
            text = "Selecciona un ZIP para comenzar."; textSize = 14f
            setTextColor(0xFF9CE8B5.toInt()); setPadding(0, dp(12), 0, dp(8))
        }
        content.addView(status)
        section(content, "REGISTRO")
        log = TextView(this).apply {
            text = "PSP Builder 0.2.0\nNo se ha iniciado ninguna compilación."
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
            } catch (e: Exception) {
                status.text = "Error al exportar: ${e.message}"
            }
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
            status.text = if (makefiles.isNotEmpty()) "Proyecto extraído. Se ha encontrado Makefile." else "ZIP extraído, pero falta Makefile; no se puede compilar todavía."
            if (makefiles.size > 1) status.text = "Se han encontrado varios Makefiles; revisa la estructura antes de compilar."
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
            addCategory(Intent.CATEGORY_OPENABLE); type = "application/zip"
            putExtra(Intent.EXTRA_TITLE, name)
        }
        startActivityForResult(intent, saveZip)
    }

    private fun compileNotice() {
        val folder = extractedDir
        if (folder == null || !folder.exists()) { status.text = "Primero importa y analiza el ZIP."; return }
        AlertDialog.Builder(this)
            .setTitle("Compilación PSP aún no conectada")
            .setMessage("El proyecto ya está extraído y validado, pero esta versión no incorpora todavía PSPDEV local ni conexión autenticada con GitHub Actions. No se ha generado EBOOT.PBP. Para compilarlo, sube sus fuentes al repositorio y ejecuta el workflow 'Compile PSP project with PSPDEV'.")
            .setPositiveButton("Entendido", null).show()
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
}
