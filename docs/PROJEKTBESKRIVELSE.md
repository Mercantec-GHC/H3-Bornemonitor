# Børnemonitor – projektbeskrivelse

*Skrevet af Tetiana.*

Projektet omfatter et system med to forbundne Arduino-enheder: en børneenhed og en forældreenhed.

Børneenheden måler temperatur, luftfugtighed og lysniveau. Funktionaliteten begrænses for at forhindre utilsigtet aktivering. Alternativt kan adgangen til bestemte funktioner beskyttes med en forældrekode.

Forældreenheden skal kunne:

* Aktivere forskellige tilstande: søvn, gåtur, aktivitet og madning.
* Modtage advarsler ved forhøjet temperatur eller luftfugtighed samt påmindelser om madning og søvn.
* Afgive en lydalarm.
* Vise relevante oplysninger på en skærm, eksempelvis varigheden af den valgte tilstand.

Begge enheder skal kunne registrere hændelser i en digital dagbog. Det skal undersøges, hvordan tidsregistreringen kan implementeres på Arduino, og om der er behov for at hente tidsoplysninger via backend.

Via en tilknyttet hjemmeside skal forældrene kunne ændre systemets indstillinger, eksempelvis angive varigheden af en gåtur. Muligheden for at tilføje billeder kan også indgå.

På baggrund af oplysningerne i databasen skal systemet kunne udarbejde enkel statistik, som vises på hjemmesiden. Statistikken skal kunne downloades som en PDF-rapport. Det skal undersøges, hvordan rapporterne kan genereres automatisk, og hvor omfattende implementeringen vil være.

Den tekniske løsning omfatter følgende:

* Login med autentificering og autorisation ved hjælp af JWT.
* En database hos Neon.tech.
* En backend, der kører i Docker.

Det skal desuden afklares, om MQTT skal anvendes til kommunikationen mellem enhederne, og om systemet skal understøtte tilslutning af flere forældreenheder.
