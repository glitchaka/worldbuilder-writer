using System.Text.Json.Nodes;

namespace WorldbuilderWriter.Core.Projects;

public static class ArchiveStateFactory
{
    public static ProjectEnvelope CreateBlank(string archiveTitle, string? storyTitle = null, string theme = "grim")
    {
        var cleanTitle = string.IsNullOrWhiteSpace(archiveTitle) ? "Archivo sin título" : archiveTitle.Trim();
        var cleanStoryTitle = storyTitle?.Trim() ?? string.Empty;
        var cleanTheme = theme is "grim" or "classic" or "kawaii" or "chronicle" or "desk" ? theme : "grim";
        var now = DateTimeOffset.UtcNow;

        var state = new JsonObject
        {
            ["dataVersion"] = 13,
            ["title"] = cleanTitle,
            ["profile"] = new JsonObject
            {
                ["archiveTitle"] = cleanTitle,
                ["storyTitle"] = cleanStoryTitle,
                ["subtitle"] = "Biblia narrativa editable",
                ["projectLabel"] = "Archivo de historia · Proyecto nuevo",
                ["homeHeading"] = "Primeras conexiones",
                ["location"] = string.Empty,
                ["author"] = string.Empty,
                ["genre"] = string.Empty,
                ["status"] = "Planificación",
                ["synopsis"] = string.Empty,
                ["chapterLabels"] = new JsonArray { "1" },
                ["theme"] = cleanTheme,
                ["activeThemeId"] = cleanTheme,
                ["customThemes"] = new JsonArray(),
                ["spotifyPlaylistUrl"] = string.Empty,
                ["youtubeAmbientUrl"] = string.Empty,
                ["coverImageDataUrl"] = string.Empty,
                ["bannerImageDataUrl"] = string.Empty,
                ["boardIconDataUrl"] = string.Empty,
                ["boardViewport"] = new JsonObject { ["scale"] = 0.55, ["x"] = 42, ["y"] = 32, ["worldSpace"] = true },
                ["sceneBoardViewport"] = new JsonObject { ["scale"] = 1, ["x"] = 18, ["y"] = 18, ["worldSpace"] = true },
                ["sceneBoardCompact"] = false,
                ["manuscriptLayout"] = DefaultManuscriptLayout(),
                ["writingAnalysis"] = DefaultWritingAnalysis(),
            },
            ["manuscript"] = new JsonObject
            {
                ["fileName"] = string.Empty,
                ["words"] = 0,
                ["chapters"] = 0,
                ["updatedLabel"] = "Recién creado",
            },
            ["characters"] = new JsonArray(),
            ["relationships"] = new JsonArray(),
            ["theories"] = new JsonArray(),
            ["timeline"] = new JsonArray(),
            ["world"] = new JsonArray(),
            ["magicSystems"] = new JsonArray(),
            ["worldTexts"] = new JsonArray(),
            ["magicTexts"] = new JsonArray(),
            ["writingChapters"] = new JsonArray(),
            ["maps"] = new JsonArray(),
            ["narrativeHeat"] = new JsonArray(),
        };

        return new ProjectEnvelope
        {
            Id = $"project-{Guid.NewGuid():N}",
            CreatedAt = now,
            UpdatedAt = now,
            State = state,
        };
    }

    private static JsonObject DefaultManuscriptLayout() => new()
    {
        ["preset"] = "editorial",
        ["pageWidthMm"] = 152.4,
        ["pageHeightMm"] = 228.6,
        ["marginTopMm"] = 20,
        ["marginRightMm"] = 19,
        ["marginBottomMm"] = 22,
        ["marginLeftMm"] = 19,
        ["fontFamily"] = "Garamond",
        ["fontSizePt"] = 11,
        ["lineHeight"] = 1.35,
        ["paragraphIndentMm"] = 5,
        ["chapterOpening"] = "Página nueva",
        ["sceneSeparator"] = "⁂",
        ["headerText"] = "{título}",
        ["footerText"] = "{página}",
    };

    private static JsonObject DefaultWritingAnalysis() => new()
    {
        ["repetitionWindow"] = 40,
        ["fillerPhrases"] = new JsonArray { "de repente", "entonces", "bueno", "en realidad", "de alguna manera", "era", "estaba", "había" },
    };
}
