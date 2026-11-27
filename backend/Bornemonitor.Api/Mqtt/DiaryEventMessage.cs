namespace Bornemonitor.Api.Mqtt;

// child/event/duration: {"event_id": 1795074236, "type": "sleep", "action": "start", "timestamp": "..."}
// child/event/instant:  {"event_id": 1795074240, "type": "eat", "timestamp": "..."}
public class DiaryEventMessage
{
    public required long EventId { get; set; }
    public required string Type { get; set; }
    public string? Action { get; set; } // "start" / "stop", only for duration events
    public required DateTime Timestamp { get; set; }
}
