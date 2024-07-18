# p1_manipulation
Reads data from a P1 port and manipulates the data so that a charging station can charge a car on solar panels


Stappen om het programma te gebruiken:
Installeer de Boost library:

Onder Linux kun je dit doen met sudo apt-get install libboost-all-dev.
Voor Windows kun je de pre-built binaries downloaden van de Boost-website.
Compileer het programma:

Onder Linux:
sh
Code kopiëren
g++ -o serial_read serial_read.cpp -lboost_system -lboost_serialization
Onder Windows (bij gebruik van MinGW bijvoorbeeld):
sh
Code kopiëren
g++ -o serial_read.exe serial_read.cpp -lboost_system -lboost_serialization
Voer het programma uit:

Zorg ervoor dat je de juiste seriële poort instelt in het programma (COM3 of /dev/ttyUSB0 bijvoorbeeld).
Start het programma:
sh
Code kopiëren
./serial_read
of onder Windows:
sh
Code kopiëren
serial_read.exe
Opmerkingen:
Het programma leest doorlopend data van de seriële poort en toont deze op het scherm totdat er een fout optreedt.
Pas de seriële poort-instellingen (baudrate, parity, stopbits, etc.) aan op basis van de configuratie van jouw seriële apparaat.
Voor het debuggen en testen kun je gebruik maken van een seriële emulator of tools zoals minicom onder Linux of PuTTY onder Windows.
