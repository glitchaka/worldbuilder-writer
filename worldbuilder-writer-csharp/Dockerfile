FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY . .
RUN dotnet restore src/WorldbuilderWriter.Web/WorldbuilderWriter.Web.csproj
RUN dotnet publish src/WorldbuilderWriter.Web/WorldbuilderWriter.Web.csproj -c Release -o /app/publish --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:10.0 AS final
WORKDIR /app
ENV ASPNETCORE_URLS=http://+:8080
EXPOSE 8080
COPY --from=build /app/publish .
ENTRYPOINT ["dotnet", "WorldbuilderWriter.Web.dll"]
