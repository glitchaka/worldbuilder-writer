namespace WorldbuilderWriter.Core.Projects;

public interface IProjectRepository
{
    string WorkspacePath { get; }

    Task<IReadOnlyList<ProjectSummary>> ListAsync(CancellationToken cancellationToken = default);

    Task<ProjectEnvelope> OpenAsync(string projectId, CancellationToken cancellationToken = default);

    Task<ProjectEnvelope> CreateAsync(string archiveTitle, string? storyTitle = null, string theme = "grim", CancellationToken cancellationToken = default);

    Task SaveAsync(ProjectEnvelope project, CancellationToken cancellationToken = default);

    Task DeleteAsync(string projectId, CancellationToken cancellationToken = default);
}
