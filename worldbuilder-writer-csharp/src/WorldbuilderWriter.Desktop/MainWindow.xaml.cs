using System.Collections;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Line = System.Windows.Shapes.Line;
using Microsoft.Win32;
using WorldbuilderWriter.Core.Models;
using WorldbuilderWriter.Core.Services;
using WorldbuilderWriter.Desktop.Services;
using WorldbuilderWriter.Desktop.Windows;
using WorldbuilderWriter.Infrastructure.Manuscripts;
using WorldbuilderWriter.Infrastructure.Projects;

namespace WorldbuilderWriter.Desktop;

public partial class MainWindow : Window
{
    private readonly DesktopSettingsStore _settings = new();
    private readonly IManuscriptImportService _manuscriptImporter = new DocxManuscriptImportService();
    private FileProjectRepository? _repository;
    private StoryProject? _project;
    private CharacterRecord? _selectedCharacter;
    private Point _characterDragOffset;
    private Border? _draggedCharacterCard;
    private bool _loadingCharacter;
    private bool _loadingWriting;
    private bool _loadingWorld;
    private bool _loadingTheme;
    private double _boardZoom = 1;
    private double _sceneBoardZoom = 1;

    public MainWindow()
    {
        InitializeComponent();
        MagicList.SelectionChanged += MagicList_SelectionChanged;
        TimelineList.SelectionChanged += TimelineList_SelectionChanged;
        WorldName.TextChanged += WorldEditor_Changed;
        WorldSummary.TextChanged += WorldEditor_Changed;
        WorldDetails.TextChanged += WorldEditor_Changed;
        WorldCategory.SelectionChanged += WorldEditor_Changed;
        MagicName.TextChanged += MagicEditor_Changed;
        MagicSource.TextChanged += MagicEditor_Changed;
        MagicRules.TextChanged += MagicEditor_Changed;
        MagicCosts.TextChanged += MagicEditor_Changed;
        MagicLimits.TextChanged += MagicEditor_Changed;
        TimelineWhen.TextChanged += TimelineEditor_Changed;
        TimelineTitle.TextChanged += TimelineEditor_Changed;
        TimelineChapter.TextChanged += TimelineEditor_Changed;
        TimelineSummary.TextChanged += TimelineEditor_Changed;
    }

    private async void Window_Loaded(object sender, RoutedEventArgs e)
    {
        ApplyTheme("grim");
        var saved = await _settings.LoadWorkspaceAsync();
        if (saved is { Length: > 0 } && Directory.Exists(saved)) await ConnectWorkspaceAsync(saved);
        else SetStatus("Elige una carpeta externa. La aplicación distribuida comienza vacía.");
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2) ToggleMaximize();
        else if (e.LeftButton == MouseButtonState.Pressed) DragMove();
    }

    private void Minimize_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void Maximize_Click(object sender, RoutedEventArgs e) => ToggleMaximize();
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
    private void ToggleMaximize() => WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;

    private async void ChooseWorkspace_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Elige la carpeta externa de tus proyectos", Multiselect = false };
        if (dialog.ShowDialog(this) != true || string.IsNullOrWhiteSpace(dialog.FolderName)) return;
        await _settings.SaveWorkspaceAsync(dialog.FolderName);
        await ConnectWorkspaceAsync(dialog.FolderName);
        SetStatus("Carpeta externa conectada.");
    }

    private async Task ConnectWorkspaceAsync(string path)
    {
        _repository = new FileProjectRepository(path);
        WorkspacePathText.Text = _repository.RootPath;
        await RefreshProjectsAsync();
    }

    private async void RefreshProjects_Click(object sender, RoutedEventArgs e) => await RefreshProjectsAsync();

    private async Task RefreshProjectsAsync()
    {
        if (_repository is null)
        {
            ProjectList.ItemsSource = null;
            EmptyLibraryMessage.Visibility = Visibility.Visible;
            return;
        }

        try
        {
            var projects = await _repository.ListAsync();
            ProjectList.ItemsSource = projects;
            EmptyLibraryMessage.Visibility = projects.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            SetStatus($"{projects.Count} proyecto(s) en la carpeta externa.");
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            SetStatus(error.Message);
        }
    }

    private async void CreateProject_Click(object sender, RoutedEventArgs e)
    {
        if (_repository is null)
        {
            SetStatus("Primero elige una carpeta externa.");
            return;
        }

        var title = string.IsNullOrWhiteSpace(NewProjectTitle.Text) ? "Nueva obra" : NewProjectTitle.Text.Trim();
        var theme = SelectedTag(NewProjectTheme, "grim");
        try
        {
            var project = await _repository.CreateAsync(title, theme);
            NewProjectTitle.Clear();
            await OpenProjectAsync(project.Id);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            SetStatus(error.Message);
        }
    }

    private async void OpenProject_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: ProjectSummary summary }) await OpenProjectAsync(summary.Id);
    }

    private async Task OpenProjectAsync(string projectId)
    {
        if (_repository is null) return;
        try
        {
            _project = await _repository.OpenAsync(projectId);
            ProjectArchiveTitle.Text = _project.Profile.ArchiveTitle;
            ProjectStatusText.Text = _project.Profile.Status;
            SelectTheme(_project.Profile.Theme);
            ApplyTheme(_project.Profile.Theme);
            LibraryView.Visibility = Visibility.Collapsed;
            WorkspaceView.Visibility = Visibility.Visible;
            RefreshAllProjectViews();
            ShowSection(BoardView, "PERSONAJES / ENLACES", "Tablero");
            SetStatus($"“{_project.Profile.ArchiveTitle}” está abierta.");
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            SetStatus(error.Message);
        }
    }

    private async void DeleteProject_Click(object sender, RoutedEventArgs e)
    {
        if (_repository is null || sender is not Button { Tag: ProjectSummary summary }) return;
        var answer = MessageBox.Show(this, $"¿Eliminar “{summary.ArchiveTitle}” y su carpeta externa completa?", "Eliminar proyecto", MessageBoxButton.YesNo, MessageBoxImage.Warning);
        if (answer != MessageBoxResult.Yes) return;
        await _repository.DeleteAsync(summary.Id);
        await RefreshProjectsAsync();
        SetStatus("Proyecto eliminado.");
    }

    private async void BackToLibrary_Click(object sender, RoutedEventArgs e)
    {
        await SaveProjectAsync(showMessage: false);
        _project = null;
        WorkspaceView.Visibility = Visibility.Collapsed;
        LibraryView.Visibility = Visibility.Visible;
        await RefreshProjectsAsync();
    }

    private void ShowBoard_Click(object sender, RoutedEventArgs e) => ShowSection(BoardView, "PERSONAJES / ENLACES", "Tablero");
    private void ShowManuscript_Click(object sender, RoutedEventArgs e) => ShowSection(ManuscriptView, "CAPÍTULOS / ESCENAS", "Manuscrito");
    private void ShowWorld_Click(object sender, RoutedEventArgs e) => ShowSection(WorldView, "MUNDO / MAGIA / CRONOLOGÍA", "Creación de mundo");

    private void ShowSection(UIElement target, string eyebrow, string title)
    {
        BoardView.Visibility = target == BoardView ? Visibility.Visible : Visibility.Collapsed;
        ManuscriptView.Visibility = target == ManuscriptView ? Visibility.Visible : Visibility.Collapsed;
        WorldView.Visibility = target == WorldView ? Visibility.Visible : Visibility.Collapsed;
        SectionEyebrow.Text = eyebrow;
        SectionTitle.Text = title;
        if (target == BoardView) RefreshBoard();
        if (target == ManuscriptView) RefreshManuscript();
        if (target == WorldView) RefreshWorldViews();
    }

    private async void SaveProject_Click(object sender, RoutedEventArgs e) => await SaveProjectAsync(showMessage: true);

    private async Task SaveProjectAsync(bool showMessage)
    {
        if (_repository is null || _project is null) return;
        NormalizeWritingOrder();
        try
        {
            await _repository.SaveAsync(_project);
            if (showMessage) SetStatus($"Guardado a las {DateTime.Now:HH:mm:ss}.");
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            SetStatus(error.Message);
        }
    }

    private void EditProjectProfile_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var dialog = new ProjectProfileWindow(_project.Profile) { Owner = this };
        if (dialog.ShowDialog() != true) return;
        dialog.ApplyTo(_project.Profile);
        ProjectArchiveTitle.Text = _project.Profile.ArchiveTitle;
        ProjectStatusText.Text = _project.Profile.Status;
        SetStatus("Datos de la obra actualizados; pulsa Guardar proyecto para persistirlos.");
    }

    private void ProjectTheme_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_loadingTheme || _project is null) return;
        var theme = SelectedTag(ProjectTheme, "grim");
        _project.Profile.Theme = theme;
        ApplyTheme(theme);
    }

    private void SelectTheme(string theme)
    {
        _loadingTheme = true;
        ProjectTheme.SelectedItem = ProjectTheme.Items.Cast<ComboBoxItem>().FirstOrDefault(item => string.Equals(item.Tag?.ToString(), theme, StringComparison.OrdinalIgnoreCase)) ?? ProjectTheme.Items[0];
        _loadingTheme = false;
    }

    private void ApplyTheme(string theme)
    {
        var palette = theme switch
        {
            "classic" => new ThemePalette("#151D1B", "#21302C", "#30463F", "#EEE0B9", "#2A2115", "#D4A84C", "#9B3D31", "#7C6A45", "#F3E6C8", "#B4A685"),
            "kawaii" => new ThemePalette("#30263A", "#44324E", "#5C4064", "#FFE8F3", "#3A263A", "#E87BAE", "#C94E78", "#98749C", "#FFF1F7", "#D9BBD0"),
            "chronicle" => new ThemePalette("#23201B", "#312B23", "#40382D", "#E6DDC8", "#2E2922", "#967757", "#81463A", "#6F6251", "#ECE4D3", "#ABA08C"),
            "desk" => new ThemePalette("#1A2025", "#242D34", "#303B43", "#E7E4DC", "#20272B", "#6E9D8D", "#9A4C47", "#53646C", "#F0EEE8", "#9FADB0"),
            _ => new ThemePalette("#17130F", "#251A13", "#332319", "#E4D5B6", "#2D2118", "#B76548", "#8E352C", "#6F5138", "#EAD9B9", "#A89679"),
        };
        Resources["AppBackground"] = BrushFrom(palette.App);
        Resources["PanelBackground"] = BrushFrom(palette.Panel);
        Resources["Surface"] = BrushFrom(palette.Surface);
        Resources["Paper"] = BrushFrom(palette.Paper);
        Resources["Ink"] = BrushFrom(palette.Ink);
        Resources["Accent"] = BrushFrom(palette.Accent);
        Resources["Danger"] = BrushFrom(palette.Danger);
        Resources["Line"] = BrushFrom(palette.Line);
        Resources["TextLight"] = BrushFrom(palette.Text);
        Resources["Muted"] = BrushFrom(palette.Muted);
    }

    private static SolidColorBrush BrushFrom(string value) => new((Color)ColorConverter.ConvertFromString(value));
    private static string SelectedTag(ComboBox comboBox, string fallback) => (comboBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() ?? fallback;
    private static string SelectedContent(ComboBox comboBox, string fallback) => (comboBox.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? fallback;

    private void RefreshAllProjectViews()
    {
        RefreshBoard();
        RefreshManuscript();
        RefreshWorldViews();
    }

    private void AddCharacter_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var character = new CharacterRecord
        {
            Name = string.IsNullOrWhiteSpace(CharacterName.Text) ? "Personaje sin nombre" : CharacterName.Text.Trim(),
            Role = CharacterRole.Text.Trim(),
            Background = CharacterBackground.Text.Trim(),
            X = 80 + _project.Characters.Count % 6 * 245,
            Y = 90 + _project.Characters.Count / 6 * 145,
        };
        _project.Characters.Add(character);
        CharacterName.Clear();
        CharacterRole.Clear();
        CharacterBackground.Clear();
        _selectedCharacter = character;
        RefreshBoard();
        SelectCharacter(character);
    }

    private void AddRelationship_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || RelationshipSource.SelectedValue is not string sourceId || RelationshipTarget.SelectedValue is not string targetId || sourceId == targetId)
        {
            SetStatus("Selecciona dos personajes distintos para crear el enlace.");
            return;
        }
        _project.Relationships.Add(new RelationshipRecord
        {
            SourceId = sourceId,
            TargetId = targetId,
            Type = SelectedContent(RelationshipType, "Interacción"),
            Label = RelationshipLabel.Text.Trim(),
        });
        RelationshipLabel.Clear();
        RefreshBoard();
    }

    private void RefreshBoard()
    {
        BoardCanvas.Children.Clear();
        if (_project is null) return;
        foreach (var relationship in _project.Relationships)
        {
            var source = _project.Characters.FirstOrDefault(character => character.Id == relationship.SourceId);
            var target = _project.Characters.FirstOrDefault(character => character.Id == relationship.TargetId);
            if (source is null || target is null) continue;
            var line = new Line
            {
                X1 = source.X + 95,
                Y1 = source.Y + 45,
                X2 = target.X + 95,
                Y2 = target.Y + 45,
                Stroke = RelationshipBrush(relationship.Type),
                StrokeThickness = 2 + relationship.Strength * 0.5,
                Opacity = 0.95,
            };
            if (relationship.Type is "Conocido" or "Interacción") line.StrokeDashArray = new DoubleCollection([8, 6]);
            BoardCanvas.Children.Add(line);
        }

        foreach (var character in _project.Characters)
        {
            var card = CreateCharacterCard(character);
            Canvas.SetLeft(card, character.X);
            Canvas.SetTop(card, character.Y);
            BoardCanvas.Children.Add(card);
        }

        var characters = _project.Characters.ToArray();
        RelationshipSource.ItemsSource = characters;
        RelationshipTarget.ItemsSource = characters;
        SelectedCharacterPanel.Visibility = _selectedCharacter is null ? Visibility.Collapsed : Visibility.Visible;
    }

    private Border CreateCharacterCard(CharacterRecord character)
    {
        var name = new TextBlock { Text = character.Name, Foreground = BrushFrom("#2D2118"), FontFamily = new FontFamily("Georgia"), FontSize = 17, FontWeight = FontWeights.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis };
        var role = new TextBlock { Text = string.IsNullOrWhiteSpace(character.Role) ? "Sin rol" : character.Role, Foreground = BrushFrom("#735E49"), FontSize = 10, Margin = new Thickness(0, 6, 0, 0), TextTrimming = TextTrimming.CharacterEllipsis };
        var stack = new StackPanel { Margin = new Thickness(14, 11, 10, 9) };
        stack.Children.Add(name);
        stack.Children.Add(role);
        var content = new Grid();
        content.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        content.ColumnDefinitions.Add(new ColumnDefinition());
        if (TryLoadCharacterPhoto(character) is { } photo)
        {
            content.Children.Add(new Border { Width = 54, Height = 64, Background = BrushFrom("#CBB993"), Margin = new Thickness(9, 9, 0, 9), Child = new Image { Source = photo, Stretch = Stretch.UniformToFill } });
            Grid.SetColumn(stack, 1);
        }
        content.Children.Add(stack);
        var card = new Border
        {
            Width = 190,
            Height = 90,
            Background = BrushFrom("#E9D8B6"),
            BorderBrush = _selectedCharacter?.Id == character.Id ? BrushFrom("#C0503E") : BrushFrom("#6D5038"),
            BorderThickness = _selectedCharacter?.Id == character.Id ? new Thickness(3) : new Thickness(1.5),
            CornerRadius = new CornerRadius(3),
            Child = content,
            Tag = character,
            Cursor = Cursors.Hand,
        };
        card.MouseLeftButtonDown += CharacterCard_MouseLeftButtonDown;
        card.MouseMove += CharacterCard_MouseMove;
        card.MouseLeftButtonUp += CharacterCard_MouseLeftButtonUp;
        return card;
    }

    private void CharacterCard_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is not Border { Tag: CharacterRecord character } card) return;
        SelectCharacter(character);
        _draggedCharacterCard = card;
        _characterDragOffset = e.GetPosition(card);
        card.CaptureMouse();
        e.Handled = true;
    }

    private void CharacterCard_MouseMove(object sender, MouseEventArgs e)
    {
        if (_draggedCharacterCard is null || _selectedCharacter is null || e.LeftButton != MouseButtonState.Pressed) return;
        var point = e.GetPosition(BoardCanvas);
        _selectedCharacter.X = Math.Clamp(point.X - _characterDragOffset.X, 0, BoardCanvas.Width - _draggedCharacterCard.Width);
        _selectedCharacter.Y = Math.Clamp(point.Y - _characterDragOffset.Y, 0, BoardCanvas.Height - _draggedCharacterCard.Height);
        Canvas.SetLeft(_draggedCharacterCard, _selectedCharacter.X);
        Canvas.SetTop(_draggedCharacterCard, _selectedCharacter.Y);
    }

    private void CharacterCard_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        _draggedCharacterCard?.ReleaseMouseCapture();
        _draggedCharacterCard = null;
        RefreshBoard();
    }

    private void SelectCharacter(CharacterRecord character)
    {
        _selectedCharacter = character;
        _loadingCharacter = true;
        SelectedCharacterName.Text = character.Name;
        SelectedCharacterRole.Text = character.Role;
        SelectedCharacterNotes.Text = character.Background;
        SelectedCharacterPhoto.Text = string.IsNullOrWhiteSpace(character.PhotoPath) ? "Sin fotografía" : character.PhotoPath;
        _loadingCharacter = false;
        SelectedCharacterPanel.Visibility = Visibility.Visible;
    }

    private void SelectedCharacterField_Changed(object sender, TextChangedEventArgs e)
    {
        if (_loadingCharacter || _selectedCharacter is null) return;
        _selectedCharacter.Name = string.IsNullOrWhiteSpace(SelectedCharacterName.Text) ? "Personaje sin nombre" : SelectedCharacterName.Text;
        _selectedCharacter.Role = SelectedCharacterRole.Text;
        _selectedCharacter.Background = SelectedCharacterNotes.Text;
        RefreshBoard();
    }

    private void DeleteCharacter_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || _selectedCharacter is null) return;
        if (MessageBox.Show(this, $"¿Eliminar a “{_selectedCharacter.Name}” y todas sus relaciones?", "Eliminar personaje", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        var id = _selectedCharacter.Id;
        _project.Characters.RemoveAll(character => character.Id == id);
        _project.Relationships.RemoveAll(relationship => relationship.SourceId == id || relationship.TargetId == id);
        _selectedCharacter = null;
        RefreshBoard();
    }

    private void ChooseCharacterPhoto_Click(object sender, RoutedEventArgs e)
    {
        if (_repository is null || _project is null || _selectedCharacter is null) return;
        var dialog = new OpenFileDialog { Title = "Elegir fotografía del personaje", Filter = "Imágenes|*.png;*.jpg;*.jpeg;*.webp;*.bmp", Multiselect = false };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            var projectDirectory = Path.Combine(_repository.RootPath, _project.Id);
            var mediaDirectory = Path.Combine(projectDirectory, "media", "characters");
            Directory.CreateDirectory(mediaDirectory);
            var extension = Path.GetExtension(dialog.FileName).ToLowerInvariant();
            var destination = Path.Combine(mediaDirectory, $"{_selectedCharacter.Id}{extension}");
            File.Copy(dialog.FileName, destination, overwrite: true);
            _selectedCharacter.PhotoPath = Path.GetRelativePath(projectDirectory, destination);
            SelectedCharacterPhoto.Text = _selectedCharacter.PhotoPath;
            RefreshBoard();
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or NotSupportedException)
        {
            SetStatus(error.Message);
        }
    }

    private BitmapImage? TryLoadCharacterPhoto(CharacterRecord character)
    {
        if (_repository is null || _project is null || string.IsNullOrWhiteSpace(character.PhotoPath)) return null;
        try
        {
            var path = Path.GetFullPath(Path.Combine(_repository.RootPath, _project.Id, character.PhotoPath));
            if (!File.Exists(path)) return null;
            var image = new BitmapImage();
            image.BeginInit();
            image.CacheOption = BitmapCacheOption.OnLoad;
            image.UriSource = new Uri(path, UriKind.Absolute);
            image.EndInit();
            image.Freeze();
            return image;
        }
        catch (Exception error) when (error is IOException or NotSupportedException)
        {
            return null;
        }
    }

    private static Brush RelationshipBrush(string type) => BrushFrom(type switch
    {
        "Familia" => "#D1A94F",
        "Amistad" => "#B74C3D",
        "Romance" => "#D66C82",
        "Alianza" => "#5D9A7F",
        "Enemistad" => "#8E2634",
        "Rivalidad" => "#A85B25",
        "Conocido" => "#8B7BAA",
        _ => "#A99A87",
    });

    private void BoardZoomOut_Click(object sender, RoutedEventArgs e) => SetBoardZoom(_boardZoom - 0.1);
    private void BoardZoomIn_Click(object sender, RoutedEventArgs e) => SetBoardZoom(_boardZoom + 0.1);
    private void BoardZoomReset_Click(object sender, RoutedEventArgs e) => SetBoardZoom(1);
    private void SetBoardZoom(double value)
    {
        _boardZoom = Math.Clamp(value, 0.35, 2.25);
        BoardCanvas.LayoutTransform = new ScaleTransform(_boardZoom, _boardZoom);
        BoardZoomLabel.Text = $"{_boardZoom:P0}";
    }

    private WritingChapter? SelectedChapter => ChapterList.SelectedItem as WritingChapter;
    private WritingScene? SelectedScene => SceneList.SelectedItem as WritingScene;

    private void RefreshManuscript(string? chapterId = null, string? sceneId = null)
    {
        if (_project is null) return;
        var currentChapterId = chapterId ?? SelectedChapter?.Id;
        var currentSceneId = sceneId ?? SelectedScene?.Id;
        var chapters = _project.Chapters.OrderBy(chapter => chapter.Order).ToArray();
        ChapterList.ItemsSource = chapters;
        ChapterList.SelectedItem = chapters.FirstOrDefault(chapter => chapter.Id == currentChapterId) ?? chapters.FirstOrDefault();
        RefreshSceneList(currentSceneId);
        RefreshSceneBoard();
    }

    private void RefreshSceneList(string? sceneId = null)
    {
        var scenes = SelectedChapter?.Scenes.OrderBy(scene => scene.Order).ToArray() ?? [];
        SceneList.ItemsSource = scenes;
        SceneList.SelectedItem = scenes.FirstOrDefault(scene => scene.Id == sceneId) ?? scenes.FirstOrDefault();
        LoadWritingEditor();
    }

    private void ChapterList_SelectionChanged(object sender, SelectionChangedEventArgs e) => RefreshSceneList();
    private void SceneList_SelectionChanged(object sender, SelectionChangedEventArgs e) => LoadWritingEditor();

    private void LoadWritingEditor()
    {
        _loadingWriting = true;
        var chapter = SelectedChapter;
        var scene = SelectedScene;
        ChapterLabelEditor.Text = chapter?.Label ?? string.Empty;
        ChapterTitleEditor.Text = chapter?.Title ?? string.Empty;
        SceneTitleEditor.Text = scene?.Title ?? string.Empty;
        ScenePovEditor.Text = scene?.Pov ?? string.Empty;
        SceneLocationEditor.Text = scene?.Location ?? string.Empty;
        SceneLayerEditor.Text = scene?.NarrativeLayer ?? string.Empty;
        SceneStatusEditor.SelectedItem = SceneStatusEditor.Items.Cast<ComboBoxItem>().FirstOrDefault(item => string.Equals(item.Content?.ToString(), scene?.Status, StringComparison.OrdinalIgnoreCase)) ?? SceneStatusEditor.Items[0];
        WritingEditor.Text = scene?.Content ?? string.Empty;
        WritingEditor.IsEnabled = scene is not null;
        WordCountText.Text = $"{WordCount(scene?.Content)} palabras";
        _loadingWriting = false;
    }

    private void WritingField_Changed(object sender, RoutedEventArgs e)
    {
        if (_loadingWriting) return;
        var chapter = SelectedChapter;
        var scene = SelectedScene;
        if (chapter is not null)
        {
            chapter.Label = ChapterLabelEditor.Text;
            chapter.Title = ChapterTitleEditor.Text;
        }
        if (scene is not null)
        {
            scene.Title = SceneTitleEditor.Text;
            scene.Pov = ScenePovEditor.Text;
            scene.Location = SceneLocationEditor.Text;
            scene.NarrativeLayer = SceneLayerEditor.Text;
            scene.Status = SelectedContent(SceneStatusEditor, "Borrador");
            scene.Content = WritingEditor.Text;
            WordCountText.Text = $"{WordCount(scene.Content)} palabras";
        }
    }

    private void AddChapter_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var chapter = new WritingChapter { Label = $"Capítulo {_project.Chapters.Count + 1}", Order = _project.Chapters.Count };
        var scene = new WritingScene { ChapterId = chapter.Id };
        chapter.Scenes.Add(scene);
        _project.Chapters.Add(chapter);
        RefreshManuscript(chapter.Id, scene.Id);
    }

    private void AddScene_Click(object sender, RoutedEventArgs e)
    {
        var chapter = SelectedChapter;
        if (chapter is null) { AddChapter_Click(sender, e); return; }
        var scene = new WritingScene { ChapterId = chapter.Id, Order = chapter.Scenes.Count, Title = $"Escena {chapter.Scenes.Count + 1}" };
        chapter.Scenes.Add(scene);
        RefreshManuscript(chapter.Id, scene.Id);
    }

    private void DeleteChapter_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || SelectedChapter is not { } chapter) return;
        if (MessageBox.Show(this, $"¿Eliminar {chapter.Label}: “{chapter.Title}” y sus {chapter.Scenes.Count} escenas?", "Eliminar capítulo", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        _project.Chapters.Remove(chapter);
        NormalizeWritingOrder();
        RefreshManuscript();
    }

    private void DeleteScene_Click(object sender, RoutedEventArgs e)
    {
        if (SelectedChapter is not { } chapter || SelectedScene is not { } scene) return;
        if (MessageBox.Show(this, $"¿Eliminar la escena “{scene.Title}”?", "Eliminar escena", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        chapter.Scenes.Remove(scene);
        NormalizeWritingOrder();
        RefreshManuscript(chapter.Id);
    }

    private void PurgeChapters_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || _project.Chapters.Count == 0) return;
        var scenes = _project.Chapters.Sum(chapter => chapter.Scenes.Count);
        if (MessageBox.Show(this, $"¿Purgar {_project.Chapters.Count} capítulos y {scenes} escenas?", "Purgar manuscrito", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        _project.Chapters.Clear();
        RefreshManuscript();
    }

    private async void ImportManuscript_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var dialog = new OpenFileDialog { Title = "Importar manuscrito como texto", Filter = "Manuscritos|*.docx;*.txt;*.md|Word|*.docx|Texto|*.txt;*.md", Multiselect = false };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            await using var stream = File.OpenRead(dialog.FileName);
            var imported = await _manuscriptImporter.ImportAsync(stream, System.IO.Path.GetFileName(dialog.FileName), _project.Chapters.Count);
            _project.Chapters.AddRange(imported.Chapters);
            NormalizeWritingOrder();
            RefreshManuscript(imported.Chapters.FirstOrDefault()?.Id);
            await SaveProjectAsync(showMessage: false);
            SetStatus($"Se importaron {imported.Chapters.Count} capítulos y {imported.Chapters.Sum(chapter => chapter.Scenes.Count)} escenas como contenido editable.");
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            SetStatus(error.Message);
        }
    }

    private void DistractionFree_Click(object sender, RoutedEventArgs e)
    {
        if (SelectedScene is not { } scene) return;
        var window = new DistractionFreeWindow(scene.Title, scene.Content) { Owner = this };
        if (window.ShowDialog() != true) return;
        scene.Content = window.ResultText;
        WritingEditor.Text = scene.Content;
    }

    private void RefreshSceneBoard()
    {
        SceneBoardHost.Children.Clear();
        if (_project is null) return;
        foreach (var chapter in _project.Chapters.OrderBy(chapter => chapter.Order)) SceneBoardHost.Children.Add(CreateSceneColumn(chapter));
        SetSceneBoardZoom(_sceneBoardZoom);
    }

    private Border CreateSceneColumn(WritingChapter chapter)
    {
        var stack = new StackPanel();
        var header = new Border { Background = BrushFrom("#2C211A"), Padding = new Thickness(13), Tag = new ChapterDrag(chapter.Id), Cursor = Cursors.SizeAll };
        var headerText = new StackPanel();
        headerText.Children.Add(new TextBlock { Text = chapter.Label, Foreground = BrushFrom("#B5A184"), FontSize = 9 });
        headerText.Children.Add(new TextBlock { Text = chapter.Title, FontFamily = new FontFamily("Georgia"), FontSize = 17, TextWrapping = TextWrapping.Wrap });
        headerText.Children.Add(new TextBlock { Text = $"{chapter.Scenes.Count} escenas · {chapter.Scenes.Sum(scene => WordCount(scene.Content))} palabras", Foreground = BrushFrom("#B5A184"), FontSize = 9, Margin = new Thickness(0, 5, 0, 0) });
        header.Child = headerText;
        header.PreviewMouseMove += DragSource_PreviewMouseMove;
        stack.Children.Add(header);
        foreach (var scene in chapter.Scenes.OrderBy(scene => scene.Order)) stack.Children.Add(CreateSceneCard(chapter, scene));
        var column = new Border { Width = 285, MinHeight = 360, Background = BrushFrom("#D8C49C"), BorderBrush = BrushFrom("#826343"), BorderThickness = new Thickness(1), Margin = new Thickness(0, 0, 14, 0), Child = stack, Tag = chapter, AllowDrop = true };
        column.Drop += SceneColumn_Drop;
        return column;
    }

    private Border CreateSceneCard(WritingChapter chapter, WritingScene scene)
    {
        var stack = new StackPanel();
        stack.Children.Add(new TextBlock { Text = string.IsNullOrWhiteSpace(scene.NarrativeLayer) ? "SIN CAPA" : scene.NarrativeLayer.ToUpperInvariant(), Foreground = BrushFrom("#7A5B3F"), FontSize = 8 });
        stack.Children.Add(new TextBlock { Text = scene.Title, Foreground = BrushFrom("#2A1F17"), FontFamily = new FontFamily("Georgia"), FontSize = 16, Margin = new Thickness(0, 4, 0, 0), TextWrapping = TextWrapping.Wrap });
        stack.Children.Add(new TextBlock { Text = $"{(string.IsNullOrWhiteSpace(scene.Pov) ? "POV sin definir" : $"POV · {scene.Pov}")} · {WordCount(scene.Content)} palabras · {scene.Status}", Foreground = BrushFrom("#765D48"), FontSize = 9, Margin = new Thickness(0, 7, 0, 0), TextWrapping = TextWrapping.Wrap });
        var card = new Border { Background = BrushFrom("#EFE3C8"), BorderBrush = SceneStatusBrush(scene.Status), BorderThickness = new Thickness(0, 0, 0, 4), Padding = new Thickness(12), Margin = new Thickness(10, 10, 10, 0), Child = stack, Tag = new SceneDrag(chapter.Id, scene.Id), Cursor = Cursors.SizeAll, AllowDrop = true };
        card.PreviewMouseMove += DragSource_PreviewMouseMove;
        card.MouseLeftButtonDown += SceneCard_MouseLeftButtonDown;
        card.Drop += SceneCard_Drop;
        return card;
    }

    private void DragSource_PreviewMouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed || sender is not FrameworkElement { Tag: { } dragData } element) return;
        if (dragData is SceneDrag or ChapterDrag) DragDrop.DoDragDrop(element, dragData, DragDropEffects.Move);
    }

    private void SceneColumn_Drop(object sender, DragEventArgs e)
    {
        if (_project is null || sender is not Border { Tag: WritingChapter target }) return;
        if (e.Data.GetData(typeof(SceneDrag)) is SceneDrag sceneDrag) MoveScene(sceneDrag, target.Id, target.Scenes.Count);
        else if (e.Data.GetData(typeof(ChapterDrag)) is ChapterDrag chapterDrag) MoveChapter(chapterDrag.ChapterId, target.Id);
        e.Handled = true;
    }

    private void SceneCard_Drop(object sender, DragEventArgs e)
    {
        if (sender is not Border { Tag: SceneDrag target } || e.Data.GetData(typeof(SceneDrag)) is not SceneDrag moving) return;
        var targetChapter = _project?.Chapters.FirstOrDefault(chapter => chapter.Id == target.ChapterId);
        var targetIndex = targetChapter?.Scenes.FindIndex(scene => scene.Id == target.SceneId) ?? -1;
        if (targetIndex >= 0) MoveScene(moving, target.ChapterId, targetIndex);
        e.Handled = true;
    }

    private void MoveScene(SceneDrag moving, string targetChapterId, int targetIndex)
    {
        if (_project is null) return;
        var source = _project.Chapters.FirstOrDefault(chapter => chapter.Id == moving.ChapterId);
        var target = _project.Chapters.FirstOrDefault(chapter => chapter.Id == targetChapterId);
        var scene = source?.Scenes.FirstOrDefault(item => item.Id == moving.SceneId);
        if (source is null || target is null || scene is null) return;
        source.Scenes.Remove(scene);
        scene.ChapterId = target.Id;
        target.Scenes.Insert(Math.Clamp(targetIndex, 0, target.Scenes.Count), scene);
        NormalizeWritingOrder();
        RefreshManuscript(target.Id, scene.Id);
    }

    private void MoveChapter(string movingId, string targetId)
    {
        if (_project is null || movingId == targetId) return;
        var chapters = _project.Chapters.OrderBy(chapter => chapter.Order).ToList();
        var moving = chapters.FirstOrDefault(chapter => chapter.Id == movingId);
        var targetIndex = chapters.FindIndex(chapter => chapter.Id == targetId);
        if (moving is null || targetIndex < 0) return;
        chapters.Remove(moving);
        chapters.Insert(Math.Clamp(targetIndex, 0, chapters.Count), moving);
        _project.Chapters = chapters;
        NormalizeWritingOrder();
        RefreshManuscript(moving.Id);
    }

    private void SceneCard_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount != 2) return;
        if (sender is not Border { Tag: SceneDrag scene }) return;
        RefreshManuscript(scene.ChapterId, scene.SceneId);
        ManuscriptTabs.SelectedIndex = 0;
    }

    private void SceneTrash_Drop(object sender, DragEventArgs e)
    {
        if (_project is null) return;
        if (e.Data.GetData(typeof(SceneDrag)) is SceneDrag sceneDrag)
        {
            var chapter = _project.Chapters.FirstOrDefault(item => item.Id == sceneDrag.ChapterId);
            var scene = chapter?.Scenes.FirstOrDefault(item => item.Id == sceneDrag.SceneId);
            if (chapter is not null && scene is not null && MessageBox.Show(this, $"¿Eliminar la escena “{scene.Title}”?", "Papelera", MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes) chapter.Scenes.Remove(scene);
        }
        else if (e.Data.GetData(typeof(ChapterDrag)) is ChapterDrag chapterDrag)
        {
            var chapter = _project.Chapters.FirstOrDefault(item => item.Id == chapterDrag.ChapterId);
            if (chapter is not null && MessageBox.Show(this, $"¿Eliminar {chapter.Label} y sus {chapter.Scenes.Count} escenas?", "Papelera", MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes) _project.Chapters.Remove(chapter);
        }
        NormalizeWritingOrder();
        RefreshManuscript();
    }

    private static Brush SceneStatusBrush(string status) => BrushFrom(status switch { "Final" => "#4C8A63", "Revisión" => "#C09335", _ => "#77736C" });
    private void SceneBoardZoomOut_Click(object sender, RoutedEventArgs e) => SetSceneBoardZoom(_sceneBoardZoom - 0.1);
    private void SceneBoardZoomIn_Click(object sender, RoutedEventArgs e) => SetSceneBoardZoom(_sceneBoardZoom + 0.1);
    private void SetSceneBoardZoom(double value)
    {
        _sceneBoardZoom = Math.Clamp(value, 0.4, 1.8);
        SceneBoardHost.LayoutTransform = new ScaleTransform(_sceneBoardZoom, _sceneBoardZoom);
        SceneBoardZoomLabel.Text = $"{_sceneBoardZoom:P0}";
    }

    private void RefreshWorldViews()
    {
        if (_project is null) return;
        WorldList.ItemsSource = null;
        WorldList.ItemsSource = _project.World.ToArray();
        MagicList.ItemsSource = null;
        MagicList.ItemsSource = _project.MagicSystems.ToArray();
        TimelineList.ItemsSource = null;
        TimelineList.ItemsSource = _project.Timeline.OrderBy(item => item.Order).ToArray();
    }

    private void AddWorldEntry_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var entry = new WorldEntry { Category = SelectedContent(WorldCategory, "Otro"), Name = string.IsNullOrWhiteSpace(WorldName.Text) ? "Entrada sin nombre" : WorldName.Text.Trim(), Summary = WorldSummary.Text, Details = WorldDetails.Text };
        _project.World.Add(entry);
        RefreshWorldViews();
        WorldList.SelectedItem = WorldList.Items.Cast<WorldEntry>().FirstOrDefault(item => item.Id == entry.Id);
    }

    private void WorldList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (WorldList.SelectedItem is not WorldEntry entry) return;
        _loadingWorld = true;
        WorldCategory.SelectedItem = WorldCategory.Items.Cast<ComboBoxItem>().FirstOrDefault(item => string.Equals(item.Content?.ToString(), entry.Category, StringComparison.OrdinalIgnoreCase)) ?? WorldCategory.Items[0];
        WorldName.Text = entry.Name;
        WorldSummary.Text = entry.Summary;
        WorldDetails.Text = entry.Details;
        _loadingWorld = false;
    }

    private void WorldEditor_Changed(object? sender, RoutedEventArgs e)
    {
        if (_loadingWorld || WorldList.SelectedItem is not WorldEntry entry) return;
        entry.Category = SelectedContent(WorldCategory, "Otro");
        entry.Name = WorldName.Text;
        entry.Summary = WorldSummary.Text;
        entry.Details = WorldDetails.Text;
    }

    private void DeleteWorldEntry_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || WorldList.SelectedItem is not WorldEntry entry) return;
        _project.World.Remove(entry);
        ClearWorldEditors();
        RefreshWorldViews();
    }

    private void AddMagicSystem_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var system = new MagicSystem { Name = string.IsNullOrWhiteSpace(MagicName.Text) ? "Sistema sin nombre" : MagicName.Text.Trim(), Source = MagicSource.Text, Rules = MagicRules.Text, Costs = MagicCosts.Text, Limits = MagicLimits.Text };
        _project.MagicSystems.Add(system);
        RefreshWorldViews();
        MagicList.SelectedItem = MagicList.Items.Cast<MagicSystem>().FirstOrDefault(item => item.Id == system.Id);
    }

    private void MagicList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (MagicList.SelectedItem is not MagicSystem system) return;
        _loadingWorld = true;
        MagicName.Text = system.Name;
        MagicSource.Text = system.Source;
        MagicRules.Text = system.Rules;
        MagicCosts.Text = system.Costs;
        MagicLimits.Text = system.Limits;
        _loadingWorld = false;
    }

    private void MagicEditor_Changed(object? sender, RoutedEventArgs e)
    {
        if (_loadingWorld || MagicList.SelectedItem is not MagicSystem system) return;
        system.Name = MagicName.Text;
        system.Source = MagicSource.Text;
        system.Rules = MagicRules.Text;
        system.Costs = MagicCosts.Text;
        system.Limits = MagicLimits.Text;
    }

    private void DeleteMagicSystem_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || MagicList.SelectedItem is not MagicSystem system) return;
        _project.MagicSystems.Remove(system);
        ClearMagicEditors();
        RefreshWorldViews();
    }

    private void AddTimelineEvent_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null) return;
        var item = new TimelineEvent { When = TimelineWhen.Text, Title = string.IsNullOrWhiteSpace(TimelineTitle.Text) ? "Evento sin título" : TimelineTitle.Text.Trim(), Chapter = TimelineChapter.Text, Summary = TimelineSummary.Text, Order = _project.Timeline.Count };
        _project.Timeline.Add(item);
        RefreshWorldViews();
        TimelineList.SelectedItem = TimelineList.Items.Cast<TimelineEvent>().FirstOrDefault(entry => entry.Id == item.Id);
    }

    private void TimelineList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (TimelineList.SelectedItem is not TimelineEvent item) return;
        _loadingWorld = true;
        TimelineWhen.Text = item.When;
        TimelineTitle.Text = item.Title;
        TimelineChapter.Text = item.Chapter;
        TimelineSummary.Text = item.Summary;
        _loadingWorld = false;
    }

    private void TimelineEditor_Changed(object? sender, TextChangedEventArgs e)
    {
        if (_loadingWorld || TimelineList.SelectedItem is not TimelineEvent item) return;
        item.When = TimelineWhen.Text;
        item.Title = TimelineTitle.Text;
        item.Chapter = TimelineChapter.Text;
        item.Summary = TimelineSummary.Text;
    }

    private void DeleteTimelineEvent_Click(object sender, RoutedEventArgs e)
    {
        if (_project is null || TimelineList.SelectedItem is not TimelineEvent item) return;
        _project.Timeline.Remove(item);
        for (var index = 0; index < _project.Timeline.Count; index++) _project.Timeline[index].Order = index;
        ClearTimelineEditors();
        RefreshWorldViews();
    }

    private void ClearWorldEditors() { _loadingWorld = true; WorldName.Clear(); WorldSummary.Clear(); WorldDetails.Clear(); WorldCategory.SelectedIndex = 0; _loadingWorld = false; }
    private void ClearMagicEditors() { _loadingWorld = true; MagicName.Clear(); MagicSource.Clear(); MagicRules.Clear(); MagicCosts.Clear(); MagicLimits.Clear(); _loadingWorld = false; }
    private void ClearTimelineEditors() { _loadingWorld = true; TimelineWhen.Clear(); TimelineTitle.Clear(); TimelineChapter.Clear(); TimelineSummary.Clear(); _loadingWorld = false; }

    private void NormalizeWritingOrder()
    {
        if (_project is null) return;
        var chapters = _project.Chapters.OrderBy(chapter => chapter.Order).ToList();
        for (var chapterIndex = 0; chapterIndex < chapters.Count; chapterIndex++)
        {
            var chapter = chapters[chapterIndex];
            chapter.Order = chapterIndex;
            for (var sceneIndex = 0; sceneIndex < chapter.Scenes.Count; sceneIndex++)
            {
                chapter.Scenes[sceneIndex].Order = sceneIndex;
                chapter.Scenes[sceneIndex].ChapterId = chapter.Id;
            }
        }
        _project.Chapters = chapters;
    }

    private static int WordCount(string? text) => string.IsNullOrWhiteSpace(text) ? 0 : Regex.Matches(text, @"[\p{L}\p{N}]+(?:['’’-][\p{L}\p{N}]+)*").Count;
    private void SetStatus(string message) => StatusText.Text = message;

    private sealed record ThemePalette(string App, string Panel, string Surface, string Paper, string Ink, string Accent, string Danger, string Line, string Text, string Muted);
    [Serializable] private sealed record SceneDrag(string ChapterId, string SceneId);
    [Serializable] private sealed record ChapterDrag(string ChapterId);
}
