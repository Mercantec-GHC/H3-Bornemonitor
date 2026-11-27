# Backend

*Andrii.* ASP.NET Core (.NET 10), minimal API. Kode: [`backend/Bornemonitor.Api`](../backend/Bornemonitor.Api).

## Status

| Del | Status |
|---|---|
| Forbindelse til MQTT-broker (#6) | ✅ |
| `child/telemetry` gemmes i Neon, tabel `Measurements` (#7) | ✅ |
| `GET /api/measurements` – seneste 100 målinger, nyeste først (#8); `?hours=24` til grafen | ✅ |
| Docker + deploy på deploy.mags.dk (#9) | ✅ kører på https://bornemonitor.mercantec.tech – release [v0.1.0](https://github.com/Mercantec-GHC/H3-Bornemonitor/releases/tag/v0.1.0) |
| `child/status` online/offline (#11), tabel `ChildStatusChanges` | ✅ |
| Genforbindelse til brokeren efter tabt forbindelse | ✅ |
| Dagbog (#12), tabel `DiaryEntries`, `GET /api/diary` | ✅ |
| Hjemmeside: status, målinger + graf, dagbog – https://bornemonitor.mercantec.tech | ✅ |
| JWT (#13) | ⬜ |

Ugyldig JSON eller JSON uden et påkrævet felt bliver logget (`fail`) og ikke gemt.

Målinger gemmes samlet **hvert 15. minut** (og når appen stopper), så Neon kan sove imellem – gratisplanen har 100 compute-timer om måneden. API'et viser derfor målinger med op til 15 min. forsinkelse.

**`child/status` (#11):** ingen telemetri i 60 sek. → backend publicerer `offline` med `last_seen`; telemetri igen → `online`. Kun ved skift, *retained*, QoS 1. Hvert skift gemmes i tabellen `ChildStatusChanges`. Efter genstart læser backenden den retained status fra brokeren og gentager den ikke. Logikken ligger i `ChildStatusTracker` og er dækket af unit-tests.

**Dagbog (#12):** `child/event/duration` (`sleep`/`walk`): `start` opretter en åben række, `stop` med samme `event_id` lukker den. `child/event/instant` (`eat`/`diaper`): én række. Tid = `timestamp` fra Arduinoen. Dubletter (samme `event_id`) ignoreres – i koden og med et unikt indeks i databasen. `stop` uden `start` og forkert type/action logges og gemmes ikke. Backend abonnerer med QoS 1, så hændelser ikke tabes mellem broker og backend. Hændelser gemmes inden for 5 sek. Reglerne ligger i `DiaryRules` og er dækket af unit-tests.

## API og hjemmeside

| Adresse | Hvad |
|---|---|
| `/` | Hjemmesiden (`wwwroot/`: `index.html`, `app.js`, `style.css`) – opdaterer sig selv hvert minut |
| `GET /api/measurements` | seneste 100 målinger; `?hours=1..168` = alle fra perioden |
| `GET /api/diary` | dagbog, nyeste først; `?days=1..90` (standard 7). `durationMinutes` = `null` mens søvn/gåtur er i gang |
| `GET /api/status` | seneste 20 skift online/offline; første = nuværende status |

Grafen bruger [Chart.js](https://www.chartjs.org/) 4.4.1 fra CDN. Ingen build – filerne serveres direkte af backenden.

## Kør lokalt

Hemmeligheder ligger i user-secrets, ikke i Git:

```bash
cd backend/Bornemonitor.Api
dotnet user-secrets set "Mqtt:Host" "<broker>"
dotnet user-secrets set "Mqtt:Username" "<bruger>"
dotnet user-secrets set "Mqtt:Password" "<adgangskode>"
dotnet user-secrets set "ConnectionStrings:Neon" "Host=...;Database=...;Username=...;Password=...;SSL Mode=Require"
dotnet run
```

Unit-tests (fra mappen `backend`): `dotnet test -m:1` (`-m:1`, ellers kan to projekter bygge samtidig og fejle på `apphost`).

Neon-strengen skal være i formatet `Host=...;` – Npgsql forstår ikke `postgresql://`.

**Bemærk:** Når backenden også kører på serveren, gemmes hver besked to gange, hvis du kører den lokalt samtidig.

## Deploy

[deploy.mags.dk](https://deploy.mags.dk) (Dokploy, fælles bruger fra Notion, dag 16) → projekt **Bornemonitor** → service **backend** (Compose).
Direkte link (kræver login): [backend i Dokploy](https://deploy.mags.dk/dashboard/project/CLiaE6P1JNfa1rGIFAKO9/environment/H0Hz_VntjnQ520C4BkHq_/services/compose/6C54WEEf06svCZdPtPzc0). Fanen **Logs** viser, hvad backenden laver lige nu.

- Kilde: dette repo, branch `dev`, fil [`compose.yaml`](../compose.yaml). Dokploy bygger image ud fra [`Dockerfile`](../backend/Bornemonitor.Api/Dockerfile).
- Hemmeligheder i fanen *Environment*: `MQTT_HOST`, `MQTT_USERNAME`, `MQTT_PASSWORD`, `NEON_CONNECTION`.
- Ny version: push til `dev` → *Deploy* i Dokploy.
- Domæne: **https://bornemonitor.mercantec.tech** → port `8080` (vælges selv i fanen *Domains*, går via Cloudflare). sslip.io-domænet kan ikke nås udefra.

To begrænsninger på serveren, som `compose.yaml` tager højde for:

1. Docker kan ikke oprette nye netværk → servicen bruger det eksisterende `dokploy-network`.
2. Ingen ledige inotify-instanser → `DOTNET_hostBuilder__reloadConfigOnChange: "false"`.

Derfor virker `docker compose up` ikke lokalt (netværket findes kun på serveren). Brug `dotnet run`.
