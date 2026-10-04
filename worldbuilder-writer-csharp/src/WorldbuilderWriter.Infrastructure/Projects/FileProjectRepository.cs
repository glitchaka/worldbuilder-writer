using System.Text.Json;
using System.Text.RegularExpressions;
using WorldbuilderWriter.Core.Models;
using WorldbuilderWriter.Core.Services;

namespace WorldbuilderWriter.Infrastructure.Projects;

public sealed class FileProjectRepository : IProjectRepository
{
    private const string ProjectFileName = "project.json";
    private static readonly Regex ValidId = new("^project-[a-f0-9]{32}$", RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        PropertyNameCaseInsensitive = true,
        WriteIndented = true,
    };

    public FileProjectRepository(string rootPath)
    {
        RootPath = Path.GetFullPath(rootPath);
        Directory.CreateDirectory(RootPath);
    }

    public string RootPath { get; }

    public async Task<IReadOnlyList<ProjectSummary>> ListAsync(CancellationToken cancellationToken = default)
    {
        var result = new List<ProjectSummary>();
        foreach (var directory in Directory.EnumerateDirectories(RootPath, "project-*", SearchOption.TopDirectoryOnly))
        {
            cancellationToken.ThrowIfCancellationRequested();
            var id = Path.GetFileName(directory);
            if (!IsValidId(id)) continue;
            try
            {
                result.Add(ProjectSummary.From(await ReadAsync(id, cancellationToken).ConfigureAwait(false)));
            }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or InvalidDataException)
            {
                // Una carpeta dañada o ajena no debe bloquear el resto de la biblioteca.
            }
        }

        return result.OrderByDescending(project => project.UpdatedAt).ToArray();
    }

    public async Task<StoryProject> CreateAsync(string title, string theme, CancellationToken cancellationToken = default)
    {
        var project = StoryProject.Create(title, theme);
        Directory.CreateDirectory(ProjectDirectory(project.Id));
        await SaveAsync(project, cancellationToken).ConfigureAwait(false);
        return project;
    }

    public Task<StoryProject> OpenAsync(string projectId, CancellationToken cancellationToken = default) =>
        ReadAsync(projectId, cancellationToken);

    public async Task SaveAsync(StoryProject project, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(project);
        if (!IsValidId(project.Id)) throw new InvalidDataException("El identificador del proyecto no es válido.");
        project.UpdatedAt = DateTimeOffset.UtcNow;
        var directory = ProjectDirectory(project.Id);
        Directory.CreateDirectory(directory);
        var destination = Path.Combine(directory, ProjectFileName);
        var temporary = Path.Combine(directory, $".{ProjectFileName}.{Guid.NewGuid():N}.tmp");
        try
        {
            await using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None, 64 * 1024, FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(stream, project, JsonOptions, cancellationToken).ConfigureAwait(false);
                await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
            }
            File.Move(temporary, destination, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }
    }

    public Task DeleteAsync(string projectId, CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var directory = ProjectDirectory(projectId);
        if (Directory.Exists(directory)) Directory.Delete(directory, recursive: true);
        return Task.CompletedTask;
    }

    private async Task<StoryProject> ReadAsync(string projectId, CancellationToken cancellationToken)
    {
        var path = Path.Combine(ProjectDirectory(projectId), ProjectFileName);
        await using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 64 * 1024, FileOptions.Asynchronous | FileOptions.SequentialScan);
        var project = await JsonSerializer.DeserializeAsync<StoryProject>(stream, JsonOptions, cancellationToken).ConfigureAwait(false);
        if (project is null || !IsValidId(project.Id)) throw new InvalidDataException("El archivo no contiene un proyecto compatible.");
        Normalize(project);
        return project;
    }

    private string ProjectDirectory(string projectId)
    {
        if (!IsValidId(projectId)) throw new InvalidDataException("El identificador del proyecto no es válido.");
        var path = Path.GetFullPath(Path.Combine(RootPath, projectId));
        var relative = Path.GetRelativePath(RootPath, path);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative)) throw new InvalidDataException("La ruta sale de la carpeta de proyectos.");
        return path;
    }

    private static bool IsValidId(string? value) => value is not null && ValidId.IsMatch(value);

    private static void Normalize(StoryProject project)
    {
        project.Profile ??= new ProjectProfile();
        project.Characters ??= [];
        project.Relationships ??= [];
        project.Chapters ??= [];
        project.World ??= [];
        project.MagicSystems ??= [];
        project.Timeline ??= [];
        project.Profile.Theme = ThemeRules.Normalize(project.Profile.Theme);
        foreach (var chapter in project.Chapters.OrderBy(chapter => chapter.Order))
        {
            chapter.Scenes ??= [];
            for (var index = 0; index < chapter.Scenes.Count; index++)
            {
                chapter.Scenes[index].ChapterId = chapter.Id;
                chapter.Scenes[index].Order = index;
            }
        }
    }
}

public static class ProjectRootResolver
{
    public static string Resolve(string? configuredPath)
    {
        if (!string.IsNullOrWhiteSpace(configuredPath)) return Path.GetFullPath(configuredPath);
        var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        var localData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var basePath = !string.IsNullOrWhiteSpace(documents)
            ? documents
            : !string.IsNullOrWhiteSpace(localData)
                ? localData
                : userProfile;
        if (string.IsNullOrWhiteSpace(basePath)) throw new InvalidOperationException("No se pudo resolver una carpeta externa para los proyectos.");
        return Path.Combine(basePath, "Worldbuilder Writer", "Projects");
    }
}
