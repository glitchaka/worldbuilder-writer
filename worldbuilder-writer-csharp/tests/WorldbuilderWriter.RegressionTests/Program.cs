using System.IO.Compression;
using System.Xml.Linq;
using WorldbuilderWriter.Infrastructure.Manuscripts;
using WorldbuilderWriter.Infrastructure.Projects;
using WorldbuilderWriter.Core.Models;

var manuscriptPath = CreateSyntheticManuscript();
try
{
    var importer = new DocxManuscriptImportService();
    await using var manuscriptStream = File.OpenRead(manuscriptPath);
    var manuscript = await importer.ImportAsync(manuscriptStream, Path.GetFileName(manuscriptPath), 0);

    Expect(manuscript.Chapters.Count == 11, $"Se esperaban 11 secciones y se obtuvieron {manuscript.Chapters.Count}.");
    Expect(ContainsLabel(manuscript, "capítulo 4"), "No se detectó el capítulo 4 dividido en dos párrafos.");
    Expect(ContainsLabel(manuscript, "capítulo 6"), "No se detectó el capítulo 6.");
    Expect(ContainsLabel(manuscript, "interludio"), "No se detectó el interludio.");
    var firstChapter = manuscript.Chapters.First(chapter => chapter.Label.Contains("1", StringComparison.Ordinal));
    Expect(firstChapter.Scenes.Count == 2, "El renglón vacío no se convirtió en un salto de escena.");
}
finally
{
    File.Delete(manuscriptPath);
}

var projectRoot = Path.Combine(Path.GetTempPath(), $"wbw-repository-{Guid.NewGuid():N}");
try
{
    var repository = new FileProjectRepository(projectRoot);
    var project = await repository.CreateAsync("Prueba externa", "grim");
    project.Profile.Author = "Prueba de regresión";
    project.Characters.Add(new CharacterRecord { Name = "Personaje", PhotoPath = "media/characters/personaje.png" });
    await repository.SaveAsync(project);

    var loaded = await repository.OpenAsync(project.Id);
    Expect(loaded.Profile.Author == "Prueba de regresión", "El proyecto externo no se guardó correctamente.");
    Expect(loaded.Characters.Single().PhotoPath == "media/characters/personaje.png", "La ruta relativa de fotografía no se conservó.");
    var projectDirectory = Path.Combine(projectRoot, project.Id);
    var storedFiles = Directory.GetFiles(projectDirectory).Select(path => Path.GetFileName(path) ?? string.Empty).ToArray();
    Expect(storedFiles.SequenceEqual(["project.json"]), $"Se encontraron archivos inesperados: {string.Join(", ", storedFiles)}");
    Expect(!Directory.EnumerateFiles(projectRoot, "*backup*", SearchOption.AllDirectories).Any(), "Se creó un archivo de respaldo no solicitado.");

    await repository.DeleteAsync(project.Id);
    Expect(!Directory.Exists(projectDirectory), "La eliminación del proyecto no quitó su carpeta externa.");
}
finally
{
    if (Directory.Exists(projectRoot)) Directory.Delete(projectRoot, recursive: true);
}

Console.WriteLine("Pruebas de regresión superadas: importación DOCX, saltos de escena y almacenamiento externo.");

static bool ContainsLabel(WorldbuilderWriter.Core.Services.ImportedManuscript manuscript, string expected) =>
    manuscript.Chapters.Any(chapter => chapter.Label.Contains(expected, StringComparison.OrdinalIgnoreCase));

static void Expect(bool condition, string message)
{
    if (!condition) throw new InvalidDataException(message);
}

static string CreateSyntheticManuscript()
{
    var path = Path.Combine(Path.GetTempPath(), $"wbw-manuscript-{Guid.NewGuid():N}.docx");
    XNamespace word = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
    var headings = new[]
    {
        "PRÓLOGO — Antes del barro",
        "CAPÍTULO 1 — El comienzo",
        "CAPÍTULO 2. Carnaval",
        "CAPÍTULO III Alvargio",
        "CAPÍTULO",
        "4. Cuarto capítulo",
        "CAPÍTULO 5 — El Pósito",
        "CAPÍTULO 6 — Sexto capítulo",
        "INTERLUDIO — Bajo las estrellas",
        "CAPÍTULO 7 — Umbrales",
        "CAPÍTULO 8 — Por un trago",
        "CAPÍTULO 9 — Felisa",
    };
    var body = new XElement(word + "body");
    foreach (var heading in headings)
    {
        body.Add(new XElement(
            word + "p",
            new XElement(word + "pPr", new XElement(word + "pStyle", new XAttribute(word + "val", "Heading1"))),
            new XElement(word + "r", new XElement(word + "t", heading))));
        if (heading == "CAPÍTULO") continue;
        body.Add(new XElement(word + "p", new XElement(word + "r", new XElement(word + "t", $"Texto de {heading}."))));
        if (heading.StartsWith("CAPÍTULO 1", StringComparison.Ordinal))
        {
            body.Add(new XElement(word + "p"));
            body.Add(new XElement(word + "p", new XElement(word + "r", new XElement(word + "t", "Segunda escena."))));
        }
    }

    var document = new XDocument(new XElement(word + "document", body));
    var styles = new XDocument(new XElement(
        word + "styles",
        new XElement(
            word + "style",
            new XAttribute(word + "styleId", "Heading1"),
            new XElement(word + "name", new XAttribute(word + "val", "heading 1")))));

    using var archive = ZipFile.Open(path, ZipArchiveMode.Create);
    WriteEntry(archive, "word/document.xml", document);
    WriteEntry(archive, "word/styles.xml", styles);
    return path;
}

static void WriteEntry(ZipArchive archive, string name, XDocument document)
{
    var entry = archive.CreateEntry(name, CompressionLevel.SmallestSize);
    using var stream = entry.Open();
    document.Save(stream);
}
