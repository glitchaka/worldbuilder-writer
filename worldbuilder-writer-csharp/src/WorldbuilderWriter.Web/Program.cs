using WorldbuilderWriter.Core.Services;
using WorldbuilderWriter.Infrastructure.Manuscripts;
using WorldbuilderWriter.Infrastructure.Projects;
using WorldbuilderWriter.UI;
using WorldbuilderWriter.Web.Components;

var builder = WebApplication.CreateBuilder(args);

builder.Services
    .AddRazorComponents()
    .AddInteractiveServerComponents();

var configuredRoot = builder.Configuration["WorldbuilderWriter:ProjectRoot"];
builder.Services.AddSingleton<IProjectRepository>(_ => new FileProjectRepository(ProjectRootResolver.Resolve(configuredRoot)));
builder.Services.AddSingleton<IManuscriptImportService, DocxManuscriptImportService>();

var app = builder.Build();

if (!app.Environment.IsDevelopment())
{
    app.UseExceptionHandler("/Error", createScopeForErrors: true);
    app.UseHsts();
}

app.UseHttpsRedirection();
app.UseAntiforgery();
app.MapStaticAssets();
app.MapRazorComponents<App>()
    .AddInteractiveServerRenderMode()
    .AddAdditionalAssemblies(typeof(AssemblyMarker).Assembly);

app.Run();
