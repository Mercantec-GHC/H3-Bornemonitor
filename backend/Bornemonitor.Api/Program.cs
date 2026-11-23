using Bornemonitor.Api.Mqtt;
using Bornemonitor.Api.Data;
using Microsoft.EntityFrameworkCore;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddOpenApi();
builder.Services.AddDbContextFactory<AppDbContext>(options => options.UseNpgsql(builder.Configuration.GetConnectionString("Neon")));
builder.Services.AddHostedService<MqttService>();

var app = builder.Build();

if (app.Environment.IsDevelopment())
{
    app.MapOpenApi();
}

app.MapGet("/", () => "Bornemonitor API");

app.MapGet("/api/measurements", async (AppDbContext db) =>
    await db.Measurements
        .OrderByDescending(m => m.Timestamp)
        .Take(100)
        .ToListAsync());

app.Run();