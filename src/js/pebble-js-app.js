/* ── helpers ──────────────────────────────────────────────────── */
function pad2(n) { return n < 10 ? '0' + n : '' + n; }

function epochToHHMM(epoch) {
  var d = new Date(epoch * 1000);
  return pad2(d.getHours()) + ':' + pad2(d.getMinutes());
}

/* WMO weather codes are sent as integers to C side,
 * which draws the corresponding icon.
 * https://open-meteo.com/en/docs#weathervariables
 * 0=Clear, 1-3=Cloudy variants, 45/48=Fog,
 * 51-57=Drizzle, 61-67=Rain, 71-77=Snow,
 * 80-82=Showers, 85-86=Snow showers, 95+=Storm */

/* ── fetch weather from Open-Meteo ───────────────────────────── */
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      var lat = pos.coords.latitude;
      var lon = pos.coords.longitude;
      fetchOpenMeteo(lat, lon);
    },
    function (err) {
      console.log('Geolocation error: ' + err.message);
    },
    { timeout: 15000, maximumAge: 60000 }
  );
}

function fetchOpenMeteo(lat, lon) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + lat +
    '&longitude=' + lon +
    '&current=temperature_2m,weather_code,wind_speed_10m' +
    '&daily=temperature_2m_max,temperature_2m_min,weather_code,wind_speed_10m_max,sunrise,sunset' +
    '&wind_speed_unit=kmh' +
    '&timezone=auto' +
    '&forecast_days=2';

  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    try {
      var data = JSON.parse(this.responseText);
      sendWeather(data);
    } catch (e) {
      console.log('Parse error: ' + e);
    }
  };
  xhr.onerror = function () { console.log('XHR error'); };
  xhr.open('GET', url);
  xhr.send();
}

function sendWeather(data) {
  var cur = data.current || {};
  var daily = data.daily || {};

  /* today = index 0, tomorrow = index 1 */
  var todayMin  = daily.temperature_2m_min  ? Math.round(daily.temperature_2m_min[0])  : 0;
  var todayMax  = daily.temperature_2m_max  ? Math.round(daily.temperature_2m_max[0])  : 0;
  var todaySunrise = daily.sunrise ? daily.sunrise[0] : '';
  var todaySunset  = daily.sunset  ? daily.sunset[0]  : '';

  var tmrwMin  = daily.temperature_2m_min  ? Math.round(daily.temperature_2m_min[1])  : 0;
  var tmrwMax  = daily.temperature_2m_max  ? Math.round(daily.temperature_2m_max[1])  : 0;
  var tmrwCode = daily.weather_code        ? daily.weather_code[1]                     : 0;
  var tmrwWind = daily.wind_speed_10m_max  ? Math.round(daily.wind_speed_10m_max[1])  : 0;
  var tmrwSunrise = daily.sunrise ? daily.sunrise[1] : '';
  var tmrwSunset  = daily.sunset  ? daily.sunset[1]  : '';

  /* Open-Meteo sunrise/sunset are ISO strings like "2025-03-29T06:32"
   * Extract HH:MM */
  function isoToHHMM(iso) {
    if (!iso) return '--:--';
    var parts = iso.split('T');
    return parts.length > 1 ? parts[1].substring(0, 5) : '--:--';
  }

  var msg = {
    'TEMPERATURE':         Math.round(cur.temperature_2m || 0),
    'TEMP_MIN':            todayMin,
    'TEMP_MAX':            todayMax,
    'WIND_SPEED':          Math.round(cur.wind_speed_10m || 0),
    'WEATHER_CODE':        cur.weather_code || 0,
    'SUNRISE':             isoToHHMM(todaySunrise),
    'SUNSET':              isoToHHMM(todaySunset),
    'FORECAST_TEMP':       Math.round((tmrwMin + tmrwMax) / 2),
    'FORECAST_TEMP_MIN':   tmrwMin,
    'FORECAST_TEMP_MAX':   tmrwMax,
    'FORECAST_WEATHER_CODE': tmrwCode,
    'FORECAST_WIND':       tmrwWind,
    'FORECAST_SUNRISE':    isoToHHMM(tmrwSunrise),
    'FORECAST_SUNSET':     isoToHHMM(tmrwSunset),
    'UPDATE_TIMESTAMP':    Math.floor(Date.now() / 1000)
  };

  Pebble.sendAppMessage(msg,
    function () { console.log('Weather sent OK'); },
    function (e) { console.log('Weather send failed: ' + JSON.stringify(e)); }
  );
}

/* ── configuration page ──────────────────────────────────────── */
Pebble.addEventListener('showConfiguration', function () {
  var html = '<!DOCTYPE html><html><head>' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>' +
    'body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee}' +
    'h1{font-size:1.4em}' +
    'p{font-size:0.9em;color:#aaa}' +
    'button{width:100%;padding:12px;font-size:1.1em;background:#0f3460;' +
    'color:#fff;border:none;border-radius:4px;cursor:pointer;margin-top:20px}' +
    'button:active{background:#e94560}' +
    '</style></head><body>' +
    '<h1>No Time For This</h1>' +
    '<p>Configuration options coming soon.</p>' +
    '<button onclick="done()">Close</button>' +
    '<script>' +
    'function done(){' +
    'document.location.href="pebblejs://close#"+encodeURIComponent("{}");' +
    '}' +
    '</script></body></html>';

  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (e && e.response) {
    try {
      var cfg = JSON.parse(decodeURIComponent(e.response));
      console.log('Config closed: ' + JSON.stringify(cfg));
    } catch (ex) {
      console.log('Config parse error: ' + ex);
    }
  }
});

/* ── lifecycle ───────────────────────────────────────────────── */
Pebble.addEventListener('ready', function () {
  console.log('PebbleKit JS ready');
  fetchWeather();
});

/* respond to watch pings (hourly refresh) */
Pebble.addEventListener('appmessage', function (e) {
  console.log('AppMessage from watch - refreshing weather');
  fetchWeather();
});
