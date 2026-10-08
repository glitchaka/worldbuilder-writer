# Worldbuilder Writer — arquitectura nativa

Esta reconstrucción usa como fuente de verdad la aplicación original de `app/` y `lib/archive-data.ts`. La implementación C# no forma parte del diseño ni de la migración.

## Objetivo

Aplicación de escritorio nativa, local-first y sin navegador/runtime web. La interfaz se organiza alrededor de la tarea del escritor y no alrededor de cada función técnica.

## Navegación principal

### 1. Planificación
- Tablero de personajes y relaciones
- Personajes
- Hilos / teorías
- Cronología

### 2. Escritura
- Manuscrito por capítulos y escenas
- Metadatos de escena: POV, ubicación, capa narrativa y estado
- Análisis de escritura
- Corrector
- Referencias cruzadas a personajes, mundo, magia y textos importados
- Modo enfoque sin distracciones
- Maquetación / previsualización

### 3. Mundo
- Atlas / entradas de worldbuilding
- Mapas y marcadores
- Sistemas de magia
- Textos importados de referencia
- Adjuntos e imágenes

### 4. Revisión y salida
- Revisión global
- Exportación de manuscrito
- Exportación PDF
- Paquete de proyecto `.wbw`
- Copia/restauración del proyecto

## Elementos globales

Se mantienen fuera de la navegación principal para no contaminar la escritura:
- Biblioteca de proyectos
- Buscar / ir a
- Guardado y estado del proyecto
- Tema visual
- Preferencias
- Multimedia opcional
- Modo enfoque

## Principios de interfaz

1. Una sola barra lateral compacta; no duplicar navegación con pestañas superiores.
2. Acciones secundarias aparecen sólo cuando el contexto las necesita.
3. El manuscrito ocupa el máximo espacio disponible.
4. Metadatos y herramientas viven en un inspector lateral plegable.
5. Tableros, mapas y relaciones usan lienzo dedicado; no se mezclan con formularios de edición.
6. Los controles de formato sólo aparecen al editar texto.
7. Multimedia, temas y exportación no ocupan espacio permanente.
8. Ninguna función de la aplicación original se elimina por simplificación visual.

## Módulos C++

- `core/`: modelo de dominio compatible con `ArchiveState`.
- `storage/`: JSON, `.wbw`, adjuntos, autosave y migraciones.
- `manuscript/`: capítulos, escenas, edición rica, referencias y análisis.
- `planning/`: personajes, relaciones, teorías y cronología.
- `world/`: atlas, mapas, magia, importados y adjuntos.
- `review/`: corrección, revisión, maquetación y exportaciones.
- `ui/`: ventana principal, navegación, inspector, comandos y modo enfoque.

## Compatibilidad

El formato de proyecto debe conservar todos los campos de `ArchiveState`, incluidos:
- perfil y temas personalizados;
- personajes, relaciones, teorías y cronología;
- worldbuilding, magia y adjuntos;
- capítulos y escenas;
- mapas y marcadores;
- textos importados;
- maquetación y análisis de escritura;
- viewports de tableros;
- datos de migración antiguos que deban preservarse al guardar.

El paquete `.wbw` mantiene `manifest.json`, `project.json` y `media/` para evitar pérdida de datos al pasar entre la edición original y la nativa.
