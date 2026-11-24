# Backend

*Andrii.* ASP.NET Core (.NET 10), minimal API. Kode: [`backend/Bornemonitor.Api`](../backend/Bornemonitor.Api).

## Status

| Del | Status |
|---|---|
| Forbindelse til MQTT-broker (#6) | ✅ |
| `child/telemetry` gemmes i Neon, tabel `Measurements` (#7) | ✅ |
| `GET /api/measurements` – seneste 100 målinger, nyeste først (#8) | ✅ |
| Docker + deploy på deploy.mags.dk (#9) | ✅ kører – ⏳ venter på domæne, release v0.1 |
| `child/status`, dagbog, JWT (#11–#13) | ⬜ |

Ugyldig JSON eller JSON uden et påkrævet felt bliver logget (`fail`) og ikke gemt.

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

Neon-strengen skal være i formatet `Host=...;` – Npgsql forstår ikke `postgresql://`.

**Bemærk:** Når backenden også kører på serveren, gemmes hver besked to gange, hvis du kører den lokalt samtidig.

## Deploy

[deploy.mags.dk](https://deploy.mags.dk) (Dokploy, fælles bruger fra Notion, dag 16) → projekt **Bornemonitor** → service **backend** (Compose).
Direkte link (kræver login): [backend i Dokploy](https://deploy.mags.dk/dashboard/project/CLiaE6P1JNfa1rGIFAKO9/environment/H0Hz_VntjnQ520C4BkHq_/services/compose/6C54WEEf06svCZdPtPzc0). Fanen **Logs** viser, hvad backenden laver lige nu.

- Kilde: dette repo, branch `dev`, fil [`compose.yaml`](../compose.yaml). Dokploy bygger image ud fra [`Dockerfile`](../backend/Bornemonitor.Api/Dockerfile).
- Hemmeligheder i fanen *Environment*: `MQTT_HOST`, `MQTT_USERNAME`, `MQTT_PASSWORD`, `NEON_CONNECTION`.
- Ny version: push til `dev` → *Deploy* i Dokploy.
- Port `8080`. Domænet fra Dokploy (sslip.io, `185.22.75.2`) kan ikke nås udefra – vi har spurgt om et subdomæne under `mags.dk`.

To begrænsninger på serveren, som `compose.yaml` tager højde for:

1. Docker kan ikke oprette nye netværk → servicen bruger det eksisterende `dokploy-network`.
2. Ingen ledige inotify-instanser → `DOTNET_hostBuilder__reloadConfigOnChange: "false"`.

Derfor virker `docker compose up` ikke lokalt (netværket findes kun på serveren). Brug `dotnet run`.
