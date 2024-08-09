Hier is een uitgebreide `README.md` voor de C++ applicatie:

---

# Serial Data Monitor and Controller

Deze C++ applicatie leest en verwerkt data van een seriële poort en stuurt aangepaste gegevens door naar een andere seriële poort. De applicatie is specifiek ontworpen voor het monitoren en regelen van energiestromen zoals verbruik en generatie over meerdere fasen, wat nuttig kan zijn in scenario's zoals het beheren van laadstations voor elektrische voertuigen of andere energietoepassingen.

## Inhoudsopgave

- [Installatie](#installatie)
- [Gebruik](#gebruik)
- [Configuratie](#configuratie)
- [Werking](#werking)
- [Afhankelijkheden](#afhankelijkheden)
- [Licentie](#licentie)

## Installatie

### Vereisten

- C++11 of hoger
- Boost Asio library (versie 1.66 of hoger)

### Compilatie

Om de applicatie te compileren, gebruik de volgende commandoregelinstructies:

```bash
g++ -o serial_monitor main.cpp -lboost_system -lboost_serialization -lpthread
```

Zorg ervoor dat je de juiste paden naar de Boost bibliotheek hebt ingesteld als deze niet in je standaard bibliotheekpaden staan.

## Gebruik

Om de applicatie uit te voeren, gebruik:

```bash
./serial_monitor <read_port> <read_baud_rate> <write_port> <write_baud_rate> <read_parity> <read_stop_bits> <read_data_bits> <write_parity> <write_stop_bits> <write_data_bits> <current_limit> <car_charging_three_phases>
```

### Argumenten:

- `<read_port>`: Seriële poort voor het lezen van gegevens (bv. `/dev/ttyUSB0`).
- `<read_baud_rate>`: Baudrate voor de leespoort (bv. `9600`).
- `<write_port>`: Seriële poort voor het schrijven van gegevens (bv. `/dev/ttyUSB1`).
- `<write_baud_rate>`: Baudrate voor de schrijfpoort (bv. `9600`).
- `<read_parity>`: Pariteitsoptie voor de leespoort (`none`, `odd`, `even`).
- `<read_stop_bits>`: Stopbitoptie voor de leespoort (`one`, `onepointfive`, `two`).
- `<read_data_bits>`: Aantal databits voor de leespoort (bv. `8`).
- `<write_parity>`: Pariteitsoptie voor de schrijfpoort (`none`, `odd`, `even`).
- `<write_stop_bits>`: Stopbitoptie voor de schrijfpoort (`one`, `onepointfive`, `two`).
- `<write_data_bits>`: Aantal databits voor de schrijfpoort (bv. `8`).
- `<current_limit>`: Stroomlimiet voor faseregeling (bv. `32.0`).
- `<car_charging_three_phases>`: Boolean waarde (`true` of `false`) om aan te geven of het voertuig met drie fasen laadt.

### Voorbeeld

```bash
./serial_monitor /dev/ttyUSB0 9600 /dev/ttyUSB1 9600 none one 8 none one 8 32.0 true
```

## Configuratie

De applicatie leest gegevens van een seriële poort, verwerkt deze gegevens door gebruik te maken van reguliere expressies, en stuurt aangepaste gegevens door naar een andere seriële poort. Het doel hiervan is om energiestromen te monitoren en te regelen op basis van vooraf ingestelde limieten en configuraties.

### Regular Expressions

De ontvangen data wordt verwerkt met behulp van verschillende reguliere expressies om specifieke velden zoals stroom, verbruik, en generatie te extraheren.

### CRC Berekening

Een CRC16 checksum wordt berekend en toegevoegd aan de aangepaste gegevens voordat deze worden verzonden.

## Werking

1. **Data Lezen**: De applicatie leest asynchroon gegevens van de leespoort.
2. **Data Verwerken**: Gegevens worden geparsed en relevante waarden worden opgeslagen.
3. **Data Zenden**: Aangepaste gegevens, inclusief een CRC16 checksum, worden doorgestuurd naar de schrijfpoort.
4. **Tabel Weergave**: Een overzichtstabel wordt weergegeven in de terminal die de huidige waarden voor stroom, verbruik, en generatie toont.
5. **Timer**: Om de 10 seconden wordt de data bijgewerkt en opnieuw verzonden.

## Afhankelijkheden

- **Boost Asio**: Voor het beheer van asynchrone invoer/uitvoer operaties en timers.
- **C++ Standaardbibliotheek**: Voor data handling, string verwerking, en reguliere expressies.

## Licentie

Dit project is gelicenseerd onder de MIT-licentie. Zie het LICENSE-bestand voor meer informatie.
