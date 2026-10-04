using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using WorldbuilderWriter.Core.Models;
using WorldbuilderWriter.Core.Services;

namespace WorldbuilderWriter.Infrastructure.Manuscripts;

public sealed class DocxManuscriptImportService : IManuscriptImportService
{
    private const long MaximumBytes = 8 * 1024 * 1024;

    public async Task<ImportedManuscript> ImportAsync(Stream source, string fileName, int startingOrder, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(source);
        var extension = Path.GetExtension(fileName).ToLowerInvariant();
        IReadOnlyList<ImportedLine> lines = extension switch
        {
            ".docx" => await ReadDocxAsync(source, cancellationToken).ConfigureAwait(false),
            ".txt" or ".md" => await ReadTextAsync(source, cancellationToken).ConfigureAwait(false),
            _ => throw new InvalidDataException("El manuscrito debe ser DOCX, TXT o Markdown."),
        };
        return ManuscriptStructureParser.Parse(lines, Path.GetFileNameWithoutExtension(fileName), startingOrder);
    }

    private static async Task<IReadOnlyList<ImportedLine>> ReadTextAsync(Stream source, CancellationToken cancellationToken)
    {
        using var reader = new StreamReader(source, Encoding.UTF8, detectEncodingFromByteOrderMarks: true, leaveOpen: true);
        var content = await reader.ReadToEndAsync(cancellationToken).ConfigureAwait(false);
        return content.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n')
            .Split('\n').Select(line => new ImportedLine(line, false)).ToArray();
    }

    private static async Task<IReadOnlyList<ImportedLine>> ReadDocxAsync(Stream source, CancellationToken cancellationToken)
    {
        await using var buffer = new MemoryStream();
        await source.CopyToAsync(buffer, cancellationToken).ConfigureAwait(false);
        if (buffer.Length > MaximumBytes) throw new InvalidDataException("El DOCX supera el límite de 8 MB.");
        buffer.Position = 0;

        using var archive = new ZipArchive(buffer, ZipArchiveMode.Read, leaveOpen: true);
        var documentEntry = archive.GetEntry("word/document.xml")
            ?? throw new InvalidDataException("El DOCX no contiene un documento principal.");
        var styles = await ReadXmlEntryAsync(archive.GetEntry("word/styles.xml"), cancellationToken).ConfigureAwait(false);
        var document = await ReadXmlEntryAsync(documentEntry, cancellationToken).ConfigureAwait(false)
            ?? throw new InvalidDataException("El DOCX no contiene texto.");

        XNamespace word = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
        var styleNames = ReadStyleNames(styles, word);
        var result = new List<ImportedLine>();
        foreach (var paragraph in document.Descendants(word + "p"))
        {
            cancellationToken.ThrowIfCancellationRequested();
            var text = ReadParagraphText(paragraph, word).Replace('\u00A0', ' ').Trim();
            var styleId = paragraph.Element(word + "pPr")?.Element(word + "pStyle")?.Attribute(word + "val")?.Value ?? string.Empty;
            styleNames.TryGetValue(styleId, out var styleName);
            var isHeading = IsPrimaryHeading(styleId) || IsPrimaryHeading(styleName);
            result.Add(new ImportedLine(text, isHeading));
        }
        return result;
    }

    private static async Task<XDocument?> ReadXmlEntryAsync(ZipArchiveEntry? entry, CancellationToken cancellationToken)
    {
        if (entry is null) return null;
        await using var stream = entry.Open();
        return await XDocument.LoadAsync(stream, LoadOptions.None, cancellationToken).ConfigureAwait(false);
    }

    private static string ReadParagraphText(XElement paragraph, XNamespace word)
    {
        var builder = new StringBuilder();
        foreach (var node in paragraph.Descendants())
        {
            if (node.Name == word + "t") builder.Append(node.Value);
            else if (node.Name == word + "tab") builder.Append('\t');
            else if (node.Name == word + "br" || node.Name == word + "cr") builder.AppendLine();
        }
        return builder.ToString();
    }

    private static Dictionary<string, string> ReadStyleNames(XDocument? styles, XNamespace word)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (styles is null) return result;
        foreach (var style in styles.Descendants(word + "style"))
        {
            var id = style.Attribute(word + "styleId")?.Value;
            var name = style.Element(word + "name")?.Attribute(word + "val")?.Value;
            if (!string.IsNullOrWhiteSpace(id) && !string.IsNullOrWhiteSpace(name)) result[id] = name;
        }
        return result;
    }

    private static bool IsPrimaryHeading(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return false;
        var normalized = value.Replace(" ", string.Empty, StringComparison.Ordinal).ToLowerInvariant();
        return normalized is "title" or "título" or "heading1" or "titulo1" or "título1";
    }
}

internal sealed record ImportedLine(string Text, bool IsHeading);

internal static partial class ManuscriptStructureParser
{
    private sealed record Heading(string Label, string Title);
    private sealed class Section
    {
        public required string Label { get; set; }
        public required string Title { get; set; }
        public List<string> Lines { get; } = [];
    }

    [GeneratedRegex(@"^(?<label>cap(?:í|i)tulo\s+(?:\d+[a-z]?|[ivxlcdm]+|\p{L}+))(?:\s*(?::|—|–|-)\s*|\.\s+|\s+)?(?<title>.*?)[.]?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex ChapterPattern();

    [GeneratedRegex(@"^(?<label>(?:prólogo|prologo|epílogo|epilogo|introducción|introduccion|prefacio|posfacio|interludio(?:\s+(?:\d+[a-z]?|[ivxlcdm]+))?))(?:\s*(?::|—|–|-)\s*|\.\s+|\s+)?(?<title>.*?)[.]?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex SpecialPattern();

    [GeneratedRegex(@"^escena(?:\s+(?:\d+[a-z]?|[ivxlcdm]+|\p{L}+))?(?:\s*(?::|—|–|-)\s*|\.\s+|\s+)?(?<title>.*?)[.]?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex ScenePattern();

    [GeneratedRegex(@"^(?:(?:\*\s*){3,}|(?:#\s*){3,}|(?:[-—–]\s*){3,}|⁂|※)$", RegexOptions.CultureInvariant)]
    private static partial Regex SeparatorPattern();

    public static ImportedManuscript Parse(IReadOnlyList<ImportedLine> source, string fallbackTitle, int startingOrder)
    {
        var sections = new List<Section>();
        Section? current = null;
        var preamble = new List<string>();
        var detected = 0;

        for (var index = 0; index < source.Count; index++)
        {
            var line = source[index];
            if (IsSplitHeading(line.Text) && TryJoinSplitHeading(source, index, out var joined, out var consumed))
            {
                line = new ImportedLine(joined, line.IsHeading);
                index = consumed;
            }

            var heading = ParseHeading(line.Text);
            if (heading is null && line.IsHeading && current is not null && !string.IsNullOrWhiteSpace(line.Text))
            {
                if (current.Lines.All(string.IsNullOrWhiteSpace) && current.Title == "Sin título")
                {
                    current.Title = line.Text.Trim();
                    continue;
                }
                heading = new Heading(InferLabel(sections, current, startingOrder), line.Text.Trim());
            }

            if (heading is not null)
            {
                detected++;
                if (current is not null) sections.Add(current);
                else if (preamble.Any(line => !string.IsNullOrWhiteSpace(line))) sections.Add(new Section { Label = "Preliminares", Title = fallbackTitle }.WithLines(preamble));
                preamble.Clear();
                current = new Section { Label = heading.Label, Title = heading.Title };
                continue;
            }

            if (current is null) preamble.Add(line.Text);
            else current.Lines.Add(line.Text);
        }

        if (current is not null) sections.Add(current);
        else sections.Add(new Section { Label = $"Capítulo {startingOrder + 1}", Title = fallbackTitle }.WithLines(preamble));

        var chapters = sections.Select((section, index) => CreateChapter(section, startingOrder + index)).ToArray();
        return new ImportedManuscript(chapters, detected);
    }

    private static Section WithLines(this Section section, IEnumerable<string> lines)
    {
        section.Lines.AddRange(lines);
        return section;
    }

    private static WritingChapter CreateChapter(Section section, int order)
    {
        var chapter = new WritingChapter { Label = section.Label, Title = section.Title, Order = order };
        chapter.Scenes = CreateScenes(chapter.Id, section.Lines);
        return chapter;
    }

    private static List<WritingScene> CreateScenes(string chapterId, IReadOnlyList<string> lines)
    {
        var scenes = new List<WritingScene>();
        var content = new List<string>();
        var title = string.Empty;

        void Flush()
        {
            if (content.All(string.IsNullOrWhiteSpace) && string.IsNullOrWhiteSpace(title) && scenes.Count > 0) return;
            scenes.Add(new WritingScene
            {
                ChapterId = chapterId,
                Order = scenes.Count,
                Title = string.IsNullOrWhiteSpace(title) ? $"Escena {scenes.Count + 1}" : title,
                Content = string.Join(Environment.NewLine, content).Trim(),
            });
            content.Clear();
            title = string.Empty;
        }

        foreach (var raw in lines)
        {
            var line = raw.TrimEnd();
            if (string.IsNullOrWhiteSpace(line))
            {
                if (content.Any(item => !string.IsNullOrWhiteSpace(item))) Flush();
                continue;
            }
            var scene = ScenePattern().Match(line.Trim());
            if (scene.Success || SeparatorPattern().IsMatch(line.Trim()))
            {
                if (content.Any(item => !string.IsNullOrWhiteSpace(item)) || !string.IsNullOrWhiteSpace(title)) Flush();
                title = scene.Success && !string.IsNullOrWhiteSpace(scene.Groups["title"].Value)
                    ? scene.Groups["title"].Value.Trim()
                    : string.Empty;
                continue;
            }
            content.Add(line);
        }
        if (content.Any(item => !string.IsNullOrWhiteSpace(item)) || !string.IsNullOrWhiteSpace(title) || scenes.Count == 0) Flush();
        return scenes;
    }

    private static Heading? ParseHeading(string raw)
    {
        var line = raw.Trim().TrimStart('#').Trim();
        if (line.Length is 0 or > 180) return null;
        var match = ChapterPattern().Match(line);
        if (!match.Success) match = SpecialPattern().Match(line);
        if (!match.Success) return null;
        var title = match.Groups["title"].Value.Trim();
        return new Heading(match.Groups["label"].Value.Trim(), string.IsNullOrWhiteSpace(title) ? "Sin título" : title);
    }

    private static bool IsSplitHeading(string text) =>
        Regex.IsMatch(text.Trim(), @"^(?:cap(?:í|i)tulo|interludio)[.]?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    private static bool TryJoinSplitHeading(IReadOnlyList<ImportedLine> lines, int index, out string joined, out int consumed)
    {
        joined = string.Empty;
        consumed = index;
        for (var next = index + 1; next < lines.Count; next++)
        {
            if (string.IsNullOrWhiteSpace(lines[next].Text)) continue;
            var continuation = lines[next].Text.Trim();
            if (!Regex.IsMatch(continuation, @"^(?:\d+[a-z]?|[ivxlcdm]+|uno|dos|tres|cuatro|cinco|seis|siete|ocho|nueve|diez)(?:\b|\.)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)) return false;
            joined = $"{lines[index].Text.Trim().TrimEnd('.')} {continuation}";
            consumed = next;
            return true;
        }
        return false;
    }

    private static string InferLabel(IReadOnlyList<Section> sections, Section current, int startingOrder)
    {
        foreach (var label in sections.Select(section => section.Label).Append(current.Label).Reverse())
        {
            var match = Regex.Match(label, @"^cap(?:í|i)tulo\s+(\d+)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
            if (match.Success) return $"Capítulo {int.Parse(match.Groups[1].Value) + 1}";
        }
        return $"Capítulo {startingOrder + sections.Count + 1}";
    }
}
