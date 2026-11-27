using Bornemonitor.Api.Data;
using Bornemonitor.Api.Mqtt;

namespace Bornemonitor.Api.Diary;

public enum DiaryDecision
{
    Create,          // new row
    Close,           // set EndedAt on the open row
    IgnoreDuplicate, // already saved (QoS 1 and the Arduino queue can send again)
    StopWithoutStart,// the start was lost, nothing to close
    Invalid          // wrong type or action for the topic
}

// What to do with one diary event (#12). No database here, so it can be unit tested.
public static class DiaryRules
{
    private static readonly string[] DurationTypes = ["sleep", "walk"];
    private static readonly string[] InstantTypes = ["eat", "diaper"];

    // existing = the saved row with the same event_id, or null
    public static DiaryDecision Decide(DiaryEventMessage msg, bool isDuration, DiaryEntry? existing)
    {
        if (!isDuration)
        {
            if (!InstantTypes.Contains(msg.Type) || msg.Action != null)
                return DiaryDecision.Invalid;
            return existing == null ? DiaryDecision.Create : DiaryDecision.IgnoreDuplicate;
        }

        if (!DurationTypes.Contains(msg.Type))
            return DiaryDecision.Invalid;

        if (msg.Action == "start")
            return existing == null ? DiaryDecision.Create : DiaryDecision.IgnoreDuplicate;

        if (msg.Action == "stop")
        {
            if (existing == null)
                return DiaryDecision.StopWithoutStart;
            if (existing.Type != msg.Type)
                return DiaryDecision.Invalid;
            return existing.EndedAt == null ? DiaryDecision.Close : DiaryDecision.IgnoreDuplicate;
        }

        return DiaryDecision.Invalid;
    }
}
