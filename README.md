# Worldbuilder Writer

Aplicación nativa de escritorio para escritura y construcción de mundos.

## Estado

La rama `main` contiene exclusivamente la reconstrucción en C++20 + Qt 6. No depende de navegador, Node, React, Blazor ni .NET.

La aplicación anterior se conserva únicamente como referencia funcional en la rama `legacy-web-reference`; no se reutilizan sus assets, fuentes, iconos ni arquitectura.

## Build

Requisitos:

- CMake 3.24+
- Qt 6.6+ con Widgets, PrintSupport, Multimedia y MultimediaWidgets
- Compilador C++20

```powershell
cmake -S . -B build
cmake --build build --config Release
cmake --install build --config Release --prefix dist
```

En Windows, `qt_generate_deploy_app_script` copia las dependencias necesarias para distribuir la aplicación como programa stand-alone.

Consulta `FUNCTION_INVENTORY.md` para la lista de funciones que deben conservarse y `ARCHITECTURE.md` para la organización nueva.
