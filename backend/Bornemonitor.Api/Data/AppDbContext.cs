using Microsoft.EntityFrameworkCore;

namespace Bornemonitor.Api.Data;

public class AppDbContext(DbContextOptions<AppDbContext> options) : DbContext(options)
{
    public DbSet<Measurement> Measurements => Set<Measurement>();
    public DbSet<ChildStatusChange> ChildStatusChanges => Set<ChildStatusChange>();
    public DbSet<DiaryEntry> DiaryEntries => Set<DiaryEntry>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        // Duplicates (same event_id) are stopped by the database too, not only by the code
        modelBuilder.Entity<DiaryEntry>().HasIndex(e => e.EventId).IsUnique();
    }
}