using System.Text.Json;
using System.Text.RegularExpressions;
using WorldbuilderWriter.Core.Projects;
using WorldbuilderWriter.Infrastructure.Serialization;

namespace WorldbuilderWriter.Infrastructure.Projects;

public sealed class ProjectFolderRepository : IProjectRepository
{
    private const string ProjectFileName = "project.json";
    private const string ObsoleteBackupFileName = "project.backup.json";
    private static readonly Regex ProjectIdPattern = new("^project-[a-z0-9-]{8,72}$", RegexOptions.Compiled | RegexOptions.CultureInvariant | RegexOptions.IgnoreCase);

    public ProjectFolderRepository(string workspacePath)
    {
        if (string.IsNullOrWhiteSpace(workspacePath)) throw new ArgumentException("La carpeta de proyectos no puede estar vacía.", nameof(workspacePath));
        WorkspacePath = Path.GetFullPath(workspacePath);
        Directory.CreateDirectory(WorkspacePath);
    }

    public string WorkspacePath { get; }

    public async Task<IReadOnlyList<ProjectSummary>> ListAsync(CancellationToken cancellationToken = default)
    {
        var projects = new List<ProjectSummary>();
        foreach (var directory in Directory.EnumerateDirectories(WorkspacePath, "project-*", SearchOption.TopDirectoryOnly))
        {
            cancellationToken.ThrowIfCancellationRequested();
            var projectId = Path.GetFileName(directory);
            if (!IsValidProjectId(projectId)) continue;
            try
            {
                projects.Add(ProjectSummary.From(await ReadProjectAsync(directory, cancellationToken).ConfigureAwait(false)));
            }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or InvalidDataException)
            {
                // Una carpeta ajena o incompleta no debe bloquear toda la biblioteca.
            }
        }

        return projects.OrderByDescending(project => project.UpdatedAt).ToArray();
    }

    public Task<ProjectEnvelope> OpenAsync(string projectId, CancellationToken cancellationToken = default) =>
        ReadProjectAsync(ProjectDirectory(projectId), cancellationToken);

    public async Task<ProjectEnvelope> CreateAsync(string archiveTitle, string? storyTitle = null, string theme = "grim", CancellationToken cancellationToken = default)
    {
        var project = ArchiveStateFactory.CreateBlank(archiveTitle, storyTitle, theme);
        var directory = ProjectDirectory(project.Id);
        Directory.CreateDirectory(directory);
        try
        {
            await WriteProjectAsync(directory, project, cancellationToken).ConfigureAwait(false);
            return project;
        }
        catch
        {
            if (Directory.Exists(directory) && !Directory.EnumerateFileSystemEntries(directory).Any()) Directory.Delete(directory);
            throw;
        }
    }

    public async Task SaveAsync(ProjectEnvelope project, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(project);
        if (!IsValidProjectId(project.Id)) throw new InvalidDataException("El identificador del proyecto no es válido.");
        project.UpdatedAt = DateTimeOffset.UtcNow;
        await WriteProjectAsync(ProjectDirectory(project.Id), project, cancellationToken).ConfigureAwait(false);
    }

    public Task DeleteAsync(string projectId, CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var directory = ProjectDirectory(projectId);
        if (Directory.Exists(directory)) Directory.Delete(directory, recursive: true);
        return Task.CompletedTask;
    }

    private async Task<ProjectEnvelope> ReadProjectAsync(string directory, CancellationToken cancellationToken)
    {
        DeleteObsoleteProjectFiles(directory);
        var primaryPath = Path.Combine(directory, ProjectFileName);
        return await DeserializeAsync(primaryPath, cancellationToken).ConfigureAwait(false);
    }

    private static async Task<ProjectEnvelope> DeserializeAsync(string path, CancellationToken cancellationToken)
    {
        await using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 64 * 1024, FileOptions.Asynchronous | FileOptions.SequentialScan);
        var project = await JsonSerializer.DeserializeAsync<ProjectEnvelope>(stream, JsonDefaults.Options, cancellationToken).ConfigureAwait(false);
        if (project?.State is null || !IsValidProjectId(project.Id)) throw new InvalidDataException("El archivo no contiene un proyecto compatible.");
        return project;
    }

    private static async Task WriteProjectAsync(string directory, ProjectEnvelope project, CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(directory);
        DeleteObsoleteProjectFiles(directory);
        var primaryPath = Path.Combine(directory, ProjectFileName);
        var temporaryPath = Path.Combine(directory, $".{ProjectFileName}.{Guid.NewGuid():N}.tmp");

        try
        {
            await using (var stream = new FileStream(temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None, 64 * 1024, FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(stream, project, JsonDefaults.Options, cancellationToken).ConfigureAwait(false);
                await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
            }

            File.Move(temporaryPath, primaryPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporaryPath)) File.Delete(temporaryPath);
        }
    }

    private static void DeleteObsoleteProjectFiles(string directory)
    {
        var obsoleteBackupPath = Path.Combine(directory, ObsoleteBackupFileName);
        if (File.Exists(obsoleteBackupPath)) File.Delete(obsoleteBackupPath);
    }

    private string ProjectDirectory(string projectId)
    {
        if (!IsValidProjectId(projectId)) throw new InvalidDataException("El identificador del proyecto no es válido.");
        var fullPath = Path.GetFullPath(Path.Combine(WorkspacePath, projectId));
        var relative = Path.GetRelativePath(WorkspacePath, fullPath);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative)) throw new InvalidDataException("La ruta del proyecto sale de la carpeta elegida.");
        return fullPath;
    }

    private static bool IsValidProjectId(string? projectId) =>
        projectId is not null && ProjectIdPattern.IsMatch(projectId);
}
