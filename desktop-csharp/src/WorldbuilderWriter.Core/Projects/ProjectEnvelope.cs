using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

namespace WorldbuilderWriter.Core.Projects;

public sealed class ProjectEnvelope
{
    [JsonPropertyName("formatVersion")]
    public int FormatVersion { get; init; } = 1;

    [JsonPropertyName("id")]
    public required string Id { get; init; }

    [JsonPropertyName("createdAt")]
    public required DateTimeOffset CreatedAt { get; init; }

    [JsonPropertyName("updatedAt")]
    public required DateTimeOffset UpdatedAt { get; set; }

    [JsonPropertyName("state")]
    public required JsonObject State { get; init; }
}
