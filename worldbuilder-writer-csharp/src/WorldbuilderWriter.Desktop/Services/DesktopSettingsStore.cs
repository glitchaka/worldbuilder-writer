using System.IO;

namespace WorldbuilderWriter.Desktop.Services;

public sealed class DesktopSettingsStore
{
    private readonly string _settingsPath;

    public DesktopSettingsStore()
    {
        var localData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var directory = Path.Combine(localData, "Worldbuilder Writer");
        _settingsPath = Path.Combine(directory, "workspace.txt");
    }

    public async Task<string?> LoadWorkspaceAsync(CancellationToken cancellationToken = default)
    {
        if (!File.Exists(_settingsPath)) return null;
        var path = (await File.ReadAllTextAsync(_settingsPath, cancellationToken).ConfigureAwait(false)).Trim();
        return string.IsNullOrWhiteSpace(path) ? null : path;
    }

    public async Task SaveWorkspaceAsync(string workspacePath, CancellationToken cancellationToken = default)
    {
        var directory = Path.GetDirectoryName(_settingsPath)
            ?? throw new InvalidOperationException("No se pudo resolver la carpeta de configuración.");
        Directory.CreateDirectory(directory);
        var temporary = $"{_settingsPath}.{Guid.NewGuid():N}.tmp";
        try
        {
            await File.WriteAllTextAsync(temporary, Path.GetFullPath(workspacePath), cancellationToken).ConfigureAwait(false);
            File.Move(temporary, _settingsPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }
    }
}
