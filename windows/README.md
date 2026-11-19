# PSP Builder para Windows

Aplicación Windows independiente. El código Android y sus workflows no forman parte de este proyecto.

## Compilación

El workflow `.github/workflows/windows-exe.yml` prepara un toolchain PSPDEV para Windows, compila un proyecto de prueba que debe generar un `EBOOT.PBP` real y empaqueta la aplicación como `PSP Builder.exe`.

El EXE incorpora los paquetes comprimidos de PSPDEV y del runtime MSYS2. En el primer arranque los extrae en `%LOCALAPPDATA%\PSPBuilder\runtime`; no instala servicios ni modifica el PATH global.

## PSPDEV para Windows

Se utiliza el paquete de `dmang-dev/pspdev-win` v2 (PSPDEV 15.2.0), publicado bajo licencia MIT. Su toolchain usa ejecutables alojados en MSYS2 y necesita el runtime MSYS2 y GNU Make. El workflow incorpora esos componentes al EXE para evitar que el usuario tenga que instalarlos.

La compatibilidad de la herramienta se valida en GitHub Actions mediante una compilación real del proyecto de prueba `tests/hello`. No se debe distribuir el ejecutable como funcional hasta que ese smoke test y la compilación del EXE terminen correctamente.

## Límites de importación

- ZIP de hasta 10.000 archivos.
- Hasta 300 MB descomprimidos.
- Rechazo de rutas absolutas, saltos de directorio y enlaces simbólicos.
- El ZIP original nunca se modifica.
- Cada compilación se realiza en una carpeta temporal propia.
- El EBOOT resultante se guarda en Documentos\PSP Builder\EBOOT, sin sobrescribir resultados anteriores.
