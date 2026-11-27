using Bornemonitor.Api.Mqtt;

namespace Bornemonitor.Api.Tests;

public class ChildStatusTrackerTests
{
    private static readonly DateTime Start = new DateTime(2026, 11, 25, 9, 0, 0, DateTimeKind.Utc);
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(60);

    private static ChildStatusTracker NewTracker() => new ChildStatusTracker(Timeout, Start);

    [Fact]
    public void Telemetry_GivesOnline()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.RecordTelemetry(Start, Start);

        StatusChange? change = tracker.Check(Start.AddSeconds(5));

        Assert.Equal(new StatusChange("online", null), change);
    }

    [Fact]
    public void Online_IsNotRepeated()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.RecordTelemetry(Start, Start);
        tracker.Check(Start.AddSeconds(5));

        tracker.RecordTelemetry(Start.AddSeconds(20), Start.AddSeconds(20));

        Assert.Null(tracker.Check(Start.AddSeconds(25)));
    }

    [Fact]
    public void NoTelemetryFor59Seconds_StaysOnline()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.RecordTelemetry(Start, Start);
        tracker.Check(Start);

        Assert.Null(tracker.Check(Start.AddSeconds(59)));
    }

    [Fact]
    public void NoTelemetryFor60Seconds_GivesOfflineWithLastTelemetryTimestamp()
    {
        ChildStatusTracker tracker = NewTracker();
        DateTime arduinoTime = Start.AddSeconds(-2); // the Arduino clock is a bit behind
        tracker.RecordTelemetry(arduinoTime, Start);
        tracker.Check(Start);

        StatusChange? change = tracker.Check(Start.AddSeconds(60));

        Assert.Equal(new StatusChange("offline", arduinoTime), change);
    }

    [Fact]
    public void Offline_IsNotRepeated()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.RecordTelemetry(Start, Start);
        tracker.Check(Start);
        tracker.Check(Start.AddSeconds(60));

        Assert.Null(tracker.Check(Start.AddSeconds(120)));
    }

    [Fact]
    public void TelemetryAfterOffline_GivesOnline()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.RecordTelemetry(Start, Start);
        tracker.Check(Start);
        tracker.Check(Start.AddSeconds(60));

        tracker.RecordTelemetry(Start.AddSeconds(90), Start.AddSeconds(90));

        Assert.Equal(new StatusChange("online", null), tracker.Check(Start.AddSeconds(95)));
    }

    [Fact]
    public void NoTelemetrySinceStart_GivesOfflineAfter60SecondsWithRestoredLastSeen()
    {
        ChildStatusTracker tracker = NewTracker();
        DateTime lastSeenBeforeRestart = Start.AddMinutes(-10);
        tracker.Restore(null, lastSeenBeforeRestart);

        Assert.Null(tracker.Check(Start.AddSeconds(59)));
        Assert.Equal(new StatusChange("offline", lastSeenBeforeRestart), tracker.Check(Start.AddSeconds(60)));
    }

    [Fact]
    public void RestoredOffline_IsNotPublishedAgainAfterRestart()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.Restore("offline", Start.AddMinutes(-10));

        Assert.Null(tracker.Check(Start.AddSeconds(60)));
    }

    [Fact]
    public void RestoredOnline_IsNotPublishedAgainWhenTelemetryArrives()
    {
        ChildStatusTracker tracker = NewTracker();
        tracker.Restore("online", null);
        tracker.RecordTelemetry(Start.AddSeconds(10), Start.AddSeconds(10));

        Assert.Null(tracker.Check(Start.AddSeconds(15)));
    }
}
