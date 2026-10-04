using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using WorldbuilderWriter.Core.Models;

namespace WorldbuilderWriter.Desktop.Windows;

public sealed class ProjectProfileWindow : Window
{
    private readonly TextBox _archiveTitle;
    private readonly TextBox _storyTitle;
    private readonly TextBox _subtitle;
    private readonly TextBox _author;
    private readonly TextBox _genre;
    private readonly ComboBox _status;
    private readonly TextBox _synopsis;

    public ProjectProfileWindow(ProjectProfile profile)
    {
        Title = "Datos de la obra";
        Width = 680;
        Height = 720;
        MinWidth = 560;
        MinHeight = 600;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Brush("#211812");
        Foreground = Brush("#EAD9B9");

        _archiveTitle = Editor(profile.ArchiveTitle);
        _storyTitle = Editor(profile.StoryTitle);
        _subtitle = Editor(profile.Subtitle);
        _author = Editor(profile.Author);
        _genre = Editor(profile.Genre);
        _status = new ComboBox { ItemsSource = new[] { "Planificación", "Escritura", "Revisión", "Final" }, SelectedItem = profile.Status, Padding = new Thickness(8), Margin = new Thickness(0, 4, 0, 12), Background = Brush("#E7D8B9"), Foreground = Brush("#2D2118") };
        _synopsis = Editor(profile.Synopsis, multiline: true);

        var fields = new StackPanel { Margin = new Thickness(28) };
        fields.Children.Add(new TextBlock { Text = "DATOS EDITABLES DE LA OBRA", Foreground = Brush("#B76548"), FontSize = 10, Margin = new Thickness(0, 0, 0, 14) });
        fields.Children.Add(Label("Nombre del archivo")); fields.Children.Add(_archiveTitle);
        fields.Children.Add(Label("Título de la historia")); fields.Children.Add(_storyTitle);
        fields.Children.Add(Label("Subtítulo")); fields.Children.Add(_subtitle);
        fields.Children.Add(Label("Autor")); fields.Children.Add(_author);
        fields.Children.Add(Label("Género")); fields.Children.Add(_genre);
        fields.Children.Add(Label("Estado")); fields.Children.Add(_status);
        fields.Children.Add(Label("Sinopsis")); fields.Children.Add(_synopsis);

        var cancel = ActionButton("Cancelar", "#403126");
        cancel.Click += (_, _) => DialogResult = false;
        var save = ActionButton("Aplicar cambios", "#9C4C38");
        save.Click += (_, _) => DialogResult = true;
        var actions = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 14, 0, 0) };
        actions.Children.Add(cancel);
        actions.Children.Add(save);
        fields.Children.Add(actions);

        Content = new ScrollViewer { Content = fields, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    }

    public void ApplyTo(ProjectProfile profile)
    {
        profile.ArchiveTitle = string.IsNullOrWhiteSpace(_archiveTitle.Text) ? "Nueva obra" : _archiveTitle.Text.Trim();
        profile.StoryTitle = string.IsNullOrWhiteSpace(_storyTitle.Text) ? profile.ArchiveTitle : _storyTitle.Text.Trim();
        profile.Subtitle = _subtitle.Text.Trim();
        profile.Author = _author.Text.Trim();
        profile.Genre = _genre.Text.Trim();
        profile.Status = _status.SelectedItem?.ToString() ?? "Planificación";
        profile.Synopsis = _synopsis.Text;
    }

    private static TextBlock Label(string text) => new() { Text = text, Foreground = Brush("#BCA98A"), FontSize = 11, Margin = new Thickness(0, 0, 0, 3) };

    private static TextBox Editor(string text, bool multiline = false) => new()
    {
        Text = text,
        AcceptsReturn = multiline,
        TextWrapping = multiline ? TextWrapping.Wrap : TextWrapping.NoWrap,
        Height = multiline ? 130 : double.NaN,
        MinHeight = multiline ? 130 : 38,
        VerticalScrollBarVisibility = multiline ? ScrollBarVisibility.Auto : ScrollBarVisibility.Disabled,
        Padding = new Thickness(9),
        Margin = new Thickness(0, 4, 0, 12),
        Background = Brush("#E7D8B9"),
        Foreground = Brush("#2D2118"),
        BorderBrush = Brush("#76563A"),
    };

    private static Button ActionButton(string text, string background) => new()
    {
        Content = text,
        Padding = new Thickness(16, 9, 16, 9),
        Margin = new Thickness(8, 0, 0, 0),
        Background = Brush(background),
        Foreground = Brushes.White,
        BorderBrush = Brush("#8A6A49"),
        Cursor = System.Windows.Input.Cursors.Hand,
    };

    private static SolidColorBrush Brush(string value) => new((Color)ColorConverter.ConvertFromString(value));
}
