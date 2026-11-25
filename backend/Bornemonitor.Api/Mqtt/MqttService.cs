using MQTTnet;
using MQTTnet.Protocol;
using System.Collections.Concurrent;
using System.Text.Json;
using System.Text.Json.Serialization;
using Bornemonitor.Api.Data;
using Microsoft.EntityFrameworkCore;

namespace Bornemonitor.Api.Mqtt;

public class MqttService(IConfiguration config, ILogger<MqttService> logger, IDbContextFactory<AppDbContext> dbFactory) : BackgroundService
{
    private const string TopicFilter = "child/#";
    private const string StatusTopic = "child/status";

    // Save to the database in batches, so Neon can sleep between saves (free plan: 100 compute hours per month)
    private static readonly TimeSpan SaveInterval = TimeSpan.FromMinutes(15);

    // The child sends telemetry every 20 sec. No telemetry in 60 sec (3 messages) = offline (#11)
    private static readonly TimeSpan OfflineTimeout = TimeSpan.FromSeconds(60);
    private static readonly TimeSpan CheckInterval = TimeSpan.FromSeconds(5);

    private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull // "online" has no last_seen
    };

    // Measurements waiting to be saved. Concurrent, because MQTT adds and the timer removes at the same time.
    private readonly ConcurrentQueue<Measurement> buffer = new ConcurrentQueue<Measurement>();

    private readonly ChildStatusTracker statusTracker = new ChildStatusTracker(OfflineTimeout, DateTime.UtcNow);

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
                    HandleTelemetry(payload);
                else if (topic == StatusTopic && eventArgs.ApplicationMessage.Retain)
                    RestoreStatus(payload);
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

        // If the child is silent right after a restart, last_seen comes from the newest saved measurement
        statusTracker.Restore(null, await GetLastMeasurementTimeAsync(stoppingToken));

        await client.ConnectAsync(options, stoppingToken);
        logger.LogInformation("Connected to MQTT broker {Host}", config["Mqtt:Host"]);

        // The broker sends the retained child/status right away, so the backend knows what it published before a restart
        await client.SubscribeAsync(TopicFilter, cancellationToken: stoppingToken);
        logger.LogInformation("Subscribed to {Topic}", TopicFilter);

        using PeriodicTimer timer = new(CheckInterval);
        DateTime lastSave = DateTime.UtcNow;
        try
        {
            while (await timer.WaitForNextTickAsync(stoppingToken))
            {
                await PublishStatusIfChangedAsync(client, stoppingToken);

                if (DateTime.UtcNow - lastSave >= SaveInterval)
                {
                    await SaveBufferAsync();
                    lastSave = DateTime.UtcNow;
                }
            }
        }
        catch (OperationCanceledException)
        {
            // The app is stopping
        }

        // Save what is left before the app stops
        await SaveBufferAsync();
    }

    private void HandleTelemetry(string? payload)
    {
        TelemetryMessage msg = JsonSerializer.Deserialize<TelemetryMessage>(payload ?? "", JsonOptions)!;

        buffer.Enqueue(new Measurement
        {
            TemperatureC = msg.TemperatureC,
            HumidityPct = msg.HumidityPct,
            LightLevel = msg.LightLevel,
            Timestamp = msg.Timestamp
        });

        statusTracker.RecordTelemetry(msg.Timestamp, DateTime.UtcNow);
    }

    private void RestoreStatus(string? payload)
    {
        ChildStatusMessage msg = JsonSerializer.Deserialize<ChildStatusMessage>(payload ?? "", JsonOptions)!;
        statusTracker.Restore(msg.State, msg.LastSeen);
        logger.LogInformation("Last published child status: {State}", msg.State);
    }

    private async Task PublishStatusIfChangedAsync(IMqttClient client, CancellationToken stoppingToken)
    {
        StatusChange? change = statusTracker.Check(DateTime.UtcNow);
        if (change == null)
            return;

        ChildStatusMessage msg = new ChildStatusMessage { State = change.State, LastSeen = change.LastSeen };
        MqttApplicationMessage message = new MqttApplicationMessageBuilder()
            .WithTopic(StatusTopic)
            .WithPayload(JsonSerializer.Serialize(msg, JsonOptions))
            .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
            .WithRetainFlag() // the parent unit gets the status right after it restarts
            .Build();

        await client.PublishAsync(message, stoppingToken);
        logger.LogInformation("Published child status {State}, last seen {LastSeen}", change.State, change.LastSeen);

        try
        {
            using AppDbContext db = dbFactory.CreateDbContext();
            db.ChildStatusChanges.Add(new ChildStatusChange { State = change.State, Timestamp = DateTime.UtcNow });
            await db.SaveChangesAsync(stoppingToken);
        }
        catch (Exception ex)
        {
            // The parent unit already got the status, so only the diary misses this change
            logger.LogError(ex, "Failed to save child status {State}", change.State);
        }
    }

    private async Task<DateTime?> GetLastMeasurementTimeAsync(CancellationToken stoppingToken)
    {
        try
        {
            using AppDbContext db = dbFactory.CreateDbContext();
            return await db.Measurements.MaxAsync(m => (DateTime?)m.Timestamp, stoppingToken);
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Failed to read the last measurement time");
            return null;
        }
    }

    private async Task SaveBufferAsync()
    {
        List<Measurement> batch = new List<Measurement>();
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
