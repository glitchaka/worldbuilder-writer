# Worldbuilder Writer

Editor de escritura y construcción de mundos para novelas. Reúne manuscrito por capítulos y escenas, fichas, relaciones visuales, cronología, atlas, sistemas de magia, mapas, maquetación, revisión de estilo y exportaciones editoriales.

## Estado del código

La base activa está en `worldbuilder-writer-csharp/`. Incluye la aplicación local WPF, que genera un ejecutable nativo de Windows, y la versión web ASP.NET Core/Blazor. Todo el desarrollo nuevo se realiza en C#.

La antigua edición React/TypeScript permanece únicamente como fuente heredada del despliegue actual de ChatGPT Sites; no es la base de desarrollo ni debe usarse para continuar la aplicación. `desktop-csharp/`, `desktop-web/` y `desktop-native/` son prototipos anteriores y quedaron reemplazados por la solución nueva.

## Desarrollo local en C#

Requisitos:

- SDK de .NET 10.
- Windows para ejecutar la interfaz WPF.

Comandos principales:

```powershell
cd worldbuilder-writer-csharp
dotnet restore src/WorldbuilderWriter.Desktop/WorldbuilderWriter.Desktop.csproj
dotnet run --project src/WorldbuilderWriter.Desktop
```

Para crear el ejecutable autocontenido de Windows se usa `worldbuilder-writer-csharp/publish-windows.ps1`.

## Paquetes de proyecto `.wbw`

`.wbw` es un ZIP versionado que se importa directamente desde la biblioteca de Worldbuilder Writer. Incluye:

- `manifest.json`: versión del formato e inventario de recursos.
- `project.json`: todos los datos editables del proyecto.
- `media/`: fotografías de personajes e imágenes de Mundo y Magia.

El formato actual se identifica como `worldbuilder-writer-project`, versión 1. La exportación JSON antigua dejó de ser la copia visible de proyecto.

## Organización

- `worldbuilder-writer-csharp/src/WorldbuilderWriter.Desktop/`: aplicación local WPF.
- `worldbuilder-writer-csharp/src/WorldbuilderWriter.Web/`: versión web en ASP.NET Core/Blazor.
- `worldbuilder-writer-csharp/src/WorldbuilderWriter.Core/`: modelos compartidos.
- `worldbuilder-writer-csharp/src/WorldbuilderWriter.Infrastructure/`: almacenamiento e importación.
- `app/`, `lib/`, `db/` y `drizzle/`: edición web heredada de Sites.
- `desktop-csharp/`, `desktop-web/` y `desktop-native/`: prototipos reemplazados.
