# Børnemonitor – struktur og funktionalitet

*Forslag fra Andrii. Bygger oven på [projektbeskrivelsen](PROJEKTBESKRIVELSE.md): den beskriver **hvad** systemet skal kunne, denne fil **hvordan** det hænger sammen.*

## Beslutninger

| # | Spørgsmål | Status |
|---|---|---|
| 1 | MQTT eller HTTP mellem enhederne og backend | ❓ Åben – se nedenfor |
| 2 | Hvem tjekker grænseværdierne (temperatur, fugt)? | ✅ Backend. Grænserne sættes på hjemmesiden, og logikken kan unit-testes |
| 3 | Skal børneenheden have menu / forældrekode? | ✅ Nej. Børneenheden sender kun data og har ingen menu, så der er intet at aktivere ved et uheld |
| 4 | Hvad er MVP, og hvad er "nice to have"? | ❓ Åben – forslag nedenfor |

## Enhederne

**Børneenhed (Oplà)** – kun sensorer, ingen menu
- Sender en måling hvert 10.–30. sekund: temperatur, luftfugtighed, lys og tidspunkt.
- Giver besked, når den går online.

**Forældreenhed (Oplà)**
- Viser seneste måling og den aktive tilstand med varighed.
- Knapper: søvn, gåtur, aktivitet, madning, ble-skift → gemmes som hændelse i dagbogen.
- Lydalarm og besked på skærmen ved advarsel.
- **Alarm "ingen forbindelse til børneenheden"**, hvis der ikke er kommet data i X sekunder. For en babyalarm er det vigtigste, at forældrene opdager, når systemet ikke virker.

**Backend (C#, Docker)**
- Modtager målinger og hændelser og gemmer dem i databasen (Neon).
- Tjekker grænseværdier → sender advarsel til forældreenheden og hjemmesiden.
- API til hjemmesiden og JWT-login for forældre.
- Sender indstillinger (grænser, varighed af gåtur) til enhederne.

**Hjemmeside**
- Seneste målinger og grafer, dagbog, indstillinger.
- Senere: statistik som PDF, billeder.

## Tid på Arduino

Enhederne kan hente tiden via NTP med `WiFi.getTime()` fra WiFiNINA, så der er ikke brug for et ur eller backend til det. *Skal afprøves på Oplà.*

## Kommunikation: MQTT eller HTTP?

Børnemonitoren sender langt færre beskeder end spillet (ca. én måling hvert 10.–30. sekund mod 20 pr. sekund). Mængden kræver altså ikke MQTT. Forskellen ligger i, **hvilken vej** beskederne går.

| | HTTP (POST + polling) | MQTT |
|---|---|---|
| Børneenhed → system | ✅ Enkelt: `POST /api/maalinger` | ✅ Enkelt: publish |
| Advarsel → forældreenhed | Forældreenheden spørger hvert 3.–5. sekund (`GET /api/status`) → op til 5 sek. forsinkelse | Kommer med det samme (push) |
| "Børneenheden er væk" | Backend opdager, at der ikke kommer POST → flag i status | Indbygget: MQTT *last will* |
| Hvis backend er nede | Ingen advarsler | Enhederne taler stadig sammen via brokeren |
| Kompleksitet | Lavere: kun backend | Højere: broker, topics, genforbindelse |
| Skolens netværk | HTTPS på port 443 er næsten altid åben | Port 1883 kan være blokeret. *Antagelse – i TamaPal hang begge Oplàer i `FORBINDER` 11/11* |

Begge dele virker. 5 sekunders forsinkelse på en temperaturadvarsel er ikke kritisk.

- **HTTP**, hvis vi vil have det enkelt og sikker forbindelse på skolen.
- **MQTT**, hvis vi vil have advarsler med det samme, uafhængighed af backend og flere forældreenheder næsten gratis.

**Forslag til test, før vi vælger:** få en Oplà til at forbinde til brokeren fra skolens netværk. Bliver den ved med at stå på `FORBINDER`, vælger vi HTTP.

### Hvis vi vælger MQTT – topics

```
bornemonitor/{familieId}/barn/maaling       børneenhed → {temp, fugt, lys, ts}
bornemonitor/{familieId}/barn/status        online / offline (last will)
bornemonitor/{familieId}/foraelder/haendelse forældreenhed → søvn, gåtur, madning …
bornemonitor/{familieId}/alarm              backend → forældreenhed
bornemonitor/{familieId}/indstillinger      backend → enhederne
```

## Forslag: MVP og nice to have

**MVP (til release 6 – "MVP Done", 4/12)**
- Børneenhed sender målinger → backend → database.
- Advarsel ved for høj temperatur eller fugt på forældreenheden og hjemmesiden.
- Alarm, når børneenheden mister forbindelsen.
- Tilstande på forældreenheden, gemt i dagbogen.
- Login med JWT.

**Nice to have**
- Statistik som PDF.
- Billeder.
- Flere forældreenheder.
- Påmindelser om madning og søvn.
