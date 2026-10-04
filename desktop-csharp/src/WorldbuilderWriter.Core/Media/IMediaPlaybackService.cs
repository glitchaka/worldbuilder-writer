namespace WorldbuilderWriter.Core.Media;

public enum MediaPlaybackState
{
    Empty,
    Ready,
    Playing,
    Paused,
    Stopped,
    Failed,
}

public interface IMediaPlaybackService : IAsyncDisposable
{
    MediaPlaybackState State { get; }

    MediaSourceDescriptor? Current { get; }

    event EventHandler<MediaPlaybackState>? StateChanged;

    Task LoadAsync(MediaSourceDescriptor source, CancellationToken cancellationToken = default);

    Task PlayAsync(CancellationToken cancellationToken = default);

    Task PauseAsync(CancellationToken cancellationToken = default);

    Task StopAsync(CancellationToken cancellationToken = default);
}
