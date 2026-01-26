# ESP32 Ventiili Monitooringu Süsteem

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Python](https://img.shields.io/badge/python-3.10%2B-blue.svg)
![Status](https://img.shields.io/badge/status-production-success.svg)

IoT süsteem ventiilide ja rõhu monitooringuks, kasutades ESP32 mikrokontrollerit, MQTT protokolli ja InfluxDB andmebaasi.

## 📋 Sisukord

- [Ülevaade](#ülevaade)
- [Funktsioonid](#funktsioonid)
- [Arhitektuur](#arhitektuur)
- [Nõuded](#nõuded)
- [Paigaldamine](#paigaldamine)
- [Kasutamine](#kasutamine)
- [API Dokumentatsioon](#api-dokumentatsioon)
- [Turvalisus](#turvalisus)
- [Probleemide lahendamine](#probleemide-lahendamine)
- [Panustamine](#panustamine)
- [Litsents](#litsents)

---

## Ülevaade

See projekt võimaldab jälgida pneumaatiliste ventiilide seisundit ja rõhku reaalajas. Süsteem kasutab M5Atom ESP32 seadmeid andmete kogumiseks, mis saadetakse turvaliselt VPN tunneli kaudu keskserverisse, kus need salvestatakse ja visualiseeritakse veebiliideses.

### Põhikomponendid

```
ESP32 (M5Atom) → WiFi → OpenVPN → Digital Ocean
                                        ↓
                                   MQTT Broker
                                        ↓
                                  Flask Server
                                        ↓
                                    InfluxDB
                                        ↓
                                 Apache2 + HTTPS
                                        ↓
                                   Veebibrauser
```

---

## Funktsioonid

### 🎯 ESP32 Seade

- ✅ Rõhu mõõtmine (MPX5700AP sensor, 0-7 bar)
- ✅ Ventiili seisundi jälgimine (avatud/suletud)
- ✅ Andmete puhverdamine (kuni 500 mõõtmist)
- ✅ Partii-põhine MQTT saatmine (50 kirjet korraga)
- ✅ NTP ajasünkroniseerimine
- ✅ VPN turvalisus (OpenVPN)
- ✅ LED oleku indikaator
- ✅ Serial konsooli juhtimine

### 🖥️ Serveri Pool

- ✅ Reaalajas MQTT andmete töötlemine
- ✅ Time-series andmebaas (InfluxDB)
- ✅ Veebi dashboard seadmete haldamiseks
- ✅ HTTPS turvalisus (Let's Encrypt)
- ✅ Automaatne sertifikaatide uuendamine
- ✅ Seadmete staatuse jälgimine
- ✅ Ajaloolised graafikud

### 📊 Andmete Visualiseerimine

- 📈 Reaalajas rõhugraafikud
- 🎚️ Ventiili seisundi jälgimine
- 📉 Ajalooline andmevaade
- 📊 Statistika ja kokkuvõtted
- ⚡ Kiire andmete uuendus (1 Hz)

---

## Arhitektuur

### Võrgutopoloogia

```
┌─────────────────┐
│  Kodune Võrk    │
│  192.168.1.0/24 │
│                 │
│  ┌──────────┐   │
│  │ M5Atom#1 │   │──┐
│  │10.8.0.2  │   │  │
│  └──────────┘   │  │  OpenVPN Tunnel
│                 │  │  (40094/UDP)
│  ┌──────────┐   │  │
│  │ M5Atom#2 │   │──┤
│  │10.8.0.3  │   │  │
│  └──────────┘   │  │
│                 │  │
│  ┌──────────┐   │  │
│  │  Ruuter  │───┼──┘
│  │192.168.1.1│  │
│  └──────────┘   │
└─────────────────┘
         ↓
    Internet
         ↓
┌─────────────────────────────┐
│  Digital Ocean Droplet      │
│  164.90.x.x                 │
│  ┌────────────────────────┐ │
│  │ OpenVPN Server         │ │
│  │ 10.8.0.1 (tun0)        │ │
│  └────────────────────────┘ │
│            ↓                │
│  ┌────────────────────────┐ │
│  │ MQTT Broker            │ │
│  │ Port: 1883 (VPN only)  │ │
│  └────────────────────────┘ │
│            ↓                │
│  ┌────────────────────────┐ │
│  │ Flask Application      │ │
│  │ Port: 5000 (localhost) │ │
│  └────────────────────────┘ │
│            ↓                │
│  ┌────────────────────────┐ │
│  │ InfluxDB               │ │
│  │ Port: 8086 (localhost) │ │
│  └────────────────────────┘ │
│            ↓                │
│  ┌────────────────────────┐ │
│  │ Apache2 + SSL          │ │
│  │ Port: 80, 443          │ │
│  └────────────────────────┘ │
└─────────────────────────────┘
         ↓
    HTTPS (443)
         ↓
   Kasutaja brauser
```

### Autentimise Kihid

| Kiht | Meetod | Kirjeldus |
|------|--------|-----------|
| 1 | OpenVPN | Sertifikaadi-põhine autentimine |
| 2 | MQTT | Kasutajanimi/parool |
| 3 | Flask Web | Sessiooni-põhine auth |
| 4 | InfluxDB | HTTP Basic Auth |

---

## Nõuded

### Riistvaralised Nõuded

#### ESP32 Seade
- M5Atom Matrix/Lite
- MPX5700AP rõhusensor (0-700 kPa)
- WiFi ühendus
- USB-C kaabel programmeerimiseks

#### Server
- Digital Ocean Droplet (min 1GB RAM, soovitatav 2GB)
- Ubuntu 22.04 LTS
- 20GB disk space
- Staatiline IP aadress
- Domeeninimi (HTTPS jaoks)

### Tarkvaralised Nõuded

#### ESP32 Arendus
```
Arduino IDE 2.x
ESP32 Arduino Core 3.3.4+

Teegid:
- M5Atom (M5Stack)
- PubSubClient (Nick O'Leary)
- NTPClient (Fabrice Weinberg)
- CircularBuffer (AgileWare)
```

#### Server
```
Ubuntu 22.04 LTS
Python 3.10+
Apache2 2.4+
InfluxDB 1.8+
Mosquitto 2.0+
OpenVPN 2.5+
Certbot (Let's Encrypt)

Python teegid:
- Flask
- paho-mqtt
- influxdb
```

---

## Paigaldamine

### 1️⃣ ESP32 Seadistamine

#### 1.1 Arduino IDE Setup
```bash
# Lisa ESP32 board URL:
# File → Preferences → Additional Board Manager URLs
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Paigalda ESP32 board:
# Tools → Board → Boards Manager → "esp32" → Install

# Paigalda teegid:
# Tools → Manage Libraries → Otsi ja paigalda:
# - M5Atom
# - PubSubClient
# - NTPClient
# - CircularBuffer
```

#### 1.2 Koodi Üleslaadimine
```bash
# Ava Labor3.ino Arduino IDE-s
# Vali board: Tools → Board → ESP32 Arduino → M5Atom

# Kompileeri ja laadi üles
```

#### 1.3 Esmane Seadistamine (Serial Monitor)
```
Ava Serial Monitor (115200 baud)

Käsud:
help                                    # Käskude nimekiri
wifi YourSSID;YourPassword             # WiFi seadistus
mqtt 10.8.0.1 1883 mqtt_user KAPOTeam! # MQTT seadistus
ntp                                    # NTP sünkroniseerimine
status                                 # Kontrolli olekut
```

---

### 2️⃣ Serveri Seadistamine

#### 2.1 Esialgne Serveri Setup
```bash
# Ühenda serveriga
ssh root@your_server_ip

# Uuenda süsteem
apt update && apt upgrade -y

# Paigalda põhitarkvara
apt install -y vim git ufw curl wget net-tools

# Seadista firewall
ufw allow 22/tcp
ufw allow OpenSSH
ufw enable
```

#### 2.2 OpenVPN Paigaldamine
```bash
# Lae alla paigaldus skript
wget https://git.io/vpn -O openvpn-install.sh
chmod +x openvpn-install.sh

# Käivita paigaldus
./openvpn-install.sh

# Vastused:
# Port: 40094
# Protocol: UDP
# DNS: 1.1.1.1
# Client name: m5atom1

# Ava port firewall'is
ufw allow 40094/udp
```

#### 2.3 MQTT Broker (Mosquitto)
```bash
# Paigalda Mosquitto
apt install -y mosquitto mosquitto-clients

# Loo kasutaja
mosquitto_passwd -c /etc/mosquitto/passwd mqtt_user
# Parool: KAPOTeam!

# Seadista konfiguratsioon
cat > /etc/mosquitto/conf.d/default.conf << EOF
listener 1883 10.8.0.1
allow_anonymous false
password_file /etc/mosquitto/passwd
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
EOF

# Luba MQTT ainult VPN kaudu
ufw allow in on tun0 to any port 1883 proto tcp

# Taaskäivita
systemctl restart mosquitto
```

#### 2.4 InfluxDB
```bash
# Lisa repositoorium
wget -q https://repos.influxdata.com/influxdata-archive_compat.key
echo '393e8779c89ac8d958f81f942f9ad7fb82a25e133faddaf92e15b16e6ac9ce4c influxdata-archive_compat.key' | sha256sum -c
cat influxdata-archive_compat.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg > /dev/null
echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg] https://repos.influxdata.com/debian stable main' | sudo tee /etc/apt/sources.list.d/influxdata.list

# Paigalda
apt update
apt install -y influxdb

# Käivita
systemctl enable influxdb
systemctl start influxdb

# Loo andmebaas
influx << EOF
CREATE DATABASE esp32valve
CREATE USER admin WITH PASSWORD 'KAPOTeam!' WITH ALL PRIVILEGES
GRANT ALL ON esp32valve TO admin
EXIT
EOF

# Luba autentimine
sed -i 's/# auth-enabled = false/auth-enabled = true/' /etc/influxdb/influxdb.conf
sed -i 's/bind-address = ":8086"/bind-address = "127.0.0.1:8086"/' /etc/influxdb/influxdb.conf

systemctl restart influxdb
```

#### 2.5 Flask + Apache2
```bash
# Paigalda Apache ja Python
apt install -y apache2 libapache2-mod-wsgi-py3 python3 python3-pip

# Paigalda Python teegid
pip3 install flask paho-mqtt influxdb

# Loo projekti kaust
mkdir -p /var/www/webApp
cd /var/www/webApp

# Kopeeri flask+wsgi.py ja webApp.wsgi failid siia

# Määra õigused
chown -R www-data:www-data /var/www/webApp
chmod -R 755 /var/www/webApp

# Seadista Apache
cat > /etc/apache2/sites-available/webApp.conf << EOF
<VirtualHost *:80>
    ServerName yourdomain.com

    WSGIDaemonProcess flaskapp user=www-data group=www-data threads=5
    WSGIProcessGroup flaskapp
    WSGIScriptAlias / /var/www/webApp/webApp.wsgi

    <Directory /var/www/webApp>
        WSGIApplicationGroup %{GLOBAL}
        Require all granted
    </Directory>

    ErrorLog \${APACHE_LOG_DIR}/flask_error.log
    CustomLog \${APACHE_LOG_DIR}/flask_access.log combined
</VirtualHost>
EOF

# Aktiveeri sait
a2ensite webApp.conf
a2dissite 000-default.conf
a2enmod wsgi

# Kontrolli konfiguratsiooni
apache2ctl configtest

# Taaskäivita
systemctl restart apache2
```

#### 2.6 HTTPS (Let's Encrypt)
```bash
# Paigalda Certbot
apt install -y certbot python3-certbot-apache

# Ava pordid
ufw allow 80/tcp
ufw allow 443/tcp

# Hangi sertifikaat
certbot --apache -d yourdomain.com

# Vastused:
# Email: your@email.com
# Agree to ToS: Y
# Redirect HTTP to HTTPS: 2 (Yes)

# Kontrolli automaatset uuendamist
certbot renew --dry-run
```

---

## Kasutamine

### ESP32 Serial Käsud

| Käsk | Kirjeldus | Näide |
|------|-----------|-------|
| `help` | Näita kõiki käske | - |
| `wifi <ssid>;<pass>` | Seadista WiFi | `wifi MyNetwork;MyPassword123` |
| `mqtt <srv> <port> <user> <pass>` | MQTT konfiguratsioon | `mqtt 10.8.0.1 1883 mqtt_user KAPOTeam!` |
| `status` | Näita seadme olekut | - |
| `stats` | Puhvri statistika | - |
| `ntp` | NTP sünkroniseerimine | - |
| `test on` | Lülita testimine sisse | - |
| `test off` | Lülita testimine välja | - |
| `test 2/5/8` | Testi konkreetset mahtu | `test 5` |
| `start` | Alusta MQTT saatmist | - |
| `stop` | Peata MQTT saatmine | - |
| `open` | Ava ventiil (manuaalne) | - |
| `close` | Sulge ventiil (manuaalne) | - |
| `sampling <p> <t>` | Määra sämplimise parameetrid | `sampling 0.1 500` |

### LED Staatuse Indikaatorid

| Värv | Tähendus |
|------|----------|
| 🔵 Sinine | AP režiim (konfigureerimata) |
| 🟡 Kollane | WiFi ühendatud, MQTT pole |
| 🟣 Lilla (vilkuv) | MQTT ühendatud, saatmine pausil |
| 🟢 Roheline | Ventiil avatud, saatmine aktiivne |
| 🔴 Punane | Ventiil suletud, saatmine aktiivne |
| 🟠 Oranž | Puhver täis! |

### Veebi Liides

#### Avaleht
```
https://yourdomain.com/
```
- Näitab kõiki seadmeid
- Staatused: Online, Recently Active, Offline
- Viimane ühenduse aeg
- Statistika (batches, records)

#### API Endpoints

| Endpoint | Meetod | Kirjeldus |
|----------|--------|-----------|
| `/` | GET | Seadmete nimekiri |
| `/device/<uid>/history` | GET | Viimased 1000 kirjet |
| `/device/<uid>/stats` | GET | Seadme statistika |
| `/device/<uid>/realtime` | GET | Viimased 100 punkti graafikule |
| `/device/<uid>/last` | GET | Viimane kirje + vanus |
| `/stats/all` | GET | Kõigi seadmete kokkuvõte |

#### Näide API päring
```bash
# Viimane kirje
curl https://yourdomain.com/device/509A080B65F4/last

# Vastus:
{
  "device": "509A080B65F4",
  "last_data": {
    "time": "2024-12-16T15:30:45.123Z",
    "pressure_now": 5.23,
    "valve_state": "open"
  },
  "age_seconds": 12.5,
  "age_minutes": 0.21,
  "status": "recent"
}
```

---

## API Dokumentatsioon

### MQTT Topics

#### Data Topic
```
sensors/<device_uid>/data
```

**Payload formaat (batch):**
```json
{
  "uid": "509A080B65F4",
  "count": 50,
  "data": [
    {
      "t": 1734361845123,
      "v": 1,
      "p": 5.23
    },
    ...
  ]
}
```

| Väli | Tüüp | Kirjeldus |
|------|------|-----------|
| `t` | number | Unix timestamp (millisekundites) |
| `v` | number | Ventiil: 1=avatud, 0=suletud |
| `p` | float | Rõhk (baarides) |

#### Init Topic
```
sensors/<device_uid>/init
```

HTML fragment seadme kirjeldusega (retained message).

### InfluxDB Schema

**Measurement:** `device_data`

**Tags:**
- `device` - Seadme UID

**Fields:**
- `pressure_now` (float) - Praegune rõhk baarides
- `pressure_prev` (float) - Rõhk 30ms tagasi (deprecated)
- `valve_state` (string) - "open" või "closed"

**Näide query:**
```sql
SELECT * FROM device_data 
WHERE device='509A080B65F4' 
  AND time > now() - 1h
ORDER BY time DESC
```

---

## Turvalisus

### Kaitsetasemed

#### 1. OpenVPN Krüpteerimine
- ✅ AES-256-CBC šifreerimine
- ✅ Sertifikaadi-põhine autentimine
- ✅ TLS kontroll kanal
- ✅ Perfect Forward Secrecy (PFS)

#### 2. MQTT Turvalisus
- ✅ Kasutajanimi/parool autentimine
- ✅ Juurdepääs ainult VPN võrgu kaudu (10.8.0.0/24)
- ✅ Port suletud välismaailmale (UFW)

#### 3. Veebiserveri Turvalisus
- ✅ HTTPS (TLS 1.2+)
- ✅ Let's Encrypt sertifikaat (automaatne uuendamine)
- ✅ HTTP → HTTPS redirect
- ✅ Security headers (HSTS, X-Frame-Options, jne)
- ✅ Sessiooni-põhine autentimine

#### 4. Andmebaasi Turvalisus
- ✅ InfluxDB autentimine
- ✅ Juurdepääs ainult localhost'ist
- ✅ HTTP Basic Auth

### Firewall Reeglid (UFW)

```bash
# Lubatud pordid
22/tcp      - SSH
40094/udp   - OpenVPN
80/tcp      - HTTP (redirect)
443/tcp     - HTTPS
1883/tcp    - MQTT (ainult tun0)

# Blokeeritud
Kõik muu
```

### Paroolid ja Kasutajad

**⚠️ TÄHTIS:** Muuda paroolid production keskkonnas!

| Teenus | Kasutaja | Parool (default) |
|--------|----------|------------------|
| Flask Web | admin | 1234 |
| MQTT | mqtt_user | KAPOTeam! |
| InfluxDB | admin | KAPOTeam! |

---

## Probleemide Lahendamine

### ESP32 ei ühenda MQTT-ga

**Kontroll:**
```bash
# ESP32 serial
status  # Kontrolli MQTT: Connected?

# Serveris
systemctl status mosquitto
netstat -tulpn | grep 1883
tail -f /var/log/mosquitto/mosquitto.log
```

**Lahendused:**
1. Kontrolli VPN: `cat /var/log/openvpn/openvpn-status.log`
2. Kontrolli firewall: `ufw status | grep 1883`
3. Taaskäivita Mosquitto: `systemctl restart mosquitto`

---

### Puhver täitub liiga kiiresti

**Kontroll:**
```bash
# ESP32 serial
stats  # Vaata buffer usage
```

**Lahendus:**
```bash
# Muuda sämplimise sagedust
sampling 0.5 5000  # 5 sekundit või 0.5 bar muutus
```

---

### Flask ei näita seadmeid

**Kontroll:**
```bash
# Serveris
cat /var/www/webApp/devices.json
tail -100 /var/log/apache2/flask_error.log
```

**Lahendus:**
```bash
# Puhasta devices.json
echo '{}' > /var/www/webApp/devices.json
chown www-data:www-data /var/www/webApp/devices.json
systemctl restart apache2

# Saada uued andmed ESP32-st
# Serial: start
```

---

### NTP ei sünkroniseeru

**Kontroll:**
```bash
# ESP32 serial
status  # Vaata NTP time
```

**Lahendus:**
```bash
# ESP32 serial
ntp  # Käsitsi sünkroniseerimine

# Kui ei tööta - kontrolli internetti läbi VPN
```

---

## Jõudlus ja Optimiseerimine

### Hetke parameetrid

| Parameeter | Väärtus | Kirjeldus |
|------------|---------|-----------|
| Sämplimise sagedus | 1 Hz | 1 mõõtmine sekundis |
| Rõhu muutuse künnIs | 0.2 bar | Minimaalne erinevus salvestamiseks |
| Puhvri suurus | 500 kirjet | Max ESP32 mälus |
| MQTT batch | 50 kirjet | Kirjeid paketi kohta |
| InfluxDB batch | 100 kirjet | Kirjeid korraga andmebaasi |
| MQTT buffer | 4096 baiti | Max paketi suurus |

### Kirjete arv päevas

```
1 Hz × 60 sek × 60 min × 24 h = 86,400 kirjet/päev
```

### Andmebaasi suurus (hinnang)

```
1 kirje ≈ 100 baiti (InfluxDB)
86,400 kirjet × 100 baiti ≈ 8.6 MB/päev
8.6 MB × 30 päeva ≈ 258 MB/kuu
```

---

## Arendamine ja Panustamine

### Projekti Struktuur

```
esp32-valve-monitoring/
├── README.md                    # See fail
├── README_FULL.md              # Täielik paigaldusjuhend
├── LICENSE                     # MIT litsents
├── docs/                       # Dokumentatsioon
│   ├── architecture.md         # Arhitektuuri kirjeldus
│   ├── api.md                  # API dokumentatsioon
│   └── troubleshooting.md      # Probleemide lahendamine
├── esp32/                      # ESP32 kood
│   ├── Labor3.ino              # Peamine Arduino sketch
│   └── libraries/              # Vajalikud teegid
├── server/                     # Serveri kood
│   ├── flask+wsgi.py           # Flask rakendus
│   ├── webApp.wsgi             # WSGI entry point
│   ├── templates/              # HTML mallid
│   │   └── index.html
│   └── static/                 # CSS, JS
│       ├── css/
│       └── js/
├── configs/                    # Konfiguratsioonid
│   ├── apache2/                # Apache seadistused
│   ├── mosquitto/              # MQTT seadistused
│   └── openvpn/                # VPN seadistused
└── scripts/                    # Tööriistad
    ├── debug_data.py           # Diagnostika skript
    └── backup.sh               # Backup skript
```

### Panustamise Juhend

1. Fork'i repositoorium
2. Loo oma feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit'i muudatused (`git commit -m 'Add some AmazingFeature'`)
4. Push branch'i (`git push origin feature/AmazingFeature`)
5. Ava Pull Request

### Koodistandardid

#### Python
- PEP 8 style guide
- Type hints (Python 3.10+)
- Docstrings kõigile funktsioonidele

#### C++ (ESP32)
- Google C++ Style Guide
- Kommentaarid inglise keeles
- Konstantide kasutamine (const, #define)

---

## Teadaolevad Piirangud

### ESP32
- ❌ WiFi range piiratud (15-30m siseruumides)
- ❌ NTP nõuab internetiühendust
- ❌ Puhver kaob restart'i korral
- ⚠️ ADC täpsus ±2% (rõhu mõõtmisel)

### Server
- ❌ Ühe droplet'i piirangud (CPU, RAM)
- ❌ InfluxDB v1.8 ei toeta clustering
- ⚠️ Apache2 mod_wsgi pole nii kiire kui Gunicorn

### Võrk
- ❌ OpenVPN NAT'i taga võib vajada port forwarding'ut
- ⚠️ VPN reconnect võib võtta kuni 30 sekundit

---

## Tulevikuplaan (Roadmap)

### v2.0 (Q1 2025)
- [ ] Web UI ümberdisain (React)
- [ ] Graafikute täiustamine (Chart.js → Plotly)
- [ ] Email/Telegram teavitused
- [ ] Multi-user support

### v2.1 (Q2 2025)
- [ ] InfluxDB v2.x upgrade
- [ ] Grafana integratsioon
- [ ] Mobile app (React Native)
- [ ] MQTT-over-WebSocket

### v3.0 (Q3 2025)
- [ ] Machine Learning anomaaliate tuvastamiseks
- [ ] Automaatne ventiili juhtimine
- [ ] Cloud backup (AWS S3)
- [ ] Kubernetes deployment

---

## Litsents

See projekt on litsentseeritud MIT litsentsi alusel - vaata [LICENSE](LICENSE) faili detailide jaoks.

---

## Autorid ja Kontakt

**KAPO Team**

- 📧 Email: [info@kapoteam.ee](mailto:info@kapoteam.ee)
- 🌐 Website: [www.kapoteam.ee](https://www.kapoteam.ee)
- 💬 Discord: [KAPO Team Server](https://discord.gg/kapoteam)

### Panustajad

- **Sinu Nimi** - *Esmane arendus* - [@username](https://github.com/username)

Täname kõiki [panustajaid](https://github.com/yourusername/esp32-valve-monitoring/contributors)!

---

## Tänuavaldused

- M5Stack meeskonnale M5Atom riistvara eest
- Eclipse Mosquitto projektile
- InfluxData InfluxDB eest
- Let's Encrypt tasuta SSL sertifikaatide eest
- Arduino ja ESP32 kogukonnale

---

## Versiooniajalugu

### v1.0.0 (2024-12-16)
- ✅ Esmane väljalase
- ✅ ESP32 andmete kogumine
- ✅ MQTT batch saatmine
- ✅ InfluxDB integratsioon
