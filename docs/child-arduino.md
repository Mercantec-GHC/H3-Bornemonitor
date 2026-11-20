# Funktioner i barnets Arduino

1. **Afsendelse af data via MQTT-broker til backend:**

   - `child/telemetry` – automatisk afsendelse af telemetri hvert 20. sekund;
   - `child/event/duration` – øjeblikkelig afsendelse af beskeder om start (`start`) og afslutning (`stop`) af længerevarende hændelser;
   - `child/event/instant` – afsendelse af en enkelt besked for hver enkeltstående hændelse.

2. Måling af temperatur i grader Celsius (`temperature_c`).

3. Måling af luftfugtighed i procent (`humidity_pct`).

4. Måling af lysniveau (`light_level`):

   - `very_low`
   - `low`
   - `medium`
   - `high`
   - `very_high`

5. **Visning af aktuelle værdier på displayet:**

   - Når TOUCH1 holdes nede, vises den aktuelle temperatur, luftfugtighed og lysniveau på displayet.

6. **Start af hændelser:**

   - Når TOUCH0 holdes nede, vises en liste over mulige hændelser og de tilhørende knapper på displayet:

     ```text
     Sleep - 1
     Walk - 2
     Feeding - 3
     Diaper changed - 4
     ```

   - For at starte en hændelse skal man trykke på den tilsvarende knap uden at slippe TOUCH0 (for at undgå utilsigtede tryk).

   - Displayet viser den tilsvarende besked:

     ```text
     sleep start
     walk start
     feeding done
     Diaper changed
     ```

   - `eat` (feeding) og `diaper` er enkeltstående hændelser. Beskeden sendes én gang til topic `child/event/instant`. JSON indeholder:
     - `event_id` – et unikt ID for hændelsen: Unix-tid i sekunder for tryk på knappen (heltal, `WiFi.getTime()`);
     - `type` – `eat` eller `diaper`;
     - `timestamp` – tidspunktet for hændelsen.

   - `sleep` og `walk` er længerevarende hændelser. Beskederne sendes til topic `child/event/duration`. JSON indeholder:
     - `event_id` – Unix-tid i sekunder for tryk på start-knappen (heltal), som er det samme ved start og afslutning;
     - `type` – `sleep` eller `walk`;
     - `action` – `start` eller `stop`;
     - `timestamp` – tidspunktet for hændelsens start eller afslutning.

7. **Afslutning af længerevarende hændelser:**

   - For at afslutte en aktiv hændelse (`sleep` eller `walk`) skal man holde TOUCH0 nede og trykke på knappen for den tilsvarende hændelse.
   - Arduinoen sender en besked med `"action": "stop"` til topic `child/event/duration`.
   - Ved afslutningen bruges det samme `event_id` som ved starten, men med et nyt `timestamp`.
   - Displayet viser:

     ```text
     sleep stop
     walk stop
     ```

8. **Registrering af tidspunkter for hændelser:**

   - Arduinoen bruger det indbyggede RTC (Real-Time Clock).
   - Efter forbindelse til Wi-Fi synkroniseres tiden via NTP.
   - Hver telemetribesked og hver hændelse indeholder et `timestamp` i UTC-format (ISO 8601).
   - Tidspunktet registreres af Arduinoen, når målingen foretages, eller når der trykkes på knappen – ikke når backend modtager beskeden.
   - For længerevarende hændelser gemmer Arduinoen `event_id` for den aktive hændelse, indtil den afsluttes.

9. **Håndtering af mistet forbindelse:**

   - Hvis Arduinoen midlertidigt mister Wi-Fi-forbindelsen eller forbindelsen til MQTT-brokeren, forsøger den automatisk at oprette forbindelse igen.
   - Mens forbindelsen er afbrudt, fortsætter RTC med at holde styr på tiden, så længe Arduinoen har strøm.
   - Hændelser, som ikke kunne sendes, gemmes midlertidigt i en kø.
   - Når forbindelsen er genoprettet, sender Arduinoen de gemte hændelser med deres oprindelige `timestamp`.
   - Backend overvåger status (`online/offline`) ud fra den modtagne telemetri. Arduinoen sender ikke separate statusbeskeder.
