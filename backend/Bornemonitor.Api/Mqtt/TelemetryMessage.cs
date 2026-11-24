namespace Bornemonitor.Api.Mqtt;

public class TelemetryMessage
{
    public required double TemperatureC { get; set; }
    public required double HumidityPct { get; set; }
    public required string LightLevel { get; set; }
    public required DateTime Timestamp { get; set; }
}
