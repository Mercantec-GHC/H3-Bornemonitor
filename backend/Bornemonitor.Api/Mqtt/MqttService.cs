using MQTTnet;

namespace Bornemonitor.Api.Mqtt;

public class MqttService(IConfiguration config, ILogger<MqttService> logger) : BackgroundService
{
    private const string TopicFilter = "child/#";

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var client = new MqttClientFactory().CreateMqttClient();

        client.ApplicationMessageReceivedAsync += e =>
        {
            var topic = e.ApplicationMessage.Topic;
            var payload = e.ApplicationMessage.ConvertPayloadToString();
            logger.LogInformation("MQTT {Topic}: {Payload}", topic, payload);
            return Task.CompletedTask;
        };

        var options = new MqttClientOptionsBuilder()
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
}