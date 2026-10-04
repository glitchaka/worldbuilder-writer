# Migración de Worldbuilder Writer a C#

Esta carpeta contiene la nueva base de escritorio. No empaqueta la aplicación web ni guarda obras dentro del programa: abre una carpeta externa elegida por el usuario y trabaja con el mismo `project.json` que la edición portátil actual.

## Estado de esta primera etapa

- Solución en C# sobre .NET 10 LTS y Avalonia 12.
- Ventana sin decoración del sistema, con minimizar, maximizar/restaurar y cerrar dentro de la interfaz.
- Selección de carpeta externa en el primer inicio.
- Biblioteca local que encuentra carpetas `project-*` existentes.
- Creación de obras vacías compatibles con el formato web actual.
- Escritura atómica de un único archivo `project.json`.
- Modelo común para Spotify, YouTube y archivos locales MP3, MP4, OGG y VOB.
- Contrato `IMediaPlaybackService` preparado para conectar el reproductor nativo sin acoplarlo a la interfaz.

La edición React sigue siendo la referencia funcional durante la migración. Las pantallas se trasladarán por módulos: biblioteca y almacenamiento, reproductor, manuscrito, construcción de mundo, tableros y exportadores.

## Requisitos

- SDK de .NET 10.
- Windows, macOS o Linux para ejecutar la interfaz Avalonia.

## Ejecutar

```powershell
dotnet restore WorldbuilderWriter.sln
dotnet run --project src/WorldbuilderWriter.App
```

## Almacenamiento

La única preferencia que se guarda en los datos de la aplicación es la ruta de la carpeta de proyectos. Las historias siempre quedan fuera del ejecutable:

```text
Carpeta elegida/
  project-…/
    project.json
```

Copiar o distribuir el futuro `.exe` no copiará ninguna obra.

## Reproductor nativo

La capa de dominio ya distingue Spotify, YouTube y archivos locales. La siguiente etapa conectará un adaptador multimedia nativo con soporte de códecs amplios —incluido VOB/MPEG-2— y una superficie web restringida para los reproductores oficiales de Spotify y YouTube. YouTube puede insertar publicidad dentro de su reproductor oficial; la aplicación no intentará eludirla.
