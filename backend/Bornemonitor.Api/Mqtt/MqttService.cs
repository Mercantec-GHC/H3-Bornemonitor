using MQTTnet;
using System.Collections.Concurrent;
using System.Text.Json;
using Bornemonitor.Api.Data;
using Microsoft.EntityFrameworkCore;

namespace Bornemonitor.Api.Mqtt;

public class MqttService(IConfiguration config, ILogger<MqttService> logger, IDbContextFactory<AppDbContext> dbFactory) : BackgroundService
{
    private const string TopicFilter = "child/#";

    // Save to the database in batches, so Neon can sleep between saves (free plan: 100 compute hours per month)
    private static readonly TimeSpan SaveInterval = TimeSpan.FromMinutes(15);

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    // Measurements waiting to be saved. Concurrent, because MQTT adds and the timer removes at the same time.
    private readonly ConcurrentQueue<Measurement> buffer = new();

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        IMqttClient client = new MqttClientFactory().CreateMqttClient();

        client.ApplicationMessageReceivedAsync += eventArgs =>
        {
            string topic = eventArgs.ApplicationMessage.Topic;
            string? payload = eventArgs.ApplicationMessage.ConvertPayloadToString();

            try
            {
                if (topic == "child/telemetry")
                    AddTelemetryToBuffer(payload);
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "Failed to handle {Topic}: {Payload}", topic, payload);
            }

            return Task.CompletedTask;
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

        using PeriodicTimer timer = new(SaveInterval);
        try
        {
            while (await timer.WaitForNextTickAsync(stoppingToken))
                await SaveBufferAsync();
        }
        catch (OperationCanceledException)
        {
            // The app is stopping
        }

        // Save what is left before the app stops
        await SaveBufferAsync();
    }

    private void AddTelemetryToBuffer(string? payload)
    {
        TelemetryMessage msg = JsonSerializer.Deserialize<TelemetryMessage>(payload ?? "", JsonOptions)!;

        buffer.Enqueue(new Measurement
        {
            TemperatureC = msg.TemperatureC,
            HumidityPct = msg.HumidityPct,
            LightLevel = msg.LightLevel,
            Timestamp = msg.Timestamp
        });
    }

    private async Task SaveBufferAsync()
    {
        List<Measurement> batch = [];
        while (buffer.TryDequeue(out Measurement? measurement))
            batch.Add(measurement);

        if (batch.Count == 0)
            return;

        try
        {
            using AppDbContext db = dbFactory.CreateDbContext();
            db.Measurements.AddRange(batch);
            await db.SaveChangesAsync();
            logger.LogInformation("Saved {Count} measurements", batch.Count);
        }
        catch (Exception ex)
        {
            // Put them back and try again next time
            foreach (Measurement measurement in batch)
                buffer.Enqueue(measurement);
            logger.LogError(ex, "Failed to save {Count} measurements, will retry", batch.Count);
        }
    }
}
