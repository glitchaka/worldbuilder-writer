namespace WorldbuilderWriter.Core.Media;

public enum MediaSourceKind
{
    Spotify,
    YouTube,
    LocalFile,
}

public sealed record MediaSourceDescriptor(
    string Id,
    MediaSourceKind Kind,
    string DisplayName,
    string Source,
    string? EmbedSource = null,
    bool RequiresInternet = false);
