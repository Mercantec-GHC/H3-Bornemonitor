// Børnemonitor website: reads the backend API and shows status, measurements and diary.
const REFRESH_MS = 60_000;

const TYPES = {
  sleep: { icon: "😴", name: "Søvn" },
  walk: { icon: "🚶", name: "Gåtur" },
  eat: { icon: "🍼", name: "Mad" },
  diaper: { icon: "🧷", name: "Ble" },
};
// Values from docs/MQTT topics
const LIGHT = { very_low: "Meget lav", low: "Lav", medium: "Middel", high: "Høj", very_high: "Meget høj" };

const timeFormat = new Intl.DateTimeFormat("da-DK", { hour: "2-digit", minute: "2-digit" });
const dateTimeFormat = new Intl.DateTimeFormat("da-DK", { weekday: "short", day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" });

let chart = null;

async function getJson(url) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: ${response.status}`);
  return response.json();
}

function setText(id, text) {
  document.getElementById(id).textContent = text;
}

function showStatus(changes) {
  const el = document.getElementById("status");
  if (changes.length === 0) {
    el.textContent = "Ukendt";
    el.className = "status";
    setText("status-since", "Ingen statusskift endnu");
    return;
  }
  const current = changes[0];
  const online = current.state === "online";
  el.textContent = online ? "Online" : "Offline";
  el.className = "status " + (online ? "online" : "offline");
  setText("status-since", "Siden " + dateTimeFormat.format(new Date(current.timestamp)));
}

function showLatest(measurements) {
  if (measurements.length === 0) {
    setText("measured-at", "Ingen målinger endnu");
    return;
  }
  const latest = measurements[0]; // newest first
  setText("temp", latest.temperatureC.toFixed(1) + " °C");
  setText("hum", latest.humidityPct.toFixed(0) + " %");
  setText("light", LIGHT[latest.lightLevel] ?? latest.lightLevel);
  setText("measured-at", "Målt " + dateTimeFormat.format(new Date(latest.timestamp)) + " (gemmes hvert 15. min.)");
}

function showChart(measurements) {
  const points = [...measurements].reverse(); // oldest first for the chart
  const labels = points.map(m => timeFormat.format(new Date(m.timestamp)));
  const css = getComputedStyle(document.documentElement);

  const data = {
    labels,
    datasets: [
      { label: "Temperatur (°C)", data: points.map(m => m.temperatureC), borderColor: css.getPropertyValue("--temp"), yAxisID: "temp", pointRadius: 0, borderWidth: 2 },
      { label: "Luftfugtighed (%)", data: points.map(m => m.humidityPct), borderColor: css.getPropertyValue("--hum"), yAxisID: "hum", pointRadius: 0, borderWidth: 2 },
    ],
  };

  if (chart) {
    chart.data = data;
    chart.update();
    return;
  }

  chart = new Chart(document.getElementById("chart"), {
    type: "line",
    data,
    options: {
      maintainAspectRatio: false,
      animation: false,
      interaction: { mode: "index", intersect: false },
      scales: {
        x: { ticks: { maxTicksLimit: 8 } },
        temp: { position: "left", title: { display: true, text: "°C" } },
        hum: { position: "right", title: { display: true, text: "%" }, grid: { drawOnChartArea: false } },
      },
    },
  });
}

function describeDuration(entry) {
  if (entry.type === "eat" || entry.type === "diaper") return "";
  if (entry.endedAt === null) return "i gang";
  const minutes = Math.round(entry.durationMinutes);
  return minutes >= 60 ? `${Math.floor(minutes / 60)} t ${minutes % 60} min` : `${minutes} min`;
}

function showDiary(entries) {
  const list = document.getElementById("diary");
  list.replaceChildren();

  if (entries.length === 0) {
    const li = document.createElement("li");
    li.textContent = "Ingen hændelser de sidste 7 dage";
    list.append(li);
    return;
  }

  for (const entry of entries) {
    const type = TYPES[entry.type] ?? { icon: "•", name: entry.type };
    const li = document.createElement("li");

    const icon = document.createElement("span");
    icon.className = "icon";
    icon.textContent = type.icon;

    const text = document.createElement("div");
    const what = document.createElement("div");
    what.className = "what";
    what.textContent = type.name;
    const when = document.createElement("div");
    when.className = "muted";
    when.textContent = dateTimeFormat.format(new Date(entry.startedAt));
    text.append(what, when);

    const duration = document.createElement("span");
    duration.style.marginLeft = "auto";
    duration.textContent = describeDuration(entry);
    if (entry.endedAt === null && duration.textContent) duration.className = "running";

    li.append(icon, text, duration);
    list.append(li);
  }
}

async function refresh() {
  try {
    const [status, measurements, diary] = await Promise.all([
      getJson("/api/status"),
      getJson("/api/measurements?hours=24"),
      getJson("/api/diary?days=7"),
    ]);
    showStatus(status);
    showLatest(measurements);
    showChart(measurements);
    showDiary(diary);
    setText("updated", "Opdateret " + timeFormat.format(new Date()));
  } catch (error) {
    console.error(error);
    setText("updated", "Kunne ikke hente data – prøver igen om et minut");
  }
}

refresh();
setInterval(refresh, REFRESH_MS);
