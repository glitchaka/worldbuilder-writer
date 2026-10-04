using WorldbuilderWriter.Core.Media;

namespace WorldbuilderWriter.Infrastructure.Media;

public sealed class PendingNativeMediaPlaybackService : IMediaPlaybackService
{
    public MediaPlaybackState State { get; private set; } = MediaPlaybackState.Empty;

    public MediaSourceDescriptor? Current { get; private set; }

    public event EventHandler<MediaPlaybackState>? StateChanged;

    public Task LoadAsync(MediaSourceDescriptor source, CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        Current = source ?? throw new ArgumentNullException(nameof(source));
        ChangeState(MediaPlaybackState.Ready);
        return Task.CompletedTask;
    }

    public Task PlayAsync(CancellationToken cancellationToken = default) =>
        throw new NotSupportedException("La superficie multimedia nativa se conectará en la siguiente etapa de la migración.");

    public Task PauseAsync(CancellationToken cancellationToken = default) => Task.CompletedTask;

    public Task StopAsync(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ChangeState(Current is null ? MediaPlaybackState.Empty : MediaPlaybackState.Stopped);
        return Task.CompletedTask;
    }

    public ValueTask DisposeAsync()
    {
        Current = null;
        ChangeState(MediaPlaybackState.Empty);
        return ValueTask.CompletedTask;
    }

    private void ChangeState(MediaPlaybackState state)
    {
        State = state;
        StateChanged?.Invoke(this, state);
    }
}
