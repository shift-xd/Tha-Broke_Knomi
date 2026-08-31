

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h>
#include <ArduinoJson.h>

#define DHT_PIN       4      // DHT11 DATA  -> IO4
#define DHT_TYPE      DHT11

#define MQ135_PIN     34     // MQ135 A0   -> IO34  (ADC1_CH6, input only)
#define RAIN_PIN      35     // Rain sensor-> IO35  (ADC1_CH7, input only)

// I2C — default ESP32 I2C pins
#define I2C_SDA       21
#define I2C_SCL       22

// CCS811 control pins
#define CCS811_WAK_PIN  16   // Active LOW wake
#define CCS811_RST_PIN  17   // Active LOW reset (-1 = not connected)

// ─────────────────────────────────────────────
//  WiFi AP Credentials
// ─────────────────────────────────────────────
const char* AP_SSID     = "weather Gian";
const char* AP_PASSWORD = "12345678";

// ─────────────────────────────────────────────
//  Sensor state
// ─────────────────────────────────────────────
WebServer       server(80);
DHT             dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP280 bmp;
Adafruit_CCS811 ccs;

struct SensorData {
  float    dht_temp   = 0;
  float    dht_hum    = 0;
  float    bmp_temp   = 0;
  float    bmp_pres   = 0;
  float    bmp_alt    = 0;
  uint16_t co2        = 0;
  uint16_t tvoc       = 0;
  int      mq135_raw  = 0;
  int      rain_raw   = 0;
  bool     is_raining = false;
  String   weather    = "Calculating...";
  unsigned long last_update = 0;
} sd;

bool bmp_ok = false;
bool ccs_ok = false;

const unsigned long SENSOR_INTERVAL = 3000; // ms

// ─────────────────────────────────────────────
//  Weather heuristic
// ─────────────────────────────────────────────
String inferWeather() {
  if (sd.is_raining)       return "Raining";
  if (sd.dht_hum   > 85)  return "Thunderstorm Likely";
  if (sd.bmp_pres  < 1005) return "Overcast / Low Pressure";
  if (sd.bmp_pres  < 1013) return "Partly Cloudy";
  if (sd.dht_hum   > 70)  return "Humid, Possible Showers";
  if (sd.dht_temp  > 35)  return "Hot and Sunny";
  if (sd.dht_temp  < 10)  return "Cold, Frost Risk";
  if (sd.co2       > 1500) return "Poor Air Quality";
  return "Clear and Pleasant";
}

String aqLabel(uint16_t co2) {
  if (co2 < 600)  return "Excellent";
  if (co2 < 1000) return "Good";
  if (co2 < 1500) return "Moderate";
  if (co2 < 2000) return "Poor";
  return "Hazardous";
}

// ─────────────────────────────────────────────
//  Read sensors
// ─────────────────────────────────────────────
void readSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) { sd.dht_hum = h; sd.dht_temp = t; }

  if (bmp_ok) {
    sd.bmp_temp = bmp.readTemperature();
    sd.bmp_pres = bmp.readPressure() / 100.0F;
    sd.bmp_alt  = bmp.readAltitude(1013.25);
  }

  if (ccs_ok && ccs.available() && !ccs.readData()) {
    sd.co2  = ccs.geteCO2();
    sd.tvoc = ccs.getTVOC();
  }

  sd.mq135_raw = analogRead(MQ135_PIN);
  sd.rain_raw  = analogRead(RAIN_PIN);
  sd.is_raining = (sd.rain_raw < 1500);
  sd.weather   = inferWeather();
  sd.last_update = millis();
}

// ─────────────────────────────────────────────
//  /api/data — JSON endpoint
// ─────────────────────────────────────────────
void handleApi() {
  StaticJsonDocument<512> doc;
  doc["dht_temp"]   = sd.dht_temp;
  doc["dht_hum"]    = sd.dht_hum;
  doc["bmp_temp"]   = sd.bmp_temp;
  doc["bmp_pres"]   = sd.bmp_pres;
  doc["bmp_alt"]    = sd.bmp_alt;
  doc["co2"]        = sd.co2;
  doc["tvoc"]       = sd.tvoc;
  doc["mq135_raw"]  = sd.mq135_raw;
  doc["rain_raw"]   = sd.rain_raw;
  doc["is_raining"] = sd.is_raining;
  doc["weather"]    = sd.weather;
  doc["aq_label"]   = aqLabel(sd.co2);
  doc["uptime_s"]   = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────
//  / — HTML Dashboard (This below part for website design was ai generated sorry for the use but i dont know how to do this without help im learning ! )
// ─────────────────────────────────────────────
void handleRoot() {
  String html = R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Weather Gian</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=Share+Tech+Mono&display=swap');
:root{--bg:#050d1a;--panel:#091628;--border:#0d2a4a;--accent:#00d4ff;--accent2:#00ff9d;--warn:#ff6b35;--text:#c8e8ff;--dim:#4a7090;--glow:0 0 16px rgba(0,212,255,.45);--glow2:0 0 16px rgba(0,255,157,.35)}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Share Tech Mono',monospace;min-height:100vh;overflow-x:hidden}
body::before{content:'';position:fixed;inset:0;z-index:0;background-image:linear-gradient(rgba(0,212,255,.04)1px,transparent 1px),linear-gradient(90deg,rgba(0,212,255,.04)1px,transparent 1px);background-size:40px 40px;pointer-events:none}
.wrap{position:relative;z-index:1;max-width:960px;margin:0 auto;padding:24px 16px}
header{text-align:center;padding:32px 0 24px;border-bottom:1px solid var(--border);margin-bottom:28px}
header h1{font-family:'Orbitron',sans-serif;font-weight:900;font-size:clamp(1.6rem,5vw,2.8rem);letter-spacing:.12em;color:var(--accent);text-shadow:var(--glow)}
.sub{font-size:.78rem;color:var(--dim);margin-top:6px;letter-spacing:.15em;text-transform:uppercase}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--accent2);box-shadow:var(--glow2);margin-right:6px;animation:pulse 1.8s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.hero{background:linear-gradient(135deg,#091e38 0%,#051528 100%);border:1px solid var(--border);border-radius:12px;padding:28px;text-align:center;margin-bottom:24px;box-shadow:inset 0 0 60px rgba(0,212,255,.04)}
.hero-icon{font-size:3.5rem;margin-bottom:8px}
.hero-label{font-family:'Orbitron',sans-serif;font-size:1.3rem;color:var(--accent);text-shadow:var(--glow);margin-bottom:8px}
.temp-big{font-family:'Orbitron',sans-serif;font-size:clamp(2.5rem,8vw,4rem);font-weight:700;color:#fff;line-height:1}
.temp-big span{font-size:1.4rem;color:var(--dim)}
.rain-badge{display:inline-block;padding:4px 14px;border-radius:99px;font-size:.72rem;letter-spacing:.12em;font-family:'Orbitron',sans-serif;text-transform:uppercase;margin-top:10px}
.rain-yes{background:rgba(0,212,255,.15);color:var(--accent);border:1px solid var(--accent)}
.rain-no{background:rgba(0,255,157,.1);color:var(--accent2);border:1px solid var(--accent2)}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:16px;margin-bottom:24px}
.card{background:var(--panel);border:1px solid var(--border);border-radius:10px;padding:18px 16px;position:relative;overflow:hidden;transition:border-color .3s}
.card:hover{border-color:var(--accent)}
.card::after{content:'';position:absolute;top:0;left:0;right:0;height:2px;background:linear-gradient(90deg,transparent,var(--accent),transparent);opacity:0;transition:opacity .3s}
.card:hover::after{opacity:1}
.card-label{font-size:.65rem;letter-spacing:.18em;text-transform:uppercase;color:var(--dim);margin-bottom:10px}
.card-value{font-family:'Orbitron',sans-serif;font-size:1.7rem;font-weight:700;color:var(--accent);text-shadow:var(--glow);word-break:break-all}
.card-unit{font-size:.85rem;color:var(--dim);margin-left:4px}
.card-icon{font-size:1.4rem;margin-bottom:8px}
.card.green .card-value{color:var(--accent2);text-shadow:var(--glow2)}
.card.warn .card-value{color:var(--warn)}
.bar-bg{background:rgba(255,255,255,.05);border-radius:4px;height:8px;margin-top:10px;overflow:hidden}
.bar-fill{height:100%;border-radius:4px;background:linear-gradient(90deg,var(--accent2),var(--accent));transition:width .8s ease}
footer{text-align:center;padding:20px 0;color:var(--dim);font-size:.7rem;letter-spacing:.1em;border-top:1px solid var(--border)}
#lu{color:var(--accent)} #up{color:var(--accent2)}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>WEATHER GIAN</h1>
    <div class="sub"><span class="dot"></span>Live Station &middot; ESP32 &middot; /ghost</div>
  </header>
  <div class="hero">
    <div class="hero-icon" id="wi">&#8987;</div>
    <div class="hero-label" id="wl">Loading...</div>
    <div class="temp-big" id="wt">--<span>&deg;C</span></div>
    <div id="rb"></div>
  </div>
  <div class="grid">
    <div class="card">
      <div class="card-icon">&#128167;</div>
      <div class="card-label">Humidity (DHT11)</div>
      <div class="card-value" id="hum">--<span class="card-unit">%</span></div>
    </div>
    <div class="card green">
      <div class="card-icon">&#127777;</div>
      <div class="card-label">BMP Temp</div>
      <div class="card-value" id="bt">--<span class="card-unit">&deg;C</span></div>
    </div>
    <div class="card green">
      <div class="card-icon">&#128309;</div>
      <div class="card-label">Pressure</div>
      <div class="card-value" id="pr">--<span class="card-unit">hPa</span></div>
    </div>
    <div class="card">
      <div class="card-icon">&#9968;</div>
      <div class="card-label">Altitude</div>
      <div class="card-value" id="al">--<span class="card-unit">m</span></div>
    </div>
    <div class="card" id="co2c">
      <div class="card-icon">&#129754;</div>
      <div class="card-label">CO2 (CCS811)</div>
      <div class="card-value" id="co2v">--<span class="card-unit">ppm</span></div>
    </div>
    <div class="card">
      <div class="card-icon">&#129514;</div>
      <div class="card-label">TVOC</div>
      <div class="card-value" id="tv">--<span class="card-unit">ppb</span></div>
    </div>
    <div class="card">
      <div class="card-icon">&#127787;</div>
      <div class="card-label">Air Quality</div>
      <div class="card-value" id="aq" style="font-size:1.1rem">--</div>
      <div class="bar-bg"><div class="bar-fill" id="aqb" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="card-icon">&#9879;</div>
      <div class="card-label">MQ135 Raw</div>
      <div class="card-value" id="mq">--</div>
    </div>
  </div>
  <footer>Last update: <span id="lu">--</span> &nbsp;|&nbsp; Uptime: <span id="up">--</span>s &nbsp;|&nbsp; weather Gian &middot; /ghost</footer>
</div>
<script>
async function refresh(){
  try{
    const d=await(await fetch('/api/data')).json();
    document.getElementById('wi').textContent=
      d.weather.includes('Rain')?'&#127783;':
      d.weather.includes('Thunder')?'&#9928;':
      d.weather.includes('Cloudy')?'&#9925;':
      d.weather.includes('Humid')?'&#127784;':
      d.weather.includes('Hot')?'&#9728;':
      d.weather.includes('Cold')?'&#127784;':
      d.weather.includes('Poor')?'&#128168;':'&#9728;';
    document.getElementById('wl').textContent=d.weather;
    document.getElementById('wt').innerHTML=d.dht_temp.toFixed(1)+'<span>&deg;C</span>';
    document.getElementById('rb').innerHTML=d.is_raining
      ?'<span class="rain-badge rain-yes">Rain Detected</span>'
      :'<span class="rain-badge rain-no">No Rain</span>';
    document.getElementById('hum').innerHTML=d.dht_hum.toFixed(1)+'<span class="card-unit">%</span>';
    document.getElementById('bt').innerHTML=d.bmp_temp.toFixed(1)+'<span class="card-unit">&deg;C</span>';
    document.getElementById('pr').innerHTML=d.bmp_pres.toFixed(2)+'<span class="card-unit">hPa</span>';
    document.getElementById('al').innerHTML=d.bmp_alt.toFixed(0)+'<span class="card-unit">m</span>';
    document.getElementById('co2v').innerHTML=d.co2+'<span class="card-unit">ppm</span>';
    document.getElementById('tv').innerHTML=d.tvoc+'<span class="card-unit">ppb</span>';
    document.getElementById('aq').textContent=d.aq_label;
    document.getElementById('mq').textContent=d.mq135_raw;
    document.getElementById('co2c').className='card'+(d.co2>1500?' warn':d.co2>800?'':' green');
    document.getElementById('aqb').style.width=Math.min(100,d.mq135_raw/40.95).toFixed(0)+'%';
    document.getElementById('lu').textContent=new Date().toTimeString().slice(0,8);
    document.getElementById('up').textContent=d.uptime_s;
  }catch(e){document.getElementById('wl').textContent='Sensor error';}
}
refresh();setInterval(refresh,3000);
</script>
</body>
</html>)";

  server.send(200, "text/html", html);
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Weather Gian Boot ===");

  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  Serial.println("[DHT11] init");

  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    bmp_ok = true;
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println("[BMP280] OK");
  } else {
    Serial.println("[BMP280] NOT FOUND");
  }

  pinMode(CCS811_WAK_PIN, OUTPUT);
  digitalWrite(CCS811_WAK_PIN, LOW);
  if (CCS811_RST_PIN >= 0) {
    pinMode(CCS811_RST_PIN, OUTPUT);
    digitalWrite(CCS811_RST_PIN, HIGH);
  }
  delay(70);
  if (ccs.begin()) {
    ccs_ok = true;
    Serial.println("[CCS811] OK");
  } else {
    Serial.println("[CCS811] NOT FOUND");
  }

  pinMode(MQ135_PIN, INPUT);
  pinMode(RAIN_PIN,  INPUT);

  // Start Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[AP] SSID: "); Serial.println(AP_SSID);
  Serial.print("[AP] IP  : "); Serial.println(WiFi.softAPIP());

  server.on("/",         handleRoot);
  server.on("/api/data", handleApi);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();
  Serial.println("[HTTP] Server started");

  delay(2000);
  readSensors();
}

// ─────────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (millis() - sd.last_update >= SENSOR_INTERVAL) {
    readSensors();
    Serial.printf("[DHT]  %.1fC  %.1f%%\n", sd.dht_temp, sd.dht_hum);
    if (bmp_ok) Serial.printf("[BMP]  %.1fC  %.2f hPa  %.0fm\n",
                               sd.bmp_temp, sd.bmp_pres, sd.bmp_alt);
    if (ccs_ok) Serial.printf("[CCS]  CO2=%d  TVOC=%d\n", sd.co2, sd.tvoc);
    Serial.printf("[ANA]  MQ135=%d  Rain=%d  Raining=%s\n",
                  sd.mq135_raw, sd.rain_raw, sd.is_raining ? "YES" : "NO");
    Serial.printf("[WX]   %s\n\n", sd.weather.c_str());
  }
}

Compiled in arduino ide 

