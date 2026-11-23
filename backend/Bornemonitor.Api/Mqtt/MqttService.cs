using MQTTnet;
using System.Text.Json;
using Bornemonitor.Api.Data;
using Microsoft.EntityFrameworkCore;

namespace Bornemonitor.Api.Mqtt;

public class MqttService(IConfiguration config, ILogger<MqttService> logger, IDbContextFactory<AppDbContext> dbFactory) : BackgroundService
{
    private const string TopicFilter = "child/#";
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        IMqttClient client = new MqttClientFactory().CreateMqttClient();

        client.ApplicationMessageReceivedAsync += async eventArgs =>
        {
            string topic = eventArgs.ApplicationMessage.Topic;
            string? payload = eventArgs.ApplicationMessage.ConvertPayloadToString();

            try
            {
                if (topic == "child/telemetry")
                    await SaveTelemetryAsync(payload);
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "Failed to handle {Topic}: {Payload}", topic, payload);
            }
        };

        MqttClientOptions options = new MqttClientOptionsBuilder()
            .WithTcpServer(config["Mqtt:Host"], 8883)
            .WithCredentials(config["Mqtt:Username"], config["Mqtt:Password"])
            .WithTlsOptions(tls => tls.UseTls())
            .WithClientId($"backend-{Guid.NewGuid():N}")
            .Build();

        await client.ConnectAsync(options, stoppingToken);
        logger.LogInformation("Connected to MQTT broker {Host}", config["Mqtt:Host"]);

        await client.SubscribeAsync(TopicFilter, cancellationToken: stoppingToken);
        logger.LogInformation("Subscribed to {Topic}", TopicFilter);

        await Task.Delay(Timeout.Infinite, stoppingToken);
    }

    private async Task SaveTelemetryAsync(string? payload)
    {
        TelemetryMessage msg = JsonSerializer.Deserialize<TelemetryMessage>(payload ?? "", JsonOptions)!;

        using AppDbContext db = dbFactory.CreateDbContext();

        db.Measurements.Add(new Measurement
        {
            TemperatureC = msg.TemperatureC,
            HumidityPct = msg.HumidityPct,
            LightLevel = msg.LightLevel,
            Timestamp = msg.Timestamp
        });
        await db.SaveChangesAsync();
    }
}
