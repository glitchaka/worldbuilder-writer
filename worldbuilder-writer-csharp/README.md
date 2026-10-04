# Worldbuilder Writer — C# de escritorio y web

Esta es la nueva base permanente de Worldbuilder Writer. El programa principal es una aplicación local WPF escrita en C# que genera un `.exe` de Windows y no necesita navegador. La versión web secundaria usa C#, ASP.NET Core y componentes Razor de Blazor. Este proyecto no contiene React, Node, npm ni archivos TypeScript.

## Arquitectura

- `WorldbuilderWriter.Core`: modelos de proyectos, personajes, relaciones, capítulos, escenas, mundo, magia y cronología.
- `WorldbuilderWriter.Infrastructure`: almacenamiento externo atómico en `project.json` e importación de DOCX/TXT/Markdown.
- `WorldbuilderWriter.Desktop`: aplicación local WPF; es el proyecto principal para producir el ejecutable.
- `WorldbuilderWriter.UI`: componentes Razor de la versión web.
- `WorldbuilderWriter.Web`: servidor web ASP.NET Core y host Blazor interactivo.
- `WorldbuilderWriter.RegressionTests`: comprobaciones ejecutables sin paquetes externos para importación y persistencia.

## Funciones incluidas en la aplicación local

- Biblioteca de proyectos vacía al distribuir la aplicación.
- Creación, apertura y eliminación confirmada de proyectos.
- Proyectos guardados en una carpeta externa al programa.
- Temas de fantasía sucia, fantasía clásica, moe kawaii, crónica y escritorio.
- Tablero de personajes con zoom y enlaces manuales diferenciados por tipo, color y trazo.
- Manuscrito subdividido en capítulos y escenas.
- Tablero de escenas con reordenamiento por arrastre, zoom y papelera confirmada.
- Eliminación rápida desde el índice y purga total de capítulos.
- Importación de DOCX, TXT y Markdown.
- Los párrafos vacíos del documento importado se convierten en saltos de escena.
- Creación de mundo, sistemas de magia y cronología editables.
- Ventana nativa sin decorado del sistema, con cerrar, minimizar y maximizar dentro de la aplicación.
- Modo de escritura sin distracciones a pantalla completa.

## Requisitos

- SDK de .NET 10.
- Windows, Linux o macOS para desarrollo y servidor web.

## Abrir la aplicación local

En Windows puedes hacer doble clic en `run-desktop.cmd`. También puedes abrir `WorldbuilderWriter.sln` en Visual Studio y establecer `WorldbuilderWriter.Desktop` como proyecto de inicio.

Desde PowerShell:

```powershell
dotnet restore src/WorldbuilderWriter.Desktop/WorldbuilderWriter.Desktop.csproj
dotnet run --project src/WorldbuilderWriter.Desktop --no-restore
```

## Ejecutar la versión web

```powershell
dotnet restore src/WorldbuilderWriter.Web/WorldbuilderWriter.Web.csproj
dotnet run --project src/WorldbuilderWriter.Web --no-restore
```

La aplicación se abre en `http://localhost:5187` durante desarrollo.

Para ejecutar las regresiones de importación y almacenamiento:

```powershell
dotnet run --project tests/WorldbuilderWriter.RegressionTests
```

## Carpeta externa de proyectos

Sin configuración adicional, los proyectos se guardan en:

```text
Documentos/
  Worldbuilder Writer/
    Projects/
      project-…/
        project.json
```

Para usar otra ubicación en el servidor, define `WorldbuilderWriter__ProjectRoot`:

```powershell
$env:WorldbuilderWriter__ProjectRoot = "D:\Mis historias"
dotnet run --project src/WorldbuilderWriter.Web
```

La escritura segura usa únicamente un archivo temporal oculto durante el guardado y lo sustituye de forma atómica por `project.json`.

## Crear el ejecutable distribuible

Ejecuta:

```powershell
.\publish-windows.ps1
```

El ejecutable local autocontenido queda en `artifacts/win-x64/WorldbuilderWriter.exe`. No necesita que la persona que lo reciba instale .NET. La versión web se publica por separado con `publish-web-windows.ps1`.
