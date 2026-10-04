using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace WorldbuilderWriter.Desktop.Windows;

public sealed class DistractionFreeWindow : Window
{
    private readonly TextBox _editor;

    public DistractionFreeWindow(string sceneTitle, string content)
    {
        Title = sceneTitle;
        WindowStyle = WindowStyle.None;
        WindowState = WindowState.Maximized;
        ResizeMode = ResizeMode.NoResize;
        Background = Brush("#EAE4D6");
        PreviewKeyDown += OnPreviewKeyDown;

        _editor = new TextBox
        {
            Text = content,
            AcceptsReturn = true,
            AcceptsTab = true,
            TextWrapping = TextWrapping.Wrap,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            Background = Brush("#F2EEE5"),
            Foreground = Brush("#2C2924"),
            BorderThickness = new Thickness(0),
            FontFamily = new FontFamily("Georgia"),
            FontSize = 19,
            Padding = new Thickness(160, 70, 160, 100),
            CaretBrush = Brush("#2C2924"),
        };
        SpellCheck.SetIsEnabled(_editor, true);
        TextBlock.SetLineHeight(_editor, 31);

        var title = new TextBlock { Text = string.IsNullOrWhiteSpace(sceneTitle) ? "Escena" : sceneTitle, Foreground = Brush("#DCCDB1"), FontFamily = new FontFamily("Georgia"), FontSize = 16, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(18, 0, 0, 0) };
        var cancel = Button("Salir sin aplicar", "#42352B");
        cancel.Click += (_, _) => DialogResult = false;
        var apply = Button("Aplicar y volver", "#9D4C39");
        apply.Click += (_, _) => DialogResult = true;
        var actions = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        actions.Children.Add(cancel);
        actions.Children.Add(apply);
        var header = new Grid { Height = 48, Background = Brush("#211A15") };
        header.ColumnDefinitions.Add(new ColumnDefinition());
        header.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        header.Children.Add(title);
        Grid.SetColumn(actions, 1);
        header.Children.Add(actions);

        var root = new Grid();
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition());
        root.Children.Add(header);
        Grid.SetRow(_editor, 1);
        root.Children.Add(_editor);
        Content = root;
        Loaded += (_, _) => { _editor.Focus(); _editor.CaretIndex = _editor.Text.Length; };
    }

    public string ResultText => _editor.Text;

    private void OnPreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Escape) return;
        DialogResult = false;
        e.Handled = true;
    }

    private static Button Button(string text, string background) => new()
    {
        Content = text,
        Padding = new Thickness(16, 8, 16, 8),
        Margin = new Thickness(5, 6, 8, 6),
        Background = Brush(background),
        Foreground = Brushes.White,
        BorderBrush = Brush("#6D5540"),
        Cursor = Cursors.Hand,
    };

    private static SolidColorBrush Brush(string value) => new((Color)ColorConverter.ConvertFromString(value));
}
