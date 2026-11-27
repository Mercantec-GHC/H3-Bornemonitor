namespace Bornemonitor.Api.Mqtt;

public record StatusChange(string State, DateTime? LastSeen);

// Decides if the child Arduino is online or offline from its telemetry (#11).
// No MQTT or database here, so it can be unit tested with fake times.
public class ChildStatusTracker(TimeSpan timeout, DateTime startedAt)
{
    // MQTT adds telemetry and the timer checks at the same time
    private readonly Lock gate = new Lock();

    private DateTime? lastReceivedAt; // server clock, used for the timeout
    private DateTime? lastSeen;       // Arduino timestamp, sent to the parent unit
    private string? publishedState;   // "online", "offline" or null (not known yet)

    // After a restart: the last published status (retained message) and the last known telemetry time
    public void Restore(string? state, DateTime? lastSeenBefore)
    {
        lock (gate)
        {
            publishedState ??= state;
            lastSeen ??= lastSeenBefore;
        }
    }

    public void RecordTelemetry(DateTime timestamp, DateTime now)
    {
        lock (gate)
        {
            lastReceivedAt = now;
            lastSeen = timestamp;
        }
    }

    // Returns a change to publish, or null if nothing changed
    public StatusChange? Check(DateTime now)
    {
        lock (gate)
        {
            DateTime lastActivity = lastReceivedAt ?? startedAt;
            bool isOnline = now - lastActivity < timeout && lastReceivedAt != null;
            bool isOffline = now - lastActivity >= timeout;

            if (isOnline && publishedState != "online")
            {
                publishedState = "online";
                return new StatusChange("online", null);
            }

            if (isOffline && publishedState != "offline")
            {
                publishedState = "offline";
                return new StatusChange("offline", lastSeen);
            }

            return null;
        }
    }
}
