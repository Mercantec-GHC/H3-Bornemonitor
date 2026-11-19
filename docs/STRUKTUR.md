# Børnemonitor – struktur og funktionalitet

*Forslag fra Andrii. [Projektbeskrivelsen](PROJEKTBESKRIVELSE.md) siger **hvad**, denne fil **hvordan**.*

Navne i kode, topics, endpoints og databasen er på engelsk (`child`, `parent`, `measurement` …).

## Beslutninger

| # | Spørgsmål | Status |
|---|---|---|
| 1 | MQTT eller HTTP? | ✅ MQTT (EMQX Cloud) |
| 2 | Hvem tjekker grænseværdierne? | ✅ Backend (sættes på hjemmesiden) |
| 3 | Hvor trykker man på hændelser (søvn, gåtur …)? | ✅ På børneenheden ([Tetiana](Funktioner%20i%20barnets%20Arduino)) |
| 4 | Hvad gør forældreenheden? | ✅ Kun subscriber: viser målinger, hændelser og offline-alarm ([Tetiana](parent-arduino.md)) |
| 5 | Hvem viser advarsel ved høj temperatur/fugt? | ❓ Åben – forslag: `child/alert` fra backend til forældreenheden |
| 6 | MVP og nice to have? | ❓ Åben – forslag nedenfor |

## Enhederne

- **Børneenhed** – kun publisher. Sender temperatur, fugt og lys hvert 20. sekund og hændelser (søvn, gåtur, madning, ble-skift) fra knapperne. Detaljer: [Funktioner i barnets Arduino](Funktioner%20i%20barnets%20Arduino).
- **Forældreenhed** – kun subscriber. Viser målinger, den aktive hændelse med varighed, madning/ble-skift og `CHILD OFFLINE` med lyd og røde LED'er. Detaljer: [parent-arduino.md](parent-arduino.md).
- **Backend (C#, Docker)** – gemmer i Neon, overvåger online/offline og publicerer `child/status`, tjekker grænser, API + JWT.
- **Hjemmeside** – målinger, grafer, dagbog, indstillinger.

Tiden sættes af Arduinoen (RTC, synkroniseret via NTP) og sendes som `timestamp` i UTC.

## Kommunikation: MQTT

**Valgt: MQTT** via EMQX Cloud (Serverless, gratis): `f2ba9612.ala.eu-central-1.emqxsl.com`. Login deles ikke i Git.

- Port `8883` (MQTT over TLS) – Arduino og backend.
- Port `8084`, sti `/mqtt` (WebSocket over TLS) – browser, fx MQTTX web-klient.

### Hvorfor MQTT og ikke HTTP

Få beskeder, så mængden kræver ikke MQTT. Forskellen er advarsler til forældreenheden:

| | HTTP | MQTT |
|---|---|---|
| Advarsel | Polling hvert 3.–5. sek. | Med det samme |
| Kompleksitet | Lavere | Højere (broker) |

### Dataflow

Topics har intet prefix (`child/...`). Man abonnerer på et topic hos brokeren, ikke på en enhed.

| Hvem | Publicerer | Abonnerer på |
|---|---|---|
| Børneenhed | `child/telemetry`, `child/event/duration`, `child/event/instant` | – |
| Forældreenhed | – | `child/telemetry`, `child/event/duration`, `child/event/instant`, `child/status` |
| Backend | `child/status` | `child/telemetry`, `child/event/#` |
| Hjemmeside | – | – (bruger backendens API) |

![MQTT publish / subscribe](mqtt-flow.svg)

- Backend gemmer **alle** målinger (til grafer).
- **Offline:** børneenheden sender ingen status og ingen *last will*. Backend publicerer `child/status` = `offline`, når der ikke er kommet telemetri i 60 sek. (3 × 20 sek.), og `online`, når telemetrien kommer igen – kun ved skift.
- Kun **én længerevarende hændelse ad gangen**: starter man `walk` under `sleep`, sender børneenheden først `sleep stop` og så `walk start`.

### Beskedformat (JSON)

Se [MQTT topics](MQTT%20topics) (Tetiana). Vigtigt for backend:

- `timestamp` kommer fra Arduinoen (UTC) – backend gemmer den, ikke modtagelsestidspunktet.
- Ved mistet forbindelse gemmer Arduinoen hændelser i en kø og sender dem senere med det oprindelige `timestamp` → gamle beskeder er normale.
- `event_id` er det samme ved `start` og `stop` for `sleep`/`walk` → dubletter (QoS 1, kø) genkendes på `event_id`.

Forslag til advarsel ved høj temperatur/fugt (beslutning 5):

| Topic | Eksempel |
|---|---|
| `child/alert` | `{"type": "temperature", "state": "high", "value": 26.1, "timestamp": "..."}` / `"state": "ok"` – `type`: `temperature`, `humidity` |

**Skal afklares med Tetiana:**

- Skal `child/status` være *retained*, så forældreenheden kender status efter genstart? (forslag: ja)
- Type af `event_id`: tal eller tekst?
- Advarsel ved høj temperatur/fugt: `child/alert` til forældreenheden, kun på hjemmesiden, eller ikke i MVP?

## MVP – 4. december

- Målinger → backend → database.
- Advarsel ved høj temperatur/fugt.
- Alarm ved mistet forbindelse.
- Tilstande i dagbogen.
- JWT-login.

**Nice to have:** PDF-statistik, billeder, flere forældreenheder, påmindelser, synkronisering af aktiv hændelse til forældreenheden efter genstart.
