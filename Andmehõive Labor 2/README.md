# M5Atom – UR5 rõhu põhine juhtimine (HTTP + WiFi)

See projekt demonstreerib, kuidas kasutada **M5Atom (ESP32)** seadet rõhu mõõtmiseks ning edastada see väärtus **UR5 robotile** HTTP kaudu. Süsteem toetab kahte võrguarhitektuuri:

- 📡 **Otse ruuteri kaudu** (M5Atom WiFi klient)
- 🌉 **Bridge-režiim** (arvuti sillana M5Atomi Access Pointi ja UR5 vahel)

Lisaks võimaldab **M5Atomi nupp** käivitada või peatada roboti liikumise.

---

## Projekti ülesehitus

Projekt koosneb **kolmest failist**:

| Fail | Kirjeldus |
|----|----|
| `sketch_ur5_communication.ino` | M5Atomi programm: rõhu mõõtmine, WiFi, HTTP server, nupp |
| `ur5.script` | UR5 robotiprogramm (URScript) |
| `bridge loop.py` | Python bridge, kui ruuter puudub |

Failid on leitavad:
- samast OneDrive kaustast kui dokument  
- GitHubist  
- või dokumendi lõpust

---

## Süsteemi üldine tööpõhimõte

1. **M5Atom** mõõdab rõhku analoogsisendilt  
2. Rõhk tehakse kättesaadavaks HTTP GET päringu kaudu (`/pressure`)  
3. **UR5 robot** küsib perioodiliselt rõhu väärtust  
4. Rõhk teisendatakse **liigendi nurgaks**  
5. **M5Atomi nupu vajutus** lubab või keelab roboti liikumise  

Kui robot ei ole “käivitatud”, saadab M5Atom väärtuse `-1`.

---

## 1. sketch_ur5_communication.ino (M5Atom)

### Peamised funktsioonid

- 📏 Rõhu mõõtmine ADC kaudu (ESP32)  
- 🌐 HTTP server (`WebServer`, port 80)  
- 📡 WiFi:
  - **Client mode** (ruuteri kaudu)
  - **Access Point mode** (bridge jaoks)
- 🔘 Nupuga roboti käivitamine / peatamine  
- ⚡ Interrupt-põhine nupu lugemine  

---

### HTTP endpoint

GET /pressure


**Vastused:**
- `-1` → robot ei tohi liikuda  
- `>0` → rõhk kPa-des  

---

### Nupu loogika

- Nupp: **GPIO 39**

- Muutuja:

  ```cpp
  volatile bool startUR5 = false;
Nupp töötab interruptiga, et seda loetaks ka siis, kui server on hõivatud

Kasutatakse M5.Btn.wasPressed() topeltkontrolli

---

### WiFi režiimid
**Access Point (bridge kasutamisel):**
```
WiFi.softAP(ssid, password);
IPAddress myIP = WiFi.softAPIP();
```
**Client (ruuteri kasutamisel):**
```
connectToWiFi();
Serial.println(WiFi.localIP());
```
Vajalik variant jäetakse aktiivseks, teine kommenteeritakse välja.

---

### 2. ur5.script (URScript)
**Rõhu lugemine M5Atomilt**

UR5 avab socketi ja saadab HTTP GET päringu:
```
socket_open("192.168.3.68", 80)
request = "GET /pressure HTTP/1.1\r\nHost: 192.168.3.68\r\n\r\n"
socket_send_string(request)
response = socket_read_string(prefix="close", interpret_escape=True, timeout=50)
socket_close()
prefix="close" on vajalik, sest M5Atom saadab nii HTTP headeri kui ka body.
```
**Rõhu teisendamine liigendi nurgaks**

Lineaarne interpolatsioon:
```
def pressure_to_angle(pressure):
    minPressure = 100
    maxPressure = 200
    minAngle = 0
    maxAngle = 80
    degrees = (pressure-minPressure)*(maxAngle-minAngle)/(maxPressure-minPressure)
    return degrees*3.14/180
end
```
Funktsioon tagastab radiaanid, kuna UR5 kasutab neid.

---

### Roboti käitumine
- Kui rõhk < 0
→ robot ei liigu ja ootab nupu vajutust

- Kui rõhk on kehtiv
→ arvutatakse uus nurk ja tehakse movej

---

### 3. bridge loop.py (Python bridge)
Kasutatakse juhul, kui **ruuter puudub**.

**Roll**

- UR5 → arvuti → M5Atom → arvuti → UR5

- Arvuti:

  - WiFi kaudu M5Atomi Access Pointi

  - Ethernetiga UR5 robotiga

**Bridge töövoog**
1. Ootab UR5 ühendust

2. Avab ühenduse M5Atomiga

3. Edastab UR5 GET requesti M5Atomile

4. Saab vastuse

5. Saadab vastuse UR5-le tagasi

6. Sulgeb socketi

Kõik toimub tsüklis while True.

---

### Kokkuvõte
✔ Rõhu mõõtmine M5Atomiga

✔ HTTP suhtlus UR5 robotiga

✔ Nupuga roboti juhtimine

✔ Töötab nii ruuteri kui ka bridge-lahendusega

✔ Modulaarne ja laiendatav süsteem

