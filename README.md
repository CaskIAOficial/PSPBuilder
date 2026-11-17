# PSP Builder para Android — 0.2.0

Aplicación Android nativa (Kotlin) para importar proyectos PSP en ZIP, extraerlos de forma segura, comprobar la presencia de `Makefile` y `main.c`, conservar una copia privada y exportar el ZIP a una ubicación elegida.

## Estado real de esta versión

- Importación de ZIP mediante el selector de documentos de Android.
- Extracción en almacenamiento privado de la aplicación.
- Protección contra rutas ZIP maliciosas (`../`), límite de 10.000 archivos y 300 MB descomprimidos.
- Detección de `Makefile` y `main.c`.
- Exportación del ZIP original a memoria interna, tarjeta SD o proveedor de documentos elegido.
- **La compilación EBOOT.PBP todavía no está conectada a la interfaz.** No hay PSPDEV local para Android ni autenticación configurada para lanzar GitHub Actions desde la app.

## Generar APK

El workflow `.github/workflows/android-apk.yml` genera un APK de depuración en GitHub Actions. En el repositorio, abre **Actions → Build PSP Builder Android APK** y descarga el artefacto `PSPBuilder-debug-apk` cuando termine correctamente.

Requisitos de compilación local: JDK 17, Android SDK 35 y Gradle 8.9.

## Compilar un juego PSP en GitHub Actions

1. Coloca el proyecto completo dentro de `psp-project/` (incluye `Makefile`, fuentes y recursos).
2. Abre **Actions → Compile PSP project with PSPDEV → Run workflow**.
3. Mantén `psp-project` como ruta o indica una subcarpeta dentro de ella.
4. Si termina correctamente, descarga `PSP-EBOOT-*` en Artifacts.

El workflow no puede compilar un ZIP que solo esté en el teléfono: los fuentes deben estar en el repositorio. No añadas tokens, contraseñas ni secretos al código fuente.

## Estructura

- `app/`: aplicación Android.
- `.github/workflows/android-apk.yml`: generación del APK.
- `.github/workflows/psp-build.yml`: compilación PSPDEV remota.
- `psp-project/`: carpeta donde colocar el juego.
- `docs/`: documentación y limitaciones.
