namespace WorldbuilderWriter.Core.Models;

public sealed class StoryProject
{
    public int FormatVersion { get; set; } = 1;
    public string Id { get; set; } = NewId("project");
    public DateTimeOffset CreatedAt { get; set; } = DateTimeOffset.UtcNow;
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.UtcNow;
    public ProjectProfile Profile { get; set; } = new();
    public List<CharacterRecord> Characters { get; set; } = [];
    public List<RelationshipRecord> Relationships { get; set; } = [];
    public List<WritingChapter> Chapters { get; set; } = [];
    public List<WorldEntry> World { get; set; } = [];
    public List<MagicSystem> MagicSystems { get; set; } = [];
    public List<TimelineEvent> Timeline { get; set; } = [];

    public static StoryProject Create(string title, string theme = "grim") => new()
    {
        Profile = new ProjectProfile
        {
            ArchiveTitle = string.IsNullOrWhiteSpace(title) ? "Nueva obra" : title.Trim(),
            StoryTitle = string.IsNullOrWhiteSpace(title) ? "Nueva obra" : title.Trim(),
            Theme = ThemeRules.Normalize(theme),
        },
    };

    public static string NewId(string prefix) => $"{prefix}-{Guid.NewGuid():N}";
}

public sealed class ProjectProfile
{
    public string ArchiveTitle { get; set; } = "Nueva obra";
    public string StoryTitle { get; set; } = "Nueva obra";
    public string Subtitle { get; set; } = "Biblia narrativa editable";
    public string Author { get; set; } = "";
    public string Genre { get; set; } = "";
    public string Status { get; set; } = "Planificación";
    public string Synopsis { get; set; } = "";
    public string Theme { get; set; } = "grim";
}

public sealed class CharacterRecord
{
    public string Id { get; set; } = StoryProject.NewId("personaje");
    public string Name { get; set; } = "Personaje sin nombre";
    public string Role { get; set; } = "";
    public string Occupation { get; set; } = "";
    public string Summary { get; set; } = "";
    public string Background { get; set; } = "";
    public string Notes { get; set; } = "";
    public string PhotoPath { get; set; } = "";
    public double X { get; set; } = 120;
    public double Y { get; set; } = 120;
}

public sealed class RelationshipRecord
{
    public string Id { get; set; } = StoryProject.NewId("enlace");
    public string SourceId { get; set; } = "";
    public string TargetId { get; set; } = "";
    public string Type { get; set; } = "Interacción";
    public string Label { get; set; } = "";
    public string Details { get; set; } = "";
    public int Strength { get; set; } = 1;
}

public sealed class WritingChapter
{
    public string Id { get; set; } = StoryProject.NewId("capitulo");
    public string Label { get; set; } = "Capítulo 1";
    public string Title { get; set; } = "Sin título";
    public int Order { get; set; }
    public List<WritingScene> Scenes { get; set; } = [];
}

public sealed class WritingScene
{
    public string Id { get; set; } = StoryProject.NewId("escena");
    public string ChapterId { get; set; } = "";
    public string Title { get; set; } = "Escena 1";
    public int Order { get; set; }
    public string Content { get; set; } = "";
    public string Pov { get; set; } = "";
    public string Location { get; set; } = "";
    public string NarrativeLayer { get; set; } = "";
    public string Status { get; set; } = "Borrador";
}

public sealed class WorldEntry
{
    public string Id { get; set; } = StoryProject.NewId("mundo");
    public string Category { get; set; } = "País o reino";
    public string Name { get; set; } = "Entrada sin nombre";
    public string Summary { get; set; } = "";
    public string Details { get; set; } = "";
}

public sealed class MagicSystem
{
    public string Id { get; set; } = StoryProject.NewId("magia");
    public string Name { get; set; } = "Sistema sin nombre";
    public string Source { get; set; } = "";
    public string Rules { get; set; } = "";
    public string Costs { get; set; } = "";
    public string Limits { get; set; } = "";
}

public sealed class TimelineEvent
{
    public string Id { get; set; } = StoryProject.NewId("evento");
    public string When { get; set; } = "Fecha sin definir";
    public string Title { get; set; } = "Evento sin título";
    public string Summary { get; set; } = "";
    public string Chapter { get; set; } = "";
    public int Order { get; set; }
}

public sealed record ProjectSummary(
    string Id,
    string ArchiveTitle,
    string StoryTitle,
    string Status,
    string Theme,
    int Characters,
    int Chapters,
    int Scenes,
    DateTimeOffset UpdatedAt)
{
    public static ProjectSummary From(StoryProject project) => new(
        project.Id,
        project.Profile.ArchiveTitle,
        project.Profile.StoryTitle,
        project.Profile.Status,
        project.Profile.Theme,
        project.Characters.Count,
        project.Chapters.Count,
        project.Chapters.Sum(chapter => chapter.Scenes.Count),
        project.UpdatedAt);
}

public static class ThemeRules
{
    public static readonly string[] Available = ["grim", "classic", "kawaii", "chronicle", "desk"];

    public static string Normalize(string? theme) => Available.Contains(theme, StringComparer.OrdinalIgnoreCase)
        ? theme!.ToLowerInvariant()
        : "grim";
}
