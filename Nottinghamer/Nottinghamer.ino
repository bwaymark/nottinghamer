// ============================================================
//  The Nottinghamer
//  A local information display for the LilyGO T-Display S3
//
//  Eight modes cycled with two buttons:
//    0  Weather        (Open-Meteo, free, no key)
//    1  Football       (football-data.org)
//    2  Panthers       (EIHL API)
//    3  Cricket        (Trent Bridge website)
//    4  Motorpoint     (Ticketmaster Discovery API)
//    5  Bin Collections (Nottingham City Council / ReCollect)
//    6  Bus Departures  (NCTx via crudworks Cloudflare Worker)
//    7  World Clock    (NTP, no external API)
//
//  Button 14 = next mode
//  Button 0  = previous mode
//
//  See config.h for all credentials and local settings.
//  See README.md for full setup instructions.
// ============================================================

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"

// --- Refresh intervals ---
const unsigned long WEATHER_INTERVAL  = 900000;    // 15 min
const unsigned long FOOTBALL_INTERVAL = 3600000;   // 1 hour
const unsigned long PANTHERS_INTERVAL = 3600000;   // 1 hour
const unsigned long CRICKET_INTERVAL  = 3600000;   // 1 hour
const unsigned long ARENA_INTERVAL    = 3600000;   // 1 hour
const unsigned long BINS_INTERVAL     = 86400000;  // 24 hours
const unsigned long BUS_INTERVAL      = 30000;     // 30 seconds

// --- Timezone structs ---
struct TZInfo {
  const char* label;
  int stdOffset;
  int dstOffset;
  int dstStartMonth;
  int dstEndMonth;
};

TZInfo ZONE_CITY1 = { CLOCK_CITY1_LABEL, CLOCK_CITY1_STD, CLOCK_CITY1_DST, CLOCK_CITY1_DST_START, CLOCK_CITY1_DST_END };
TZInfo ZONE_CITY2 = { CLOCK_CITY2_LABEL, CLOCK_CITY2_STD, CLOCK_CITY2_DST, CLOCK_CITY2_DST_START, CLOCK_CITY2_DST_END };
TZInfo ZONE_CITY3 = { CLOCK_CITY3_LABEL, CLOCK_CITY3_STD, CLOCK_CITY3_DST, CLOCK_CITY3_DST_START, CLOCK_CITY3_DST_END };
TZInfo ZONE_CITY4 = { CLOCK_CITY4_LABEL, CLOCK_CITY4_STD, CLOCK_CITY4_DST, CLOCK_CITY4_DST_START, CLOCK_CITY4_DST_END };

TZInfo* clockZones[] = { &ZONE_CITY1, &ZONE_CITY2, &ZONE_CITY3, &ZONE_CITY4 };

// --- Other structs ---
struct ArenaEvent {
  String name;
  String date;
  String time;
};

struct PanthersGame {
  String date;
  String time;
  String opponent;
  String comp;
};

struct CricketFixture {
  String date;
  String opponent;
  String competition;
};

// --- State ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
int currentMode = 0;
const int NUM_MODES = 8;
bool lastButtonNextState = HIGH;
bool lastButtonPrevState = HIGH;

unsigned long lastWeatherUpdate  = 0;
unsigned long lastFootballUpdate = 0;
unsigned long lastPanthersUpdate = 0;
unsigned long lastCricketUpdate  = 0;
unsigned long lastArenaUpdate    = 0;
unsigned long lastBinsUpdate     = 0;
unsigned long lastBusUpdate      = 0;
unsigned long lastClockSecond    = 0;

// =====================
// --- SHARED HELPERS ---
// =====================

void displayMessage(String l1, String l2 = "") {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10); tft.println(l1);
  if (l2 != "") { tft.setCursor(10, 40); tft.println(l2); }
}

void drawModeIndicator() {
  int spacing = 9;
  int startX  = 320/2 - (NUM_MODES * spacing)/2;
  for (int i = 0; i < NUM_MODES; i++) {
    sprite.fillCircle(startX + i * spacing, 174, 3,
      i == currentMode ? TFT_CYAN : TFT_DARKGREY);
  }
}

void setupNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = 0;
  int attempts = 0;
  while (now < 1000000000L && attempts < 20) {
    delay(500);
    time(&now);
    attempts++;
  }
}

String todayStr() {
  time_t now; time(&now);
  struct tm* utc = gmtime(&now);
  char buf[11];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
    utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday);
  return String(buf);
}

String formatDate(String iso) {
  if (iso.length() < 10) return iso;
  int year  = iso.substring(0, 4).toInt();
  int month = iso.substring(5, 7).toInt();
  int day   = iso.substring(8, 10).toInt();
  const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                           "Jul","Aug","Sep","Oct","Nov","Dec"};
  static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  int y = year; if (month < 3) y--;
  int dow = (y + y/4 - y/100 + y/400 + t[month-1] + day) % 7;
  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  char buf[12];
  snprintf(buf, sizeof(buf), "%s %d %s", days[dow], day, months[month]);
  return String(buf);
}

String formatTime(String iso) {
  if (iso.length() < 16) return "";
  return iso.substring(11, 16);
}

int daysFromToday(String dateStr) {
  if (dateStr.length() < 10) return 999;
  time_t now; time(&now);
  struct tm* utc = gmtime(&now);
  int todayYear  = utc->tm_year + 1900;
  int todayMonth = utc->tm_mon + 1;
  int todayDay   = utc->tm_mday;
  int year  = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day   = dateStr.substring(8, 10).toInt();
  auto toDays = [](int y, int m, int d) -> long {
    long days = y * 365L + y/4 - y/100 + y/400;
    int md[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if ((y%4==0&&y%100!=0)||y%400==0) md[2]=29;
    for (int i=1;i<m;i++) days+=md[i];
    return days+d;
  };
  return (int)(toDays(year,month,day)-toDays(todayYear,todayMonth,todayDay));
}

String daysLabel(int delta) {
  if (delta == 0) return "TODAY";
  if (delta == 1) return "TOMORROW";
  if (delta < 0)  return String(abs(delta)) + "d ago";
  return "in " + String(delta) + "d";
}

String stripHtml(String s) {
  String out = "";
  bool inTag = false;
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == '<') inTag = true;
    else if (s[i] == '>') inTag = false;
    else if (!inTag) out += s[i];
  }
  out.trim();
  return out;
}

uint16_t binColour(String subject) {
  String s = subject; s.toLowerCase();
  if (s.indexOf("recycl")  >= 0) return tft.color565(0, 180, 255);
  if (s.indexOf("food")    >= 0) return tft.color565(255, 140, 0);
  if (s.indexOf("garden")  >= 0) return tft.color565(0, 200, 0);
  if (s.indexOf("general") >= 0 || s.indexOf("waste") >= 0)
                                  return tft.color565(80, 80, 80);
  return TFT_WHITE;
}

// =====================
// --- CLOCK HELPERS ---
// =====================

bool isDST(struct tm* utc, TZInfo& tz) {
  if (tz.dstStartMonth == 0) return false;
  int month = utc->tm_mon + 1;
  if (month > tz.dstStartMonth && month < tz.dstEndMonth) return true;
  if (month < tz.dstStartMonth || month > tz.dstEndMonth) return false;
  if (month == tz.dstStartMonth) return utc->tm_mday >= 25;
  if (month == tz.dstEndMonth)   return utc->tm_mday < 25;
  return false;
}

String getTimeForZone(TZInfo& tz) {
  time_t now; time(&now);
  struct tm utcTime;
  gmtime_r(&now, &utcTime);
  int offsetMins = isDST(&utcTime, tz) ? tz.dstOffset : tz.stdOffset;
  time_t localTime = now + (offsetMins * 60);
  struct tm localTm;
  gmtime_r(&localTime, &localTm);
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &localTm);
  return String(buf);
}

String getDateForZone(TZInfo& tz) {
  time_t now; time(&now);
  struct tm utcTime;
  gmtime_r(&now, &utcTime);
  int offsetMins = isDST(&utcTime, tz) ? tz.dstOffset : tz.stdOffset;
  time_t localTime = now + (offsetMins * 60);
  struct tm localTm;
  gmtime_r(&localTime, &localTm);
  char buf[12];
  strftime(buf, sizeof(buf), "%a %d %b", &localTm);
  return String(buf);
}

// =====================
// --- MODE 0: WEATHER ---
// =====================

void fetchWeather() {
  displayMessage("Fetching", "weather...");

  char url[256];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,apparent_temperature,"
    "precipitation,weather_code,wind_speed_10m,wind_direction_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum"
    "&wind_speed_unit=mph"
    "&timezone=Europe/London"
    "&forecast_days=4",
    WEATHER_LAT, WEATHER_LON);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(15000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    displayMessage("Weather error", String(httpCode));
    http.end();
    lastWeatherUpdate = millis();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload)) {
    displayMessage("Weather JSON", "error");
    lastWeatherUpdate = millis();
    return;
  }

  JsonObject current = doc["current"];
  JsonObject daily   = doc["daily"];

  float tempC     = current["temperature_2m"]       | 0.0;
  float feelsLike = current["apparent_temperature"] | 0.0;
  float windMph   = current["wind_speed_10m"]       | 0.0;
  int   windDir   = current["wind_direction_10m"]   | 0;
  int   wCode     = current["weather_code"]         | 0;
  float precip    = current["precipitation"]        | 0.0;

  auto wDesc = [](int code) -> String {
    if (code == 0)  return "Clear sky";
    if (code <= 2)  return "Partly cloudy";
    if (code == 3)  return "Overcast";
    if (code <= 49) return "Foggy";
    if (code <= 57) return "Drizzle";
    if (code <= 67) return "Rain";
    if (code <= 77) return "Snow";
    if (code <= 82) return "Showers";
    if (code <= 86) return "Snow showers";
    if (code <= 99) return "Thunderstorm";
    return "Unknown";
  };

  auto windCompass = [](int deg) -> String {
    const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    return String(dirs[((deg + 22) / 45) % 8]);
  };

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_CYAN);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.print(WEATHER_CITY_NAME);
  sprite.println(" Weather");
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  sprite.setTextColor(TFT_YELLOW);
  sprite.setTextSize(4);
  sprite.setCursor(10, 32);
  sprite.print(String((int)round(tempC)));
  sprite.print((char)247);
  sprite.print("C");

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 72);
  sprite.print("Feels like ");
  sprite.print(String((int)round(feelsLike)));
  sprite.print((char)247);
  sprite.print("C");

  sprite.setTextColor(TFT_WHITE);
  sprite.setTextSize(2);
  sprite.setCursor(130, 35);
  String desc = wDesc(wCode);
  if (desc.length() > 12) desc = desc.substring(0, 12);
  sprite.println(desc);

  sprite.setTextSize(1);
  sprite.setTextColor(TFT_WHITE);
  sprite.setCursor(130, 60);
  sprite.print("Wind: ");
  sprite.print(String((int)round(windMph)));
  sprite.print("mph ");
  sprite.print(windCompass(windDir));

  sprite.setCursor(130, 72);
  sprite.print("Precip: ");
  sprite.print(String(precip, 1));
  sprite.print("mm");

  sprite.drawLine(0, 84, 320, 84, TFT_DARKGREY);

  JsonArray dates    = daily["time"];
  JsonArray maxTemps = daily["temperature_2m_max"];
  JsonArray minTemps = daily["temperature_2m_min"];
  JsonArray dayCodes = daily["weather_code"];
  JsonArray dayPrec  = daily["precipitation_sum"];

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 88);
  sprite.print("DAY         HI  LO  RAIN  COND");
  sprite.drawLine(0, 96, 320, 96, TFT_DARKGREY);

  int y = 100;
  for (int i = 0; i < 4 && i < (int)dates.size(); i++) {
    String date = dates[i] | "";
    float maxT  = maxTemps[i] | 0.0;
    float minT  = minTemps[i] | 0.0;
    int   dc    = dayCodes[i] | 0;
    float pr    = dayPrec[i]  | 0.0;

    bool isToday = (daysFromToday(date) == 0);
    sprite.setTextColor(isToday ? TFT_YELLOW : TFT_WHITE);
    sprite.setCursor(10, y);

    String dayLabel = isToday ? "Today      " : formatDate(date);
    if (!isToday && (int)dayLabel.length() < 11)
      while ((int)dayLabel.length() < 11) dayLabel += " ";

    char buf[32];
    snprintf(buf, sizeof(buf), "%2d  %2d  %4.1f  %s",
      (int)round(maxT), (int)round(minT), pr,
      wDesc(dc).substring(0,6).c_str());
    sprite.print(dayLabel + " " + String(buf));

    y += 18;
    if (i < 3) sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastWeatherUpdate = millis();
}

// =======================
// --- MODE 1: FOOTBALL ---
// =======================

void fetchFootball() {
  displayMessage("Fetching", "football...");

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_CYAN);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println("Football Fixtures");
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  int yPos = 32;

  int teamIds[]          = { FOOTBALL_TEAM1_ID, FOOTBALL_TEAM2_ID };
  const char* teamNames[] = { FOOTBALL_TEAM1_NAME, FOOTBALL_TEAM2_NAME };
  uint16_t teamColours[]  = {
    tft.color565(FOOTBALL_TEAM1_R, FOOTBALL_TEAM1_G, FOOTBALL_TEAM1_B),
    tft.color565(FOOTBALL_TEAM2_R, FOOTBALL_TEAM2_G, FOOTBALL_TEAM2_B)
  };

  for (int t = 0; t < 2; t++) {
    String url = "https://api.football-data.org/v4/teams/";
    url += teamIds[t];
    url += "/matches?status=SCHEDULED&limit=3";

    HTTPClient http;
    http.begin(url);
    http.addHeader("X-Auth-Token", FOOTBALL_API_KEY);
    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode != 200) {
      sprite.setTextColor(TFT_RED);
      sprite.setTextSize(1);
      sprite.setCursor(10, yPos);
      sprite.print(String(teamNames[t]) + " error " + String(httpCode));
      yPos += 20;
      http.end();
      continue;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(16384);
    if (deserializeJson(doc, payload)) {
      sprite.setTextColor(TFT_RED);
      sprite.setTextSize(1);
      sprite.setCursor(10, yPos);
      sprite.print(String(teamNames[t]) + " JSON error");
      yPos += 20;
      continue;
    }

    JsonArray matches = doc["matches"];

    sprite.setTextColor(teamColours[t]);
    sprite.setTextSize(2);
    sprite.setCursor(10, yPos);
    sprite.println(teamNames[t]);
    yPos += 22;

    if (matches.isNull() || matches.size() == 0) {
      sprite.setTextColor(TFT_DARKGREY);
      sprite.setTextSize(1);
      sprite.setCursor(10, yPos);
      sprite.print("No fixtures found");
      yPos += 16;
    } else {
      int shown = 0;
      for (JsonObject match : matches) {
        if (shown >= 3) break;

        String utcDate  = match["utcDate"] | "";
        String homeTeam = match["homeTeam"]["shortName"] |
                          match["homeTeam"]["name"] | "?";
        String awayTeam = match["awayTeam"]["shortName"] |
                          match["awayTeam"]["name"] | "?";
        String comp     = match["competition"]["name"] | "";

        if (comp.indexOf("Premier") >= 0)           comp = "PL";
        else if (comp.indexOf("Championship") >= 0) comp = "Champ";
        else if (comp.indexOf("League One") >= 0)   comp = "L1";
        else if (comp.indexOf("League Two") >= 0)   comp = "L2";
        else if (comp.indexOf("FA Cup") >= 0)       comp = "FAC";
        else if (comp.indexOf("EFL") >= 0)          comp = "EFL";
        else if (comp.indexOf("Europa") >= 0)       comp = "UEL";
        else if (comp.length() > 6) comp = comp.substring(0, 6);

        String dateStr = utcDate.substring(0, 10);
        String timeStr = formatTime(utcDate);

        String t1name = String(teamNames[t]);
        bool isHome = (homeTeam.indexOf(t1name.substring(0,5)) >= 0);

        String opponent = isHome ? awayTeam : homeTeam;
        String ha       = isHome ? "H" : "A";
        if (opponent.length() > 14) opponent = opponent.substring(0, 14);

        sprite.setTextColor(TFT_WHITE);
        sprite.setTextSize(1);
        sprite.setCursor(10, yPos);

        char buf[48];
        snprintf(buf, sizeof(buf), "%s %s  %-14s %s [%s]",
          formatDate(dateStr).c_str(),
          timeStr.c_str(),
          opponent.c_str(),
          ha.c_str(),
          comp.c_str());
        sprite.print(buf);
        yPos += 14;
        shown++;
      }
    }

    if (t == 0) {
      sprite.drawLine(0, yPos + 2, 320, yPos + 2, TFT_DARKGREY);
      yPos += 8;
    }
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastFootballUpdate = millis();
}

// =======================
// --- MODE 2: PANTHERS ---
// =======================

void fetchPanthers() {
  displayMessage("Fetching", "Panthers...");

  String url = "https://www.eliteleague.co.uk/api/schedule?team=";
  url += EIHL_TEAM_ID;
  url += "&season=";
  url += EIHL_SEASON_ID;

  HTTPClient http;
  http.begin(url);
  http.addHeader("Accept", "application/json");
  http.setTimeout(15000);
  int httpCode = http.GET();

  USBSerial.println("Panthers HTTP: " + String(httpCode));

  String payload = "";
  if (httpCode == 200) {
    payload = http.getString();
    USBSerial.println("Panthers: " + payload.substring(0, 200));
  }
  http.end();

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(tft.color565(255, 165, 0));
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println(String(EIHL_TEAM_NAME) + " Home");
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 30);  sprite.print("DATE");
  sprite.setCursor(105, 30); sprite.print("TIME");
  sprite.setCursor(148, 30); sprite.print("OPPONENT");
  sprite.setCursor(268, 30); sprite.print("COMP");
  sprite.drawLine(0, 39, 320, 39, TFT_DARKGREY);

  PanthersGame games[5];
  int gameCount = 0;

  if (httpCode == 200 && payload.length() > 10) {
    DynamicJsonDocument doc(32768);
    if (!deserializeJson(doc, payload)) {
      JsonArray schedule;
      if (doc["schedule"].is<JsonArray>())   schedule = doc["schedule"].as<JsonArray>();
      else if (doc["games"].is<JsonArray>()) schedule = doc["games"].as<JsonArray>();
      else if (doc.is<JsonArray>())          schedule = doc.as<JsonArray>();

      for (JsonObject game : schedule) {
        if (gameCount >= 5) break;

        String homeTeam = "";
        if (!game["home"]["shortCode"].isNull())
          homeTeam = game["home"]["shortCode"].as<String>();
        else if (!game["homeTeam"].isNull())
          homeTeam = game["homeTeam"].as<String>();

        String eihlCode = String(EIHL_TEAM_SHORT);
        if (homeTeam != eihlCode && homeTeam.indexOf(EIHL_TEAM_NAME) < 0) continue;

        String dateStr = "";
        if (!game["date"].isNull())         dateStr = game["date"].as<String>();
        else if (!game["gameDate"].isNull()) dateStr = game["gameDate"].as<String>();
        if (dateStr.length() < 10) continue;
        if (daysFromToday(dateStr.substring(0, 10)) < 0) continue;

        String opponent = "";
        if (!game["away"]["name"].isNull())
          opponent = game["away"]["name"].as<String>();
        else if (!game["awayTeam"].isNull())
          opponent = game["awayTeam"].as<String>();
        if (opponent == "") opponent = "TBC";

        String gameTime = "";
        if (!game["time"].isNull())         gameTime = game["time"].as<String>();
        else if (!game["gameTime"].isNull()) gameTime = game["gameTime"].as<String>();

        String comp = "";
        if (!game["competition"].isNull()) comp = game["competition"].as<String>();
        else if (!game["type"].isNull())   comp = game["type"].as<String>();
        if (comp == "") comp = "EIHL";

        games[gameCount].date     = dateStr.substring(0, 10);
        games[gameCount].time     = gameTime;
        games[gameCount].opponent = opponent;
        games[gameCount].comp     = comp;
        gameCount++;
      }
    }
  }

  if (gameCount == 0) {
    sprite.setTextColor(TFT_YELLOW);
    sprite.setTextSize(1);
    sprite.setCursor(10, 50);
    sprite.println("No home fixtures found");
    sprite.setTextColor(TFT_DARKGREY);
    sprite.setCursor(10, 65);
    sprite.println("(API unavailable)");
  } else {
    int y = 44;
    for (int i = 0; i < gameCount; i++) {
      bool isNext = (i == 0);
      if (isNext) sprite.fillRect(0, y - 2, 320, 16, 0x1082);

      sprite.setTextColor(isNext ? TFT_YELLOW : TFT_WHITE);
      sprite.setTextSize(1);
      sprite.setCursor(10, y);
      sprite.print(formatDate(games[i].date));
      sprite.setCursor(105, y);
      sprite.print(games[i].time.substring(0, 5));
      sprite.setCursor(148, y);
      String opp = games[i].opponent;
      if (opp.length() > 14) opp = opp.substring(0, 14);
      sprite.print(opp);
      sprite.setCursor(268, y);
      sprite.print(games[i].comp.substring(0, 4));

      y += 18;
      sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
    }
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastPanthersUpdate = millis();
}

// ======================
// --- MODE 3: CRICKET ---
// ======================

void fetchCricket() {
  displayMessage("Fetching", "cricket...");

  HTTPClient http;
  http.begin(CRICKET_FIXTURES_URL);
  http.setTimeout(15000);
  int httpCode = http.GET();

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(tft.color565(0, 180, 80));
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println(CRICKET_VENUE_SHORT);
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  if (httpCode != 200) {
    sprite.setTextColor(TFT_RED);
    sprite.setTextSize(1);
    sprite.setCursor(10, 40);
    sprite.print("HTTP error: " + String(httpCode));
    http.end();
    drawModeIndicator();
    sprite.pushSprite(0, 0);
    lastCricketUpdate = millis();
    return;
  }

  String payload = http.getString();
  http.end();

  CricketFixture fixtures[5];
  int fixCount = 0;

  int pos = 0;
  while (fixCount < 5 && pos < (int)payload.length()) {
    int datePos = payload.indexOf("datetime=\"20", pos);
    if (datePos < 0) break;

    datePos += 10;
    String dateStr = payload.substring(datePos, datePos + 10);

    if (daysFromToday(dateStr) < 0) { pos = datePos + 10; continue; }

    int blockEnd = payload.indexOf("datetime=\"20", datePos + 10);
    if (blockEnd < 0) blockEnd = min((int)payload.length(), datePos + 2000);
    String block = payload.substring(datePos, blockEnd);

    bool isHome = (block.indexOf(CRICKET_VENUE_NAME) >= 0 ||
                   block.indexOf(CRICKET_VENUE_ALT) >= 0);

    if (!isHome) { pos = datePos + 10; continue; }

    String opponent = "TBC";
    int vPos = block.indexOf(" v ");
    if (vPos < 0) vPos = block.indexOf(" vs ");
    if (vPos >= 0) {
      int end = block.indexOf("<", vPos);
      if (end < 0) end = min((int)block.length(), vPos + 40);
      opponent = stripHtml(block.substring(vPos + 3, end));
      opponent.trim();
    }

    String comp = "CRK";
    if (block.indexOf("County Championship") >= 0) comp = "CC";
    else if (block.indexOf("T20 Blast") >= 0)      comp = "T20";
    else if (block.indexOf("One-Day Cup") >= 0)    comp = "ODC";
    else if (block.indexOf("The Hundred") >= 0)    comp = "100";

    if (opponent.length() > 1) {
      fixtures[fixCount].date        = dateStr;
      fixtures[fixCount].opponent    = opponent;
      fixtures[fixCount].competition = comp;
      fixCount++;
    }
    pos = datePos + 10;
  }

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 30);  sprite.print("DATE");
  sprite.setCursor(120, 30); sprite.print("OPPONENT");
  sprite.setCursor(258, 30); sprite.print("COMP");
  sprite.drawLine(0, 39, 320, 39, TFT_DARKGREY);

  if (fixCount == 0) {
    sprite.setTextColor(TFT_YELLOW);
    sprite.setTextSize(1);
    sprite.setCursor(10, 50);
    sprite.println("No home fixtures found");
  } else {
    int y = 44;
    for (int i = 0; i < fixCount; i++) {
      bool isNext = (i == 0);
      if (isNext) sprite.fillRect(0, y - 2, 320, 16, 0x1082);

      sprite.setTextColor(isNext ? TFT_YELLOW : TFT_WHITE);
      sprite.setTextSize(1);
      sprite.setCursor(10, y);
      sprite.print(formatDate(fixtures[i].date));
      sprite.setCursor(120, y);
      String opp = fixtures[i].opponent;
      if (opp.length() > 16) opp = opp.substring(0, 16);
      sprite.print(opp);
      sprite.setCursor(258, y);
      sprite.print(fixtures[i].competition);

      y += 18;
      sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
    }
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastCricketUpdate = millis();
}

// ==========================
// --- MODE 4: ARENA EVENTS --
// ==========================

void fetchArena() {
  displayMessage("Fetching", "arena events...");

  String url = "https://app.ticketmaster.com/discovery/v2/events.json"
               "?venueId=";
  url += TICKETMASTER_VENUE_ID;
  url += "&size=5&sort=date,asc&apikey=";
  url += TICKETMASTER_KEY;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(15000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    displayMessage("Arena error", String(httpCode));
    http.end();
    lastArenaUpdate = millis();
    return;
  }

  String payload = http.getString();
  http.end();

  ArenaEvent events[5];
  int eventCount = 0;

  int pos = 0;
  while (eventCount < 5 && pos < (int)payload.length()) {
    int namePos = payload.indexOf("\"name\":\"", pos);
    if (namePos < 0) break;
    namePos += 8;
    int nameEnd = payload.indexOf("\"", namePos);
    if (nameEnd < 0) break;
    String name = payload.substring(namePos, nameEnd);

    int typePos = payload.indexOf("\"type\":\"", namePos);
    if (typePos < 0) break;
    typePos += 8;
    int typeEnd = payload.indexOf("\"", typePos);
    String type = payload.substring(typePos, typeEnd);

    if (type != "event") { pos = nameEnd + 1; continue; }
    if (name.indexOf("Season Ticket") >= 0) { pos = nameEnd + 1; continue; }

    int datePos = payload.indexOf("\"localDate\":\"", namePos);
    if (datePos < 0) break;
    datePos += 13;
    int dateEnd = payload.indexOf("\"", datePos);
    String date = payload.substring(datePos, dateEnd);

    String time = "";
    int timePos = payload.indexOf("\"localTime\":\"", datePos);
    if (timePos >= 0 && timePos < datePos + 300) {
      timePos += 13;
      int timeEnd = payload.indexOf("\"", timePos);
      time = payload.substring(timePos, timeEnd);
      if (time.length() >= 5) time = time.substring(0, 5);
    }

    events[eventCount].name = name;
    events[eventCount].date = date;
    events[eventCount].time = time;
    eventCount++;
    pos = dateEnd + 1;
  }

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_MAGENTA);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println(ARENA_NAME);
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  if (eventCount == 0) {
    sprite.setTextColor(TFT_DARKGREY);
    sprite.setTextSize(1);
    sprite.setCursor(10, 40);
    sprite.print("No upcoming events found");
    drawModeIndicator();
    sprite.pushSprite(0, 0);
    lastArenaUpdate = millis();
    return;
  }

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 30);  sprite.print("DATE");
  sprite.setCursor(110, 30); sprite.print("TIME");
  sprite.setCursor(155, 30); sprite.print("EVENT");
  sprite.drawLine(0, 39, 320, 39, TFT_DARKGREY);

  int y = 44;
  for (int i = 0; i < eventCount; i++) {
    bool isNext = (i == 0);
    if (isNext) sprite.fillRect(0, y - 2, 320, 22, 0x1082);

    sprite.setTextColor(isNext ? TFT_YELLOW : TFT_WHITE);
    sprite.setTextSize(1);
    sprite.setCursor(10, y + 4);
    sprite.print(formatDate(events[i].date));
    sprite.setCursor(110, y + 4);
    sprite.print(events[i].time);
    String name = events[i].name;
    if (name.length() > 19) name = name.substring(0, 19);
    sprite.setCursor(155, y + 4);
    sprite.print(name);

    y += 24;
    sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastArenaUpdate = millis();
}

// ======================
// --- MODE 5: BINS ---
// ======================

void fetchBins() {
  displayMessage("Fetching", "bin collections...");

  time_t now; time(&now);
  struct tm* utc = gmtime(&now);
  char todayBuf[11], endBuf[11];
  snprintf(todayBuf, sizeof(todayBuf), "%04d-%02d-%02d",
    utc->tm_year+1900, utc->tm_mon+1, utc->tm_mday);
  time_t future = now + (60 * 86400);
  struct tm* futureUtc = gmtime(&future);
  snprintf(endBuf, sizeof(endBuf), "%04d-%02d-%02d",
    futureUtc->tm_year+1900, futureUtc->tm_mon+1, futureUtc->tm_mday);

  String url = "https://api.eu.recollect.net/api/places/";
  url += BINS_PLACE_ID;
  url += "/services/";
  url += BINS_SERVICE_ID;
  url += "/events?after=";
  url += todayBuf;
  url += "&before=";
  url += endBuf;
  url += "&locale=en-GB";

  HTTPClient http;
  http.begin(url);
  http.addHeader("User-Agent", "Mozilla/5.0");
  http.addHeader("Accept", "application/json");
  http.setTimeout(15000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    displayMessage("Bins error", String(httpCode));
    lastBinsUpdate = millis();
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(16384);
  if (deserializeJson(doc, payload)) {
    displayMessage("Bins JSON", "error");
    lastBinsUpdate = millis();
    return;
  }

  JsonArray events;
  if (doc.is<JsonArray>())               events = doc.as<JsonArray>();
  else if (doc["events"].is<JsonArray>()) events = doc["events"].as<JsonArray>();

  struct BinEvent {
    String dateStr;
    String bins;
    int daysAway;
  };
  BinEvent upcoming[5];
  int count = 0;

  if (!events.isNull()) {
    for (JsonObject ev : events) {
      if (count >= 5) break;
      String dayStr = ev["day"] | "";
      if (dayStr.length() < 10) continue;
      dayStr = dayStr.substring(0, 10);
      int delta = daysFromToday(dayStr);
      if (delta < 0) continue;

      JsonArray flags = ev["flags"].as<JsonArray>();
      String bins = "";
      for (JsonObject flag : flags) {
        String subject = flag["subject"] | flag["name"] | "Collection";
        subject = stripHtml(subject);
        if (bins != "") bins += ", ";
        bins += subject;
      }
      if (bins == "") bins = "Collection";

      upcoming[count].dateStr  = dayStr;
      upcoming[count].bins     = bins;
      upcoming[count].daysAway = delta;
      count++;
    }
  }

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_CYAN);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println("Bin Collections");
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 30);  sprite.print("DATE");
  sprite.setCursor(130, 30); sprite.print("BINS");
  sprite.setCursor(258, 30); sprite.print("WHEN");
  sprite.drawLine(0, 39, 320, 39, TFT_DARKGREY);

  if (count == 0) {
    sprite.setTextColor(TFT_YELLOW);
    sprite.setTextSize(1);
    sprite.setCursor(10, 50);
    sprite.print("No collections found");
  } else {
    int y = 44;
    for (int i = 0; i < count; i++) {
      bool isNext = (i == 0);
      if (isNext) sprite.fillRect(0, y - 2, 320, 22, 0x1082);

      sprite.setTextColor(isNext ? TFT_YELLOW : TFT_WHITE);
      sprite.setTextSize(1);
      sprite.setCursor(10, y + 4);
      sprite.print(formatDate(upcoming[i].dateStr));

      uint16_t col = binColour(upcoming[i].bins);
      sprite.setTextColor(isNext ? col : TFT_DARKGREY);
      sprite.setCursor(130, y + 4);
      String bins = upcoming[i].bins;
      if (bins.length() > 16) bins = bins.substring(0, 16);
      sprite.print(bins);

      sprite.setTextColor(isNext ? TFT_YELLOW : TFT_DARKGREY);
      sprite.setCursor(258, y + 4);
      String when = daysLabel(upcoming[i].daysAway);
      if (when.length() > 9) when = when.substring(0, 9);
      sprite.print(when);

      y += 24;
      sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
    }
  }

  time_t nowFt; time(&nowFt);
  bool bst = false;
  {
    struct tm* lt = gmtime(&nowFt);
    int m = lt->tm_mon + 1;
    bst = (m > 3 && m < 10) ||
          (m == 3 && lt->tm_mday >= 25) ||
          (m == 10 && lt->tm_mday < 25);
  }
  time_t ukTime = nowFt + (bst ? 3600 : 0);
  struct tm* ukTm = gmtime(&ukTime);
  char timeBuf[6];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M", ukTm);
  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 160);
  sprite.print("Updated: ");
  sprite.print(timeBuf);

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastBinsUpdate = millis();
}

// ===================
// --- MODE 6: BUS ---
// ===================

void fetchBus() {
  displayMessage("Fetching", "buses...");

  String url = "https://nctx.crudworks.workers.dev/?stopId=";
  url += BUS_STOP_ID;
  url += "&maxResults=5";

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    displayMessage("Bus error", String(httpCode));
    http.end();
    lastBusUpdate = millis();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload)) {
    displayMessage("Bus JSON", "error");
    lastBusUpdate = millis();
    return;
  }

  JsonArray deps = doc["departures"];
  if (deps.isNull() || deps.size() == 0) {
    displayMessage("No buses", String("due at ") + BUS_STOP_ID);
    lastBusUpdate = millis();
    return;
  }

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_CYAN);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.print(BUS_STOP_ID);
  sprite.print(" ");
  sprite.setTextColor(TFT_WHITE);
  sprite.print(BUS_STOP_NAME);
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 30);  sprite.print("BUS");
  sprite.setCursor(55, 30);  sprite.print("DESTINATION");
  sprite.setCursor(270, 30); sprite.print("DUE");
  sprite.drawLine(0, 39, 320, 39, TFT_DARKGREY);

  int y = 44;
  int count = 0;

  for (JsonObject dep : deps) {
    if (count >= 5) break;

    String route    = dep["routeNumber"] | "?";
    String dest     = dep["destination"] | "Unknown";
    String due      = dep["expected"]    | "?";
    bool   realTime = dep["isRealTime"]  | false;
    String hexCol   = dep["lineColour"]  | "#FFFFFF";

    long r = strtol(hexCol.substring(1, 3).c_str(), NULL, 16);
    long g = strtol(hexCol.substring(3, 5).c_str(), NULL, 16);
    long b = strtol(hexCol.substring(5, 7).c_str(), NULL, 16);
    uint16_t lineCol = tft.color565(r, g, b);

    sprite.fillRect(8, y, 38, 18, lineCol);
    sprite.setTextColor(TFT_BLACK);
    sprite.setTextSize(1);
    int rw = route.length() * 6;
    sprite.setCursor(8 + (38 - rw) / 2, y + 5);
    sprite.print(route);

    if (dest.length() > 20) dest = dest.substring(0, 20);
    sprite.setTextColor(TFT_WHITE);
    sprite.setCursor(55, y + 4);
    sprite.print(dest);

    sprite.setTextColor(realTime ? TFT_GREEN : TFT_YELLOW);
    sprite.setCursor(270, y + 4);
    sprite.print(due);

    y += 24;
    sprite.drawLine(0, y - 2, 320, y - 2, 0x2104);
    count++;
  }

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastBusUpdate = millis();
}

// ====================
// --- MODE 7: CLOCK ---
// ====================

void drawClock() {
  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_CYAN);
  sprite.setTextSize(2);
  sprite.setCursor(10, 5);
  sprite.println("World Clock");
  sprite.drawLine(0, 26, 320, 26, TFT_DARKGREY);

  String date = getDateForZone(*clockZones[0]);
  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(220, 10);
  sprite.println(date);

  int y = 34;
  for (int i = 0; i < 4; i++) {
    bool isHome = (i == 0);
    if (isHome) sprite.fillRect(0, y, 320, 22, 0x1082);

    sprite.setTextColor(isHome ? TFT_YELLOW : TFT_WHITE);
    sprite.setTextSize(2);
    sprite.setCursor(10, y + 3);
    sprite.print(String(clockZones[i]->label));

    String timeStr = getTimeForZone(*clockZones[i]);
    sprite.setTextColor(isHome ? TFT_YELLOW : TFT_GREEN);
    sprite.setTextSize(2);
    sprite.setCursor(230, y + 3);
    sprite.print(timeStr);

    y += 26;
    if (!isHome) sprite.drawLine(0, y, 320, y, 0x2104);
  }

  sprite.drawLine(0, 167, 320, 167, TFT_DARKGREY);
  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextSize(1);
  sprite.setCursor(10, 171);
  sprite.print("btn14=next  btn0=prev");

  drawModeIndicator();
  sprite.pushSprite(0, 0);
  lastClockSecond = millis();
}

// ================
// --- DISPATCH ---
// ================

void fetchCurrentMode() {
  switch (currentMode) {
    case 0: fetchWeather();  break;
    case 1: fetchFootball(); break;
    case 2: fetchPanthers(); break;
    case 3: fetchCricket();  break;
    case 4: fetchArena();    break;
    case 5: fetchBins();     break;
    case 6: fetchBus();      break;
    case 7: drawClock();     break;
  }
}

bool needsRefresh() {
  unsigned long now = millis();
  switch (currentMode) {
    case 0: return now - lastWeatherUpdate  >= WEATHER_INTERVAL;
    case 1: return now - lastFootballUpdate >= FOOTBALL_INTERVAL;
    case 2: return now - lastPanthersUpdate >= PANTHERS_INTERVAL;
    case 3: return now - lastCricketUpdate  >= CRICKET_INTERVAL;
    case 4: return now - lastArenaUpdate    >= ARENA_INTERVAL;
    case 5: return now - lastBinsUpdate     >= BINS_INTERVAL;
    case 6: return now - lastBusUpdate      >= BUS_INTERVAL;
    case 7: return now - lastClockSecond    >= 1000;
  }
  return false;
}

// ============
// --- SETUP ---
// ============

void setup() {
  USBSerial.begin(115200);
  delay(2000);
  USBSerial.println("The Nottinghamer starting...");

  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  pinMode(BUTTON_PREV, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  sprite.createSprite(320, 180);

  displayMessage("The Nottinghamer", "Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    displayMessage("WiFi failed!", "Check config.h");
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10); tft.println("Connected!");
  tft.setCursor(10, 40); tft.println(WiFi.localIP().toString());
  delay(1500);

  setupNTP();
  fetchCurrentMode();
}

// ============
// --- LOOP ---
// ============

void loop() {
  bool btnNext = digitalRead(BUTTON_NEXT);
  bool btnPrev = digitalRead(BUTTON_PREV);

  if (btnNext == LOW && lastButtonNextState == HIGH) {
    delay(50);
    if (digitalRead(BUTTON_NEXT) == LOW) {
      currentMode = (currentMode + 1) % NUM_MODES;
      fetchCurrentMode();
    }
  }
  lastButtonNextState = btnNext;

  if (btnPrev == LOW && lastButtonPrevState == HIGH) {
    delay(50);
    if (digitalRead(BUTTON_PREV) == LOW) {
      currentMode = (currentMode - 1 + NUM_MODES) % NUM_MODES;
      fetchCurrentMode();
    }
  }
  lastButtonPrevState = btnPrev;

  if (needsRefresh()) {
    fetchCurrentMode();
  }
}
