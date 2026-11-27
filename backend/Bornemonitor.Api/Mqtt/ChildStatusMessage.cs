namespace Bornemonitor.Api.Mqtt;

// child/status: {"state": "offline", "last_seen": "..."} or {"state": "online"}
public class ChildStatusMessage
{
    public required string State { get; set; }
    public DateTime? LastSeen { get; set; }
}
