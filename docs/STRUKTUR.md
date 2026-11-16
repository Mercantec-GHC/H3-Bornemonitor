# Børnemonitor – struktur og funktionalitet

*Forslag fra Andrii. [Projektbeskrivelsen](PROJEKTBESKRIVELSE.md) siger **hvad**, denne fil **hvordan**.*

## Beslutninger

| # | Spørgsmål | Status |
|---|---|---|
| 1 | MQTT eller HTTP? | ❓ Åben |
| 2 | Hvem tjekker grænseværdierne? | ✅ Backend (sættes på hjemmesiden) |
| 3 | Menu / forældrekode på børneenheden? | ✅ Nej, den sender kun data |
| 4 | MVP og nice to have? | ❓ Åben – forslag nedenfor |

## Enhederne

- **Børneenhed** – sender temperatur, fugt og lys hvert 10.–30. sekund. Ingen menu, modtager intet (interval i koden).
- **Forældreenhed** – viser måling og aktiv tilstand. Knapper: søvn, gåtur, aktivitet, madning, ble-skift → dagbog. Lydalarm ved advarsel og når børneenheden mister forbindelsen.
- **Backend (C#, Docker)** – gemmer i Neon, tjekker grænser, sender advarsler og indstillinger til forældreenheden, API + JWT.
- **Hjemmeside** – målinger, grafer, dagbog, indstillinger.

Tiden hentes via NTP med `WiFi.getTime()` *(skal afprøves)*.

## MQTT eller HTTP?

Få beskeder, så mængden kræver ikke MQTT. Forskellen er advarsler til forældreenheden:

| | HTTP | MQTT |
|---|---|---|
| Advarsel | Polling hvert 3.–5. sek. | Med det samme |
| "Børneenhed væk" | Backend tjekker | Indbygget (*last will*) |
| Kompleksitet | Lavere | Højere (broker) |
| Skolens netværk | Port 443, altid åben | Port 1883 – skal testes |

Forslag: test om Oplà kan forbinde til brokeren fra skolen. Virker det – MQTT, ellers HTTP.

### Dataflow (hvis MQTT)

Alle topics starter med `bornemonitor/{familieId}/`. Man abonnerer på et topic hos brokeren, ikke på en enhed.

| Hvem | Publicerer | Abonnerer på |
|---|---|---|
| Børneenhed | `barn/maaling`, `barn/status` (last will) | – |
| Forældreenhed | `foraelder/haendelse` | `barn/maaling`, `barn/status`, `alarm`, `indstillinger` |
| Backend | `alarm`, `indstillinger` (retained) | `barn/maaling`, `barn/status`, `foraelder/haendelse` |
| Hjemmeside | – | – (bruger backendens API) |

```
Børneenhed ──måling──► broker ──► Backend: gem alt, tjek grænser
                              └──► Forældreenhed: vis tallet
Backend ──alarm/indstillinger──► broker ──► Forældreenhed: bip / opdater
Forældreenhed ──søvn, gåtur…──► broker ──► Backend: gem i dagbog
Hjemmeside ◄── API + JWT ──► Backend
```

- Backend gemmer **alle** målinger (til grafer). Grænsen afgør kun, om der også sendes alarm.
- Alarm sendes én gang, når grænsen overskrides, og "ok", når værdien er normal igen – ikke ved hver måling.
- "Ingen forbindelse" sendes af brokeren (last will), så den virker, selv om backend er nede.

## MVP – 4. december

- Målinger → backend → database.
- Advarsel ved høj temperatur/fugt.
- Alarm ved mistet forbindelse.
- Tilstande i dagbogen.
- JWT-login.

**Nice to have:** PDF-statistik, billeder, flere forældreenheder, påmindelser.
