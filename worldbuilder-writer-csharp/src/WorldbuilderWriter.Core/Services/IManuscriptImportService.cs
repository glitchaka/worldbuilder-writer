using WorldbuilderWriter.Core.Models;

namespace WorldbuilderWriter.Core.Services;

public sealed record ImportedManuscript(IReadOnlyList<WritingChapter> Chapters, int DetectedHeadings);

public interface IManuscriptImportService
{
    Task<ImportedManuscript> ImportAsync(Stream source, string fileName, int startingOrder, CancellationToken cancellationToken = default);
}
