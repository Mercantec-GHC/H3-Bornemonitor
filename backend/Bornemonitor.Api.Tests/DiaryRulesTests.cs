using Bornemonitor.Api.Data;
using Bornemonitor.Api.Diary;
using Bornemonitor.Api.Mqtt;

namespace Bornemonitor.Api.Tests;

public class DiaryRulesTests
{
    private static readonly DateTime Time = new DateTime(2026, 11, 27, 10, 0, 0, DateTimeKind.Utc);

    private static DiaryEventMessage Msg(string type, string? action) =>
        new DiaryEventMessage { EventId = 1795074236, Type = type, Action = action, Timestamp = Time };

    private static DiaryEntry Row(string type, DateTime? endedAt) =>
        new DiaryEntry { EventId = 1795074236, Type = type, StartedAt = Time, EndedAt = endedAt };

    [Fact]
    public void Start_New_Creates() =>
        Assert.Equal(DiaryDecision.Create, DiaryRules.Decide(Msg("sleep", "start"), true, null));

    [Fact]
    public void Start_Again_IsIgnored() =>
        Assert.Equal(DiaryDecision.IgnoreDuplicate, DiaryRules.Decide(Msg("sleep", "start"), true, Row("sleep", null)));

    [Fact]
    public void Stop_OnOpenRow_Closes() =>
        Assert.Equal(DiaryDecision.Close, DiaryRules.Decide(Msg("walk", "stop"), true, Row("walk", null)));

    [Fact]
    public void Stop_OnClosedRow_IsIgnored() =>
        Assert.Equal(DiaryDecision.IgnoreDuplicate, DiaryRules.Decide(Msg("walk", "stop"), true, Row("walk", Time.AddMinutes(30))));

    [Fact]
    public void Stop_WithoutStart_IsNotSaved() =>
        Assert.Equal(DiaryDecision.StopWithoutStart, DiaryRules.Decide(Msg("sleep", "stop"), true, null));

    [Fact]
    public void Stop_ForOtherType_IsInvalid() =>
        Assert.Equal(DiaryDecision.Invalid, DiaryRules.Decide(Msg("walk", "stop"), true, Row("sleep", null)));

    [Fact]
    public void Instant_New_Creates() =>
        Assert.Equal(DiaryDecision.Create, DiaryRules.Decide(Msg("diaper", null), false, null));

    [Fact]
    public void Instant_Again_IsIgnored() =>
        Assert.Equal(DiaryDecision.IgnoreDuplicate, DiaryRules.Decide(Msg("eat", null), false, Row("eat", null)));

    [Theory]
    [InlineData("eat", "start", true)]      // instant type on the duration topic
    [InlineData("sleep", null, false)]      // duration type on the instant topic
    [InlineData("eat", "start", false)]     // instant event with an action
    [InlineData("sleep", "pause", true)]    // unknown action
    [InlineData("sleep", null, true)]       // duration event without action
    public void WrongTypeOrAction_IsInvalid(string type, string? action, bool isDuration) =>
        Assert.Equal(DiaryDecision.Invalid, DiaryRules.Decide(Msg(type, action), isDuration, null));
}
