# Inventario funcional de la aplicación original

Fuente de verdad: `app/` y `lib/archive-data.ts`. Este inventario es la lista de conservación de funcionalidad para la migración nativa. Nada se elimina sólo porque la interfaz se simplifique.

## Biblioteca y proyecto

- Biblioteca local de obras.
- Crear obra vacía.
- Abrir obra.
- Eliminar obra con confirmación.
- Importar paquete `.wbw`.
- Guardado local automático.
- Estado de guardado.
- Perfil de proyecto: archivo, título, subtítulo, autor, género, estado, sinopsis, ubicación y etiquetas de capítulos.
- Portada, banner e icono de tablero.
- Temas predefinidos y temas personalizados.
- Copias de seguridad/restauración en Google Drive.

## Planificación

### Tablero de personajes
- Fichas posicionables en espacio de mundo.
- Pan y zoom.
- Mostrar/ocultar fichas.
- Relaciones visuales entre personajes.
- Tipos de relación diferenciados visualmente.
- Intensidad/fortaleza de relación.
- Etiqueta, detalle y certeza.
- Vista de red completa y enfocada.

### Personajes
- Nombre e imagen.
- Alias.
- Categoría y estado.
- Rol, ocupación, origen y afiliación.
- Resumen, trasfondo y descripción física.
- Rasgos.
- Evidencias por capítulo.
- Presencia por capítulo.
- Color y posición de tablero.

### Hilos / teorías
- Título.
- Estado.
- Confianza.
- Tesis.
- Evidencias.
- Contraargumento.
- Personajes vinculados.
- Etiquetas.

### Cronología
- Capítulo.
- Fecha/posición temporal libre.
- Título y resumen.
- Personajes asociados.
- Intensidad.
- Ordenación.

## Escritura

### Manuscrito
- Capítulos y escenas.
- Crear, editar, borrar y reordenar.
- Título de escena.
- Texto con formato.
- POV.
- Localización.
- Capa narrativa.
- Estado: borrador/revisión/final.
- Fecha de actualización.
- Tablero de escenas con viewport propio y modo compacto.
- Modo de escritura sin distracciones.

### Importación
- Importación de manuscrito.
- TXT / Markdown.
- DOCX mediante conversión de contenido estructurado.
- Reconstrucción de capítulos y escenas.

### Referencias
- Referencias a personajes.
- Referencias a entradas de mundo.
- Referencias a sistemas de magia.
- Referencias a textos importados.
- Apertura directa del registro referenciado.

### Revisión de escritura
- Corrector del texto.
- Coincidencias con mensaje, contexto y reemplazos.
- Detección/configuración de repeticiones.
- Lista configurable de muletillas/frases de relleno.

### Maquetación
- Presets editorial, novela amplia, bolsillo, fantasía clásica, crónica ilustrada y personalizada.
- Ancho/alto de página.
- Márgenes.
- Familia y tamaño tipográfico.
- Interlineado.
- Sangría.
- Apertura de capítulo.
- Separador de escena.
- Encabezado y pie con variables.
- Previsualización/exportación.

## Mundo

### Atlas
- Tipos: País/Reino, Región, Ciudad/Lugar, Pueblo/Cultura, Moneda, Idioma, Religión, Organización, Objeto/Artefacto, Cosmología, Concepto y Otro.
- Nombre y alias.
- Resumen.
- Geografía.
- Gobierno.
- Pueblos.
- Cultura.
- Economía.
- Moneda.
- Idiomas.
- Religiones.
- Fuerza militar.
- Historia.
- Relaciones.
- Localizaciones.
- Conflictos.
- Notas.
- Etiquetas.
- Adjuntos.

### Mapas
- Múltiples mapas.
- Nombre y descripción.
- Semilla.
- Estilos Pergamino, Atlas y Nocturno.
- Continentes, islas y rugosidad.
- Imagen de fondo opcional.
- Marcadores.
- Tipos de marcador: Capital, Ciudad, Ruina, Puerto, Fortaleza y Lugar.

### Magia
- Nombre y categoría.
- Estado: canónico, en desarrollo, secreto o descartado.
- Fuente.
- Principio.
- Acceso.
- Coste.
- Límites.
- Manifestaciones.
- Materiales.
- Instituciones.
- Usuarios.
- Riesgos.
- Historia.
- Notas.
- Evidencias.
- Etiquetas.
- Adjuntos.

### Textos de referencia
- Textos importados para Mundo.
- Textos importados para Magia.
- Nombre de origen y fecha de importación.

## Multimedia

- Playlist de Spotify.
- Ambientación de YouTube.
- Medios locales MP3, MP4, OGG y VOB.
- Reproductor local de audio/vídeo.

## Salida y portabilidad

- PDF por secciones.
- PDF de proyecto completo.
- Exportación del manuscrito.
- Paquete `.wbw` versionado.
- `manifest.json`.
- `project.json`.
- `media/` con fotos y adjuntos.
- Validación de paquetes y recursos.
- Preservación de campos heredados usados para migración.

## Funciones internas que también deben conservarse

- Normalización/migración de versiones anteriores de datos.
- IDs estables.
- Guardado sin pérdida de campos desconocidos compatibles.
- Límites de tamaño y validación de adjuntos/paquetes.
- Conversión de imágenes/recursos a formato portable cuando corresponda.
- Nombre de descarga seguro.
- Navegación de lienzo consistente entre tableros.
