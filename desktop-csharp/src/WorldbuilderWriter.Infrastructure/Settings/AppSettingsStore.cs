using System.Text.Json;
using WorldbuilderWriter.Infrastructure.Serialization;

namespace WorldbuilderWriter.Infrastructure.Settings;

public sealed record AppSettings(string? WorkspacePath);

public sealed class AppSettingsStore
{
    private readonly string _settingsPath;

    public AppSettingsStore(string? settingsRoot = null)
    {
        var root = settingsRoot ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Worldbuilder Writer");
        Directory.CreateDirectory(root);
        _settingsPath = Path.Combine(root, "settings.json");
    }

    public async Task<AppSettings> LoadAsync(CancellationToken cancellationToken = default)
    {
        if (!File.Exists(_settingsPath)) return new AppSettings(null);
        try
        {
            await using var stream = File.OpenRead(_settingsPath);
            return await JsonSerializer.DeserializeAsync<AppSettings>(stream, JsonDefaults.Options, cancellationToken).ConfigureAwait(false) ?? new AppSettings(null);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        {
            return new AppSettings(null);
        }
    }

    public async Task SaveWorkspaceAsync(string workspacePath, CancellationToken cancellationToken = default)
    {
        var fullPath = Path.GetFullPath(workspacePath);
        Directory.CreateDirectory(fullPath);
        var temporaryPath = $"{_settingsPath}.{Guid.NewGuid():N}.tmp";
        try
        {
            await using (var stream = new FileStream(temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None, 4096, FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(stream, new AppSettings(fullPath), JsonDefaults.Options, cancellationToken).ConfigureAwait(false);
                await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
            }
            File.Move(temporaryPath, _settingsPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporaryPath)) File.Delete(temporaryPath);
        }
    }
}
