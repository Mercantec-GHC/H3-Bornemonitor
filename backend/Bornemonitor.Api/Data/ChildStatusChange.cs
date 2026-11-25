namespace Bornemonitor.Api.Data;

// When the child Arduino went online or offline (#11), for the diary and the website
public class ChildStatusChange
{
    public long Id { get; set; }
    public string State { get; set; } = "";
    public DateTime Timestamp { get; set; }
}
