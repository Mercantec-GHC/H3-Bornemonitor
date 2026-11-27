namespace Bornemonitor.Api.Data;

// One diary row (#12): sleep/walk with start and end, or eat/diaper at one moment
public class DiaryEntry
{
    public long Id { get; set; }
    public long EventId { get; set; }     // Unix time of the button press, the same for start and stop. Unique.
    public string Type { get; set; } = ""; // sleep, walk, eat, diaper
    public DateTime StartedAt { get; set; }
    public DateTime? EndedAt { get; set; } // null: still running (sleep/walk) or no duration (eat/diaper)
}
