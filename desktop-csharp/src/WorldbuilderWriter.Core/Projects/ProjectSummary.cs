using System.Text.Json.Nodes;

namespace WorldbuilderWriter.Core.Projects;

public sealed record ProjectSummary(
    string Id,
    string ArchiveTitle,
    string StoryTitle,
    string Genre,
    string Status,
    string Theme,
    int Characters,
    int Relationships,
    int WorldEntries,
    int MagicSystems,
    DateTimeOffset UpdatedAt)
{
    public static ProjectSummary From(ProjectEnvelope project)
    {
        var profile = project.State["profile"] as JsonObject;
        return new ProjectSummary(
            project.Id,
            ReadString(profile, "archiveTitle", "Proyecto sin título"),
            ReadString(profile, "storyTitle"),
            ReadString(profile, "genre"),
            ReadString(profile, "status", "Planificación"),
            ReadString(profile, "theme", "grim"),
            Count(project.State, "characters"),
            Count(project.State, "relationships"),
            Count(project.State, "world"),
            Count(project.State, "magicSystems"),
            project.UpdatedAt);
    }

    private static int Count(JsonObject state, string property) =>
        state[property] is JsonArray items ? items.Count : 0;

    private static string ReadString(JsonObject? source, string property, string fallback = "")
    {
        try
        {
            return source?[property]?.GetValue<string>()?.Trim() is { Length: > 0 } value ? value : fallback;
        }
        catch (InvalidOperationException)
        {
            return fallback;
        }
    }
}
