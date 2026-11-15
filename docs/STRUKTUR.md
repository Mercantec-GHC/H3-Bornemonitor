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

MQTT-topics: `bornemonitor/{familieId}/barn/maaling`, `…/barn/status`, `…/foraelder/haendelse`, `…/alarm`, `…/indstillinger` (backend → forældreenheden, retained).

## MVP (release 6, 4/12)

- Målinger → backend → database.
- Advarsel ved høj temperatur/fugt.
- Alarm ved mistet forbindelse.
- Tilstande i dagbogen.
- JWT-login.

**Nice to have:** PDF-statistik, billeder, flere forældreenheder, påmindelser.
