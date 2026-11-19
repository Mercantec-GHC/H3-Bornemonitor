namespace Bornemonitor.Api.Data;

public class Measurement
{
    public long Id { get; set; }
    public double TemperatureC { get; set; }
    public double HumidityPct { get; set; }
    public string LightLevel { get; set; } = "";
    public DateTime Timestamp { get; set; }
}
