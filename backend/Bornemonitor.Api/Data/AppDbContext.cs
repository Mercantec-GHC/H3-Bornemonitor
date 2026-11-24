using Microsoft.EntityFrameworkCore;

namespace Bornemonitor.Api.Data;

public class AppDbContext(DbContextOptions<AppDbContext> options) : DbContext(options)
{
    public DbSet<Measurement> Measurements => Set<Measurement>();
}