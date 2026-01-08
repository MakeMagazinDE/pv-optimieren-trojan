# Steckbrief Programm Receiver

**Allgemeine Daten:**

- **Name:** Receiver
- **Autor:** Walter Trojan
- **Erstellt:** 1. Quartal 2025
- **Zielsetzung:**
  - Empfängt pro 	Sekunde ein Datenpaket vom Transmitter
  - Kommunikation 	mittels TCP-Server (Port 81)
  - Ausgabe der 	Werte auf einem Display
  - Daten-Chart 	der letzten 2 Stunden
  - Ausgabe der 	Daten auf SD-Karte, eine Datei pro Tag

**Technische Details:**

- ​	**Hardware:** ESP32
- **Bibliotheken:**
  - WiFi 			(Netzwerkverbindung)
  - Arduino	(Basis-Funktionen)
  - Wire		(Verdrahtung)
  - floatToString	(Konvertierung)
  - monitor_printf	(Anzeige)
  - SPI		(Kommunikation 	mit Display)
  - FS		(Datei-System)
  - SD		(SD-Karte)
  - LovyanGFX	(Grafik-Treiber)
  - LGFX_AUTODETECT 	(Grafik-Zusatz)
  - Stdio		(Ein/Ausgabe)
- **Pinbelegung:**
  - SD:		CS=GPIO5
  - SPI: 			Clock=GPIO18, Miso=GPIO19, Mosi=GPIO23
  - Display:	DC=GPIO25, 	Reset=GPIO26

**Ablauf:**

**Setup**

- Init WLAN, TCP-Server
- Verbindung mit TCP-Server
- Definition Datenstruktur
- Init SD-Karte
- Init Grafik-Display, zeichnet Überschriften und Raster

**Loop**

- Empfängt Zeit, Stromverbrauch und Zählerstand vom TCP-Server
- Zeigt diese Werte auf dem Display
- Berechnet Mittelwert Stromverbrauch pro Minute
- Zeichnet Minuten-Verbrauch grün, rot oder blau
- Rolliert Display, wenn voll  
- Sammelt Minutenwerte von einer Stunde
- Schreibt alle Werte einer Stunde auf die SD-Karte
- Zurück auf Loop-Anfang (Pro Sekunde ein Durchlauf)



# Steckbrief Programm Transmitter

**Allgemeine Daten:**

- **Name:** Transmitter
- **Autor:** Walter Trojan
- **Erstellt:** 1. Quartal 2025
- **Zielsetzung:**
  - Auswertung 	eines digitalen Stromzählers
  - Erfassung von 	Stromverbrauch und Gesamtzählerstand
  - Empfang von 	Datenpaketen (1x pro Sekunde)
  - Weitergabe der 	Daten über:
    - **TCP-Server** 		(Port 81)
    - **Webserver** 		 (Port 80)

**Technische Details:**

- ​	**Hardware:** ESP32
- **Bibliotheken:**
  - ESP32Time 		(RealTimeClock)
  - WiFi 			(Netzwerkverbindung)
  - time.h 			(Zeithandling)
  - WebServer 		(Webinterface)
- **Pinbelegung:**
  - UART2: 		RX = GPIO16, TX = GPIO17
  - LED Grün 	(GPIO12): WLAN ok
  - LED Gelb 	(GPIO13): Empfang eines Datenblocks



**Ablauf:**

**Setup**

- Init WLAN, TCP- und Web-Server
- LED Grün an
- Internet-Zugriff auf NTP-Zeitserver und Stellen der RTClock



**Loop**

- Wartet auf Datenempfang über RX-Eingang
- Liest einen kompletten Block vom Zähler und prüft auf Gültigkeit
- Während des Empfangs, LED Gelb an
- Extrahiert aktuelle Leistung und Gesamtzählerstand
- Erzeugt einen Zeitstempel mittels RTClock
- Speichert Werte in eine Datenstruktur
- Datenweitergabe

- TCP-Server (Port 81): Sendet die Daten an verbundene Clients
- Web-Server (Port 80): Stellt die Daten im Browser dar
- Druckt Informationen auf der Konsole

- Zurück auf Loop-Anfang (Pro Sekunde ein Durchlauf)