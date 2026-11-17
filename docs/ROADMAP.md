# Hoja de ruta técnica

## Siguiente bloque
1. Autorizar escritura del conector GitHub o subir este repositorio desde una sesión Git autenticada.
2. Implementar OAuth/token de mínimo alcance sin incrustar secretos en el APK.
3. Subir un paquete fuente a una rama de trabajo y disparar `workflow_dispatch`.
4. Consultar el estado de ejecución y descargar el artefacto EBOOT.
5. Añadir gestión de permisos de almacenamiento y pruebas en Android físico.

## Decisión de seguridad
No se debe incluir un PAT, client secret ni token personal dentro del APK, del repositorio o de los logs. Para una integración de producción se debe usar un flujo OAuth seguro o un servicio intermediario con credenciales protegidas.
