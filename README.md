# PSP Builder para Android — 0.3.0

Aplicación Android nativa para importar un proyecto PSP en ZIP, extraerlo de forma segura, enviarlo a GitHub Actions, compilarlo con PSPDEV y descargar el `EBOOT.PBP` generado.

## Funciones

- Importación de ZIP mediante el selector de documentos de Android.
- Extracción privada con protección contra ZIP Slip, límite de 10.000 archivos y 300 MB descomprimidos.
- Detección de `Makefile`; para compilar debe existir exactamente uno.
- Conexión con GitHub mediante token personal Fine-grained.
- Token cifrado con AES/GCM y clave guardada en Android Keystore.
- Subida de fuentes a una carpeta única `psp-project/mobile-...` del repositorio.
- Lanzamiento del workflow PSPDEV, seguimiento de la ejecución y descarga del artefacto.
- Exportación del archivo final `EBOOT.PBP` a la ubicación que elijas en el móvil.

## Configurar GitHub (una sola vez por dispositivo)

Crea un Fine-grained personal access token en GitHub limitado al repositorio `CaskIAOficial/PSPBuilder`, con estos permisos de repositorio:

- **Contents: Read and write**
- **Actions: Read and write**

Introduce el token en PSP Builder y pulsa **Guardar token cifrado**. No compartas el token ni lo incluyas en ZIP o código. Si revocas el token en GitHub, crea otro y guárdalo en la aplicación.

## Compilar un juego

1. El ZIP debe contener el proyecto fuente de un juego PSP, con su `Makefile`, código y recursos. No sirve un ZIP de un juego ya compilado ni un ISO/CSO.
2. Importa el ZIP y pulsa **Analizar y extraer proyecto**.
3. Guarda el token de GitHub.
4. Pulsa **COMPILAR Y DESCARGAR EBOOT.PBP** y espera a que GitHub Actions termine.
5. Elige dónde guardar `EBOOT.PBP`. Ese es el archivo ejecutable que podrás colocar en una PSP compatible o abrir con PPSSPP.

## Límites actuales de subida desde Android

Para evitar peticiones excesivas a la API de GitHub, esta versión admite hasta 100 archivos, 20 MB en total y 900 KB por archivo. Si el proyecto supera esos límites, habrá que añadir un mecanismo de subida Git Git Data (blobs/trees) o LFS.

## Workflows

- `.github/workflows/android-apk.yml`: compila el APK de Android.
- `.github/workflows/psp-build.yml`: compila el proyecto remoto con la imagen PSPDEV y publica el artefacto `PSP-EBOOT-...`.

El APK de depuración se descarga desde **Actions → Build PSP Builder Android APK → Artifacts**.
