# Forældre-Arduino – funktionel specifikation

> **Formål:** Forældre-Arduinoen viser målinger og hændelser fra barnets Arduino og advarer, når forbindelsen til barnets enhed er afbrudt. Kommunikationen foregår via en MQTT-broker.

## 1. MQTT-kommunikation

Forældre-Arduinoen er **subscriber** og publicerer ikke selv applikationsdata i første version.

| Topic | Publisher | Indhold |
| :--- | :--- | :--- |
| `child/telemetry` | Barnets Arduino | Temperatur, luftfugtighed og lysniveau (hvert 20. sekund) |
| `child/event/instant` | Barnets Arduino | Enkeltstående hændelser: `eat`, `diaper` |
| `child/event/duration` | Barnets Arduino | Start og stop af længerevarende hændelser: `sleep`, `walk` |
| `child/status` | Backend | Forbindelsesstatus for barnets Arduino: `online` / `offline` |

## 2. Normal visning og aktive hændelser

Når forbindelsen til barnets Arduino er aktiv, og ingen længerevarende hændelse er registreret, viser displayet **`ACTIVE`**.

Når en længerevarende hændelse er aktiv, viser displayet:

- Hændelsens navn: `SLEEP` eller `WALK`.
- Starttidspunktet fra hændelsens `timestamp`.
- Den aktuelle varighed, som beregnes lokalt ud fra starttidspunktet og det aktuelle klokkeslæt.

Eksempel:

```text
SLEEP
Started: 13:15
Duration: 01:27
```

**Der kan kun være én aktiv længerevarende hændelse ad gangen.** Hvis `walk` startes, mens `sleep` er aktiv, sender barnets Arduino først `sleep` med `action: stop` og derefter `walk` med `action: start`. Tilsvarende gælder i modsat retning. Begge beskeder bruger hver deres hændelses relevante `event_id`; start og stop for samme hændelse har samme `event_id`.

Når den aktive hændelse afsluttes uden at en ny startes, vender displayet tilbage til `ACTIVE`, så længe barnet er online.

> **Bemærk:** `ACTIVE` betyder, at forældre-Arduinoen ikke har registreret en igangværende hændelse. Det er ikke en garanti for, at ingen hændelse er aktiv, hvis beskeder er blevet overset under en afbrudt forbindelse.

## 3. Visning af telemetri

Når **TOUCH0 holdes nede**, viser displayet de senest modtagne værdier:

- Temperatur i °C (`temperature_c`).
- Luftfugtighed i % (`humidity_pct`).
- Lysniveau (`light_level`: `very_low`, `low`, `medium`, `high`, `very_high`).

Når TOUCH0 slippes, vender displayet tilbage til den aktuelle hovedvisning. Telemetrien opdateres, hver gang der modtages en ny besked fra `child/telemetry` (normalt hvert 20. sekund).

## 4. Enkeltstående hændelser

Når der modtages `eat` eller `diaper` via `child/event/instant`:

- LED'erne lyser **grønt**.
- Displayet viser hændelsens navn og tidspunkt.
- Displayet viser vejledningen `Cover to dismiss`.

Eksempel:

```text
FEEDING
Time: 14:35
Cover to dismiss
```

Notifikationen lukkes enten ved at dække sensoren med hånden i ca. 1 sekund eller automatisk efter **60 sekunder**. Derefter slukkes de grønne LED'er, og den normale visning gendannes.

**Teknisk note:** En ren lysmåling kan give falske registreringer i mørke. Brug så vidt muligt nærhedssensoren eller en kombination af nærhed og lys til at registrere hånden.

## 5. Alarm: Barnets Arduino er offline

Backend overvåger den indgående telemetri og publicerer status på `child/status`.

Eksempel på en offline-besked:

```json
{
  "state": "offline",
  "last_seen": "2026-11-17T14:40:00Z"
}
```

Når denne besked modtages:

- Der afspilles et kort lydsignal.
- LED'erne lyser konstant **rødt**.
- Displayet viser `CHILD OFFLINE`, tidspunktet for seneste kontakt og en vejledning til at slå lysalarmen fra.

```text
CHILD OFFLINE
Last seen: 14:40
TOUCH 1-2-3: MUTE
```

De røde LED'er slukkes ved at trykke **TOUCH1 → TOUCH2 → TOUCH3** i den rækkefølge. Bekræftelsen slukker **kun LED-alarmen**: `CHILD OFFLINE` forbliver på skærmen, så længe barnet er offline.

Når backend publicerer `{"state":"online"}`, fjernes offline-meddelelsen, og forældre-Arduinoen vender tilbage til den senest kendte normale visning. Denne visning kan være forældet, indtil nye hændelser modtages.

## 6. Forbindelsesfejl på forældre-Arduinoen

Hvis **forældre-Arduinoen selv** mister forbindelsen til Wi-Fi eller MQTT-brokeren, vises:

```text
CONNECTION LOST
Reconnecting...
```

Enheden forsøger automatisk at genoprette forbindelsen og abonnere på de fire topics igen. Denne fejl er forskellig fra `CHILD OFFLINE`, som rapporteres af backend.

## 7. Prioritering af skærmmeddelelser

Meddelelser med højere prioritet vises frem for meddelelser med lavere prioritet.

| Prioritet | Meddelelse | Reaktion |
| :---: | :--- | :--- |
| **1** | `CONNECTION LOST` | Lokal forbindelsesfejl; forsøg på genoprettelse |
| **2** | `CHILD OFFLINE` | Kort lydsignal og røde LED'er |
| **3** | `FEEDING` / `DIAPER` | Grønne LED'er; visning i højst 60 sekunder |
| **4** | `SLEEP` / `WALK` | Løbende visning af den aktive hændelse og dens varighed |
| **5** | `ACTIVE` | Standardvisning |

Når en midlertidig notifikation lukkes, gendannes den højest prioriterede aktuelle visning. Hvis flere enkeltstående hændelser modtages tæt på hinanden, bør de behandles i rækkefølge, så en besked ikke uden videre overskriver en anden.

## 8. Tid og timestamps

- Hændelserne indeholder `timestamp` i **UTC (ISO 8601)**.
- Forældre-Arduinoen synkroniserer sit ur, f.eks. via **NTP**, så varigheden af `sleep` og `walk` kan beregnes korrekt.
- Tidspunkter, der vises på displayet, konverteres efter behov fra UTC til lokal tid.
- Ved `CHILD OFFLINE` vises `last_seen` fra backend, ikke tidspunktet for modtagelsen af alarmen.

## 9. Afgrænsning af første version

Der bruges **ikke** et separat `child/state`-topic i første version. Forældre-Arduinoen holder selv styr på aktive hændelser ud fra de `start`- og `stop`-beskeder, den modtager.

Hvis den tilsluttes efter starten af en hændelse, genstarter eller mister MQTT-forbindelsen, kan den mangle oplysninger om en aktiv hændelse. Synkronisering af den aktuelle tilstand via backend kan tilføjes senere efter aftale med underviseren.
