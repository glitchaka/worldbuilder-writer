# Worldbuilder Writer — arquitectura nativa

Esta reconstrucción usa como fuente de verdad funcional la aplicación original, pero **no conserva su shell web ni su arquitectura de interfaz**. La versión C# queda descartada. La aplicación de producción es C++20 + Qt 6, local-first y sin navegador, HTML, Electron, Node, React, Blazor ni .NET como base de la UI.

## Dirección de producto definitiva

Worldbuilder Writer debe sentirse como software editorial serio para escritura y worldbuilding. No se aceptan formularios Qt acumulados, controles por defecto sin integrar, mockups estáticos, menús nativos `Archivo / Vista`, paneles permanentes sin función contextual ni diálogos usados como sustituto de un workspace.

El lenguaje global de interfaz es el de los mockups aprobados de **Escritura** y **Tablero de escenas**: shell oscuro/claro coherente, navegación horizontal de módulos, jerarquía tipográfica editorial y superficies de trabajo que ocupan el espacio principal.

## Shell global

La aplicación usa ventana propia sin barra de menú nativa. Debajo de la barra de ventana se mantiene una navegación horizontal compacta:

1. Biblioteca
2. Escritura
3. Tablero
4. Atlas
5. Mapas
6. Revisión
7. Configuración

La búsqueda global se enruta al workspace activo. El título del proyecto se muestra de forma compacta dentro del shell y el estado de guardado permanece visible sin robar superficie de trabajo.

## Módulos

### Biblioteca

- Es la pantalla principal real de la aplicación.
- Proyectos existentes se presentan como tarjetas visuales con portada, título, autor, estado, género, recuentos y modificación.
- Crear, abrir, eliminar e importar proyectos son acciones reales, no tarjetas decorativas.
- No se sustituye por una lista de texto ni por una pantalla de bienvenida vacía.

### Escritura

- El manuscrito es la superficie central.
- Panel izquierdo: índice de capítulos y escenas, navegación y acceso a documentación.
- Centro: editor real de escena con formato, deshacer/rehacer, corrector y referencias semánticas.
- Inspector derecho: contexto de escena, plegable cuando no aporta.
- Las referencias semánticas se marcan **dentro del texto** y abren una tarjeta contextual; desde ella se abre la ficha completa correspondiente.
- El modo enfoque usa fullscreen real y oculta shell, índice e inspector. No usa un diálogo central.

### Tablero de escenas

- Workspace visual por columnas de capítulo.
- Tarjetas de escena con título, POV, ubicación, estado, resumen y palabras.
- Drag and drop real para reordenar dentro de un capítulo y mover escenas entre capítulos.
- Filtros de búsqueda, estado, POV y ubicación.
- No se degrada a una lista maquillada.

### Atlas / documentación

- Navegación de conocimiento a la izquierda: personajes/tramas, Atlas, magia y referencias.
- Atlas: índice + superficie editorial de tarjetas + contexto de conexiones/menciones/notas.
- Personajes: índice buscable, ficha editorial central y contexto de identidad/continuidad.
- Hilos/teorías y cronología: superficies editoriales, no formularios gigantes.
- Sistemas de magia: fundamento, reglas, manifestación y sociedad en tarjetas; riesgos/historia/evidencias en contexto.
- Textos de referencia: índice, editor de fuente y panel de uso/menciones en manuscrito.
- Mapas no se duplican dentro del Atlas porque tienen un estudio dedicado.

### Mapas

El editor cartográfico es un workspace de producción, no un lienzo de garabatos.

- Canvas central con zoom, pan, encaje, retícula y ajuste.
- Herramientas reales de selección, movimiento, elevar/hundir heightmap, cordilleras, biomas, ríos, lagos, rutas, fronteras, regiones, asentamientos, etiquetas y medición.
- Suavizado, erosión y normalización del relieve.
- Generación procedural/heightmap basada en el pipeline nativo adaptado de Azgaar.
- Clima, hidrología, biomas, regiones, asentamientos y rutas derivados del terreno.
- Capas con visibilidad, bloqueo, opacidad, duplicación y orden.
- Biblioteca de assets visual con búsqueda/categorías e importación de PNG/JPG/WebP/SVG; los SVG se rasterizan localmente para portabilidad.
- No se generan ni se incluyen assets cartográficos mediante IA.
- Plantillas de referencia permiten superponer cartografía real o histórica mientras se trabaja, sin convertirla en el mapa final.
- Lenguaje cartográfico basado en costas, curvas de nivel, batimetría, hachures, marcas forestales, asentamientos, marco y rosa/compás; no en líneas torpes y símbolos genéricos.
- Exportación PNG/SVG.

### Revisión

La referencia visual aprobada se organiza como superficie editorial con:

- Resumen
- Consistencia
- Lenguaje
- Estructura
- Estilo
- Notas

El análisis local cubre repeticiones, muletillas, duplicaciones mecánicas y estadísticas reales del manuscrito. Consistencia verifica metadatos estructurales comprobables. Estructura calcula distribución real de capítulos/escenas. Las notas se guardan en el proyecto. Proyecto, maquetación y ambientación permanecen disponibles como herramientas secundarias.

### Configuración / salida

- Apariencia clara/oscura persistente.
- Tipografía/editor y atajos.
- Corrector ortográfico e idioma.
- Autoguardado y copias locales.
- Google Drive cuando está configurado.
- Formatos de salida implementados, sin switches que prometan funciones inexistentes.
- Preferencias cartográficas persistentes.

## Comportamientos globales obligatorios

- Guardado local y autosave sin pérdida de campos desconocidos compatibles.
- Paquete `.wbw` portable y versionado.
- Corrector ortográfico integrado y no destructivo.
- Referencias semánticas inline con popup contextual y navegación a ficha completa.
- Dark mode opcional y persistente.
- Focus/zen fullscreen real.
- Paneles secundarios contextuales y plegables.
- Ningún control visible debe prometer una función que el backend no implemente.

## Organización C++

- `core/`: modelo de dominio y compatibilidad de documento.
- `storage/`: JSON, `.wbw`, adjuntos, autosave, Drive y migraciones.
- `import/`: importación de manuscrito.
- `export/`: salida editorial.
- `ui/`: shell nativo, biblioteca, escritura, planificación, Atlas, revisión, configuración y editor cartográfico.

## Compatibilidad

El documento mantiene IDs estables, campos heredados útiles y datos desconocidos compatibles. Se conservan perfil, personajes, relaciones, teorías, cronología, worldbuilding, magia, textos de referencia, capítulos/escenas, mapas, medios, maquetación y preferencias necesarias para no perder información al abrir o guardar proyectos existentes.

El paquete `.wbw` mantiene `manifest.json`, `project.json` y `media/` para que la aplicación siga siendo local-first y portable.

## Validación de entrega

La aplicación no se considera validada porque simplemente compile. El workflow Windows debe:

1. compilar Release;
2. instalar el bundle standalone con las dependencias Qt;
3. iniciar el ejecutable desplegado;
4. recorrer Biblioteca, Escritura, Tablero, Atlas, Mapas, Revisión y Configuración dentro de un `QApplication` real;
5. finalizar con código 0.

Una compilación verde confirma integridad técnica del bundle; no autoriza a afirmar paridad visual exacta sin inspección visual.