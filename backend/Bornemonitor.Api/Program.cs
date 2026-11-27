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

// The website: wwwroot/index.html, app.js, style.css
app.UseDefaultFiles();
app.UseStaticFiles();

// Without hours: the newest 100. With hours (1-168): everything from that period, for the chart.
app.MapGet("/api/measurements", async (AppDbContext db, int? hours) =>
{
    IQueryable<Measurement> query = db.Measurements.OrderByDescending(m => m.Timestamp);

    if (hours == null)
        return Results.Ok(await query.Take(100).ToListAsync());

    if (hours < 1 || hours > 168)
        return Results.BadRequest("hours must be 1-168");

    DateTime from = DateTime.UtcNow.AddHours(-hours.Value);
    return Results.Ok(await query.Where(m => m.Timestamp >= from).ToListAsync());
});

// Diary (#12), newest first. durationMinutes is null while sleep/walk is running and for eat/diaper.
app.MapGet("/api/diary", async (AppDbContext db, int? days) =>
{
    int period = days ?? 7;
    if (period < 1 || period > 90)
        return Results.BadRequest("days must be 1-90");

    DateTime from = DateTime.UtcNow.AddDays(-period);
    List<DiaryEntry> entries = await db.DiaryEntries
        .Where(e => e.StartedAt >= from)
        .OrderByDescending(e => e.StartedAt)
        .ToListAsync();

    return Results.Ok(entries.Select(e => new
    {
        e.EventId,
        e.Type,
        e.StartedAt,
        e.EndedAt,
        DurationMinutes = e.EndedAt == null ? (double?)null : Math.Round((e.EndedAt.Value - e.StartedAt).TotalMinutes, 1)
    }));
});

// Child Arduino online/offline (#11): the newest change first. The first item is the current status.
app.MapGet("/api/status", async (AppDbContext db) =>
    await db.ChildStatusChanges
        .OrderByDescending(c => c.Timestamp)
        .Take(20)
        .ToListAsync());

app.Run();