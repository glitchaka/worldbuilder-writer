namespace WorldbuilderWriter.Core.Media;

public static class MediaSourceRules
{
    private static readonly HashSet<string> LocalExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".mp3", ".mp4", ".ogg", ".vob",
    };

    public static bool IsSupportedLocalFile(string path) =>
        !string.IsNullOrWhiteSpace(path) && LocalExtensions.Contains(Path.GetExtension(path));

    public static MediaSourceDescriptor CreateLocalFile(string path)
    {
        var fullPath = Path.GetFullPath(path);
        if (!File.Exists(fullPath)) throw new FileNotFoundException("No se encontró el archivo multimedia.", fullPath);
        if (!IsSupportedLocalFile(fullPath)) throw new NotSupportedException("Usa un archivo MP3, MP4, OGG o VOB.");
        return new MediaSourceDescriptor(Guid.NewGuid().ToString("N"), MediaSourceKind.LocalFile, Path.GetFileName(fullPath), fullPath);
    }

    public static MediaSourceDescriptor CreateSpotify(string value)
    {
        var raw = value.Trim();
        string playlistId;
        if (raw.StartsWith("spotify:playlist:", StringComparison.OrdinalIgnoreCase))
        {
            playlistId = raw["spotify:playlist:".Length..];
        }
        else if (Uri.TryCreate(raw, UriKind.Absolute, out var uri) && uri.Host.Equals("open.spotify.com", StringComparison.OrdinalIgnoreCase))
        {
            var parts = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
            var marker = Array.FindLastIndex(parts, part => part.Equals("playlist", StringComparison.OrdinalIgnoreCase));
            playlistId = marker >= 0 && marker + 1 < parts.Length ? parts[marker + 1] : string.Empty;
        }
        else
        {
            throw new FormatException("El enlace no corresponde a una playlist de Spotify.");
        }

        if (!SafeIdentifier(playlistId)) throw new FormatException("El identificador de la playlist de Spotify no es válido.");
        return new MediaSourceDescriptor(
            Guid.NewGuid().ToString("N"),
            MediaSourceKind.Spotify,
            "Spotify",
            raw,
            $"https://open.spotify.com/embed/playlist/{playlistId}",
            true);
    }

    public static MediaSourceDescriptor CreateYouTube(string value)
    {
        var raw = value.Trim();
        if (!Uri.TryCreate(raw, UriKind.Absolute, out var uri)) throw new FormatException("El enlace de YouTube no es válido.");
        var host = uri.Host.ToLowerInvariant();
        if (host.StartsWith("www.", StringComparison.Ordinal)) host = host[4..];
        if (host is not ("youtube.com" or "m.youtube.com" or "music.youtube.com" or "youtube-nocookie.com" or "youtu.be"))
            throw new FormatException("El enlace no pertenece a YouTube.");

        var query = ParseQuery(uri.Query);
        query.TryGetValue("list", out var listId);
        var videoId = string.Empty;
        var parts = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (host == "youtu.be") videoId = parts.FirstOrDefault() ?? string.Empty;
        else if (uri.AbsolutePath.Equals("/watch", StringComparison.OrdinalIgnoreCase)) query.TryGetValue("v", out videoId);
        else
        {
            var marker = Array.FindIndex(parts, part => part is "embed" or "shorts" or "live");
            if (marker >= 0 && marker + 1 < parts.Length && parts[marker + 1] != "videoseries") videoId = parts[marker + 1];
        }

        var safeVideo = SafeIdentifier(videoId) ? videoId : string.Empty;
        var safeList = SafeIdentifier(listId) ? listId : string.Empty;
        if (safeVideo.Length == 0 && safeList.Length == 0) throw new FormatException("El enlace no contiene un video o una playlist de YouTube.");

        var embed = safeVideo.Length > 0
            ? $"https://www.youtube-nocookie.com/embed/{Uri.EscapeDataString(safeVideo)}?rel=0{(safeList.Length > 0 ? $"&list={Uri.EscapeDataString(safeList)}" : string.Empty)}"
            : $"https://www.youtube-nocookie.com/embed/videoseries?list={Uri.EscapeDataString(safeList)}&rel=0";
        return new MediaSourceDescriptor(Guid.NewGuid().ToString("N"), MediaSourceKind.YouTube, "YouTube", raw, embed, true);
    }

    private static bool SafeIdentifier(string? value) =>
        !string.IsNullOrWhiteSpace(value) && value.Length is >= 6 and <= 80 && value.All(character => char.IsAsciiLetterOrDigit(character) || character is '_' or '-');

    private static Dictionary<string, string> ParseQuery(string query)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var pair in query.TrimStart('?').Split('&', StringSplitOptions.RemoveEmptyEntries))
        {
            var parts = pair.Split('=', 2);
            result[Uri.UnescapeDataString(parts[0])] = parts.Length > 1 ? Uri.UnescapeDataString(parts[1]) : string.Empty;
        }
        return result;
    }
}
