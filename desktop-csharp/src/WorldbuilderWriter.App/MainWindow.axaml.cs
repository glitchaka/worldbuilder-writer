using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using WorldbuilderWriter.Core.Projects;
using WorldbuilderWriter.Infrastructure.Projects;
using WorldbuilderWriter.Infrastructure.Settings;

namespace WorldbuilderWriter.App;

public sealed partial class MainWindow : Window, INotifyPropertyChanged
{
    private readonly AppSettingsStore _settingsStore = new();
    private ProjectFolderRepository? _repository;
    private string _workspacePathLabel = "Ninguna carpeta elegida";
    private string _statusMessage = string.Empty;
    private bool _hasWorkspace;
    private bool _hasNoProjects;

    public MainWindow()
    {
        InitializeComponent();
        DataContext = this;
    }

    public ObservableCollection<ProjectSummary> Projects { get; } = [];

    public string WorkspacePathLabel
    {
        get => _workspacePathLabel;
        private set => SetField(ref _workspacePathLabel, value);
    }

    public string StatusMessage
    {
        get => _statusMessage;
        private set
        {
            if (SetField(ref _statusMessage, value)) OnPropertyChanged(nameof(HasStatusMessage));
        }
    }

    public bool HasStatusMessage => !string.IsNullOrWhiteSpace(StatusMessage);

    public bool HasWorkspace
    {
        get => _hasWorkspace;
        private set
        {
            if (SetField(ref _hasWorkspace, value)) OnPropertyChanged(nameof(NeedsWorkspace));
        }
    }

    public bool NeedsWorkspace => !HasWorkspace;

    public bool HasNoProjects
    {
        get => _hasNoProjects;
        private set => SetField(ref _hasNoProjects, value);
    }

    public new event PropertyChangedEventHandler? PropertyChanged;

    private async void Window_Opened(object? sender, EventArgs eventArgs)
    {
        var settings = await _settingsStore.LoadAsync();
        if (!string.IsNullOrWhiteSpace(settings.WorkspacePath) && Directory.Exists(settings.WorkspacePath))
        {
            await ConnectWorkspaceAsync(settings.WorkspacePath);
        }
    }

    private async void ChooseWorkspace_Click(object? sender, RoutedEventArgs eventArgs)
    {
        var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Elige la carpeta externa de tus proyectos",
            AllowMultiple = false,
        });
        var folder = folders.FirstOrDefault();
        var localPath = folder?.Path.IsFile == true ? folder.Path.LocalPath : null;
        if (string.IsNullOrWhiteSpace(localPath))
        {
            StatusMessage = "No se eligió una carpeta local.";
            return;
        }

        await _settingsStore.SaveWorkspaceAsync(localPath);
        await ConnectWorkspaceAsync(localPath);
        StatusMessage = "Carpeta externa conectada.";
    }

    private async void Refresh_Click(object? sender, RoutedEventArgs eventArgs) => await RefreshProjectsAsync();

    private async void NewProject_Click(object? sender, RoutedEventArgs eventArgs)
    {
        if (_repository is null) return;
        var title = string.IsNullOrWhiteSpace(NewProjectTitle.Text) ? "Nueva obra" : NewProjectTitle.Text.Trim();
        try
        {
            var project = await _repository.CreateAsync(title);
            NewProjectTitle.Text = string.Empty;
            await RefreshProjectsAsync();
            StatusMessage = $"“{ProjectSummary.From(project).ArchiveTitle}” fue creada fuera de la aplicación.";
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            StatusMessage = error.Message;
        }
    }

    private void OpenProject_Click(object? sender, RoutedEventArgs eventArgs)
    {
        if (sender is Button { DataContext: ProjectSummary project })
        {
            StatusMessage = $"{project.ArchiveTitle} está lista. El editor nativo se conectará en el siguiente módulo de la migración.";
        }
    }

    private async Task ConnectWorkspaceAsync(string path)
    {
        _repository = new ProjectFolderRepository(path);
        WorkspacePathLabel = _repository.WorkspacePath;
        HasWorkspace = true;
        await RefreshProjectsAsync();
    }

    private async Task RefreshProjectsAsync()
    {
        if (_repository is null) return;
        try
        {
            var projects = await _repository.ListAsync();
            Projects.Clear();
            foreach (var project in projects) Projects.Add(project);
            HasNoProjects = Projects.Count == 0;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            StatusMessage = error.Message;
        }
    }

    private void Titlebar_PointerPressed(object? sender, PointerPressedEventArgs eventArgs)
    {
        if (eventArgs.GetCurrentPoint(this).Properties.IsLeftButtonPressed) BeginMoveDrag(eventArgs);
    }

    private void Minimize_Click(object? sender, RoutedEventArgs eventArgs) => WindowState = WindowState.Minimized;

    private void Maximize_Click(object? sender, RoutedEventArgs eventArgs) =>
        WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;

    private void Close_Click(object? sender, RoutedEventArgs eventArgs) => Close();

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(propertyName);
        return true;
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
