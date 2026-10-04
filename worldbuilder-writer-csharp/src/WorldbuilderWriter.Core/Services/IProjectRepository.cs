using WorldbuilderWriter.Core.Models;

namespace WorldbuilderWriter.Core.Services;

public interface IProjectRepository
{
    string RootPath { get; }
    Task<IReadOnlyList<ProjectSummary>> ListAsync(CancellationToken cancellationToken = default);
    Task<StoryProject> CreateAsync(string title, string theme, CancellationToken cancellationToken = default);
    Task<StoryProject> OpenAsync(string projectId, CancellationToken cancellationToken = default);
    Task SaveAsync(StoryProject project, CancellationToken cancellationToken = default);
    Task DeleteAsync(string projectId, CancellationToken cancellationToken = default);
}
