// ============================================================
//  The Nottinghamer — config.h
//  Fill in ALL settings in this file before uploading.
//  See README.md for instructions on obtaining each value.
// ============================================================

#pragma once

// ============================================================
//  WIFI
// ============================================================
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"

// ============================================================
//  HARDWARE BUTTONS
//  T-Display S3 has buttons on GPIO 14 and GPIO 0.
//  GPIO 14 = next mode, GPIO 0 = previous mode.
// ============================================================
#define BUTTON_NEXT  14
#define BUTTON_PREV   0

// ============================================================
//  WEATHER (Open-Meteo — free, no account needed)
//  Set to the latitude and longitude of your location.
//  Find yours at https://www.latlong.net
// ============================================================
#define WEATHER_LAT         52.9548f
#define WEATHER_LON         -1.1581f
#define WEATHER_CITY_NAME   "Nottingham"

// ============================================================
//  FOOTBALL (football-data.org)
//  Register free at https://www.football-data.org/client/register
//  Find your team ID by browsing https://api.football-data.org/v4/teams
//  or searching the docs. Common IDs:
//    Nottingham Forest = 67
//    Notts County      = 345
//    Man City          = 65
//    Arsenal           = 57
//    Liverpool         = 64
//  Colours: set RGB values 0-255 to match your team's colours.
// ============================================================
#define FOOTBALL_API_KEY       "YOUR_FOOTBALL_DATA_API_KEY"

#define FOOTBALL_TEAM1_ID      67
#define FOOTBALL_TEAM1_NAME    "Forest"
#define FOOTBALL_TEAM1_R       255
#define FOOTBALL_TEAM1_G       50
#define FOOTBALL_TEAM1_B       50

#define FOOTBALL_TEAM2_ID      345
#define FOOTBALL_TEAM2_NAME    "Notts County"
#define FOOTBALL_TEAM2_R       200
#define FOOTBALL_TEAM2_G       200
#define FOOTBALL_TEAM2_B       200

// ============================================================
//  ICE HOCKEY (EIHL — no account needed)
//  Team IDs from the EIHL API:
//    Nottingham Panthers = 12
//    Sheffield Steelers  = 2
//    Cardiff Devils      = 1
//    Belfast Giants      = 3
//    Dundee Stars        = 5
//    Fife Flyers         = 6
//    Glasgow Clan        = 7
//    Guildford Flames    = 8
//    Manchester Storm    = 9
//    Milton Keynes       = 10
//  Season ID: check eliteleague.co.uk — 36 = 2025/26 season.
//  EIHL_TEAM_SHORT is the short code used in the schedule API
//  to identify home games (usually 3 letters, e.g. NOT).
// ============================================================
#define EIHL_TEAM_ID       "12"
#define EIHL_SEASON_ID     "36"
#define EIHL_TEAM_NAME     "Nottingham"
#define EIHL_TEAM_SHORT    "NOT"

// ============================================================
//  CRICKET (Trent Bridge website scraper — no account needed)
//  CRICKET_FIXTURES_URL: the fixtures page for your county.
//  CRICKET_VENUE_NAME: text that appears in the page when a
//    fixture is at your home ground (used to detect home games).
//  CRICKET_VENUE_ALT: alternative spelling of the venue.
//  CRICKET_VENUE_SHORT: short display name for the screen header.
//
//  Other county URLs:
//    Lancashire:  https://www.lancashirecricket.co.uk/cricket/fixtures
//    Yorkshire:   https://www.yorkshireccc.com/cricket/fixtures
//    Warwickshire: https://www.edgbaston.com/cricket/fixtures
// ============================================================
#define CRICKET_FIXTURES_URL  "https://www.trentbridge.co.uk/cricket/first-xi-fixtures.html"
#define CRICKET_VENUE_NAME    "Trent Bridge"
#define CRICKET_VENUE_ALT     "trent-bridge"
#define CRICKET_VENUE_SHORT   "Cricket @ Trent Brdg"

// ============================================================
//  ARENA EVENTS (Ticketmaster Discovery API)
//  Register free at https://developer.ticketmaster.com
//  To find your venue ID:
//    1. Go to https://app.ticketmaster.com/discovery/v2/venues.json
//       ?keyword=YOUR+VENUE+NAME&countryCode=GB&apikey=YOUR_KEY
//    2. Look for the "id" field in the response.
//  Motorpoint Arena Nottingham = KovZ9177tQ7
//  ARENA_NAME is the display name shown on screen (max ~16 chars).
// ============================================================
#define TICKETMASTER_KEY       "YOUR_TICKETMASTER_CONSUMER_KEY"
#define TICKETMASTER_VENUE_ID  "KovZ9177tQ7"
#define ARENA_NAME             "Motorpoint Arena"

// ============================================================
//  BIN COLLECTIONS (Nottingham City Council via ReCollect API)
//  This works for addresses served by Nottingham City Council.
//  To find your Place ID:
//    1. Go to https://www.nottinghamcity.gov.uk/binreminders
//    2. Enter your postcode and select your address
//    3. Open browser DevTools (F12) > Network tab
//    4. Look for a request to api.eu.recollect.net
//    5. Your Place ID is the UUID in the URL path
//  The Service ID (50003) is fixed for Nottingham.
//
//  Other councils may use different providers — check your
//  council website for a bin reminder or calendar service.
// ============================================================
#define BINS_PLACE_ID    "CA0889C8-DBFF-11EE-96DB-AC388EA0F4B8"
#define BINS_SERVICE_ID  "50003"

// ============================================================
//  BUS DEPARTURES (Nottingham City Transport via crudworks)
//  This uses Simon Prickett's free Cloudflare Worker which
//  wraps the NCTx live departures API. NCTx buses only.
//
//  To find your Stop ID (ATCO code):
//    1. Go to https://www.traveline.info or
//       https://bustimes.org
//    2. Search for your stop by name or postcode
//    3. The ATCO code is shown in the stop details
//       It looks like: 3390FF03
//    4. Paste it into BUS_STOP_ID below
//
//  BUS_STOP_NAME is just a display label shown on screen.
// ============================================================
#define BUS_STOP_ID    "3390FF03"
#define BUS_STOP_NAME  "Beech Ave"

// ============================================================
//  WORLD CLOCK
//  Four cities shown on the clock screen.
//  City 1 should be your home city.
//
//  Offsets are in minutes from UTC:
//    London (GMT/BST):      std=0,    dst=60
//    Paris (CET/CEST):      std=60,   dst=120
//    Dubai (GST, no DST):   std=240,  dst=240
//    New York (EST/EDT):    std=-300, dst=-240
//    Chicago (CST/CDT):     std=-360, dst=-300
//    Denver (MST/MDT):      std=-420, dst=-360
//    Los Angeles (PST/PDT): std=-480, dst=-420
//    Vancouver (PST/PDT):   std=-480, dst=-420
//    Calgary (MST/MDT):     std=-420, dst=-360
//    Ottawa (EST/EDT):      std=-300, dst=-240
//    Halifax (AST/ADT):     std=-240, dst=-180
//    Riyadh (AST, no DST):  std=180,  dst=180
//    Colombo (no DST):      std=330,  dst=330
//    Singapore (no DST):    std=480,  dst=480
//    Sydney (AEDT/AEST):    std=660,  dst=600
//    Auckland (NZDT/NZST):  std=780,  dst=720
//
//  DST months (Northern Hemisphere):
//    Europe:        start=3, end=10
//    North America: start=3, end=11
//  Southern Hemisphere DST is reversed — for simplicity,
//  set DST_START=0 to disable DST for southern cities.
// ============================================================
#define CLOCK_CITY1_LABEL      "Nottingham"
#define CLOCK_CITY1_STD        0
#define CLOCK_CITY1_DST        60
#define CLOCK_CITY1_DST_START  3
#define CLOCK_CITY1_DST_END    10

#define CLOCK_CITY2_LABEL      "Vancouver"
#define CLOCK_CITY2_STD        -480
#define CLOCK_CITY2_DST        -420
#define CLOCK_CITY2_DST_START  3
#define CLOCK_CITY2_DST_END    11

#define CLOCK_CITY3_LABEL      "Ottawa"
#define CLOCK_CITY3_STD        -300
#define CLOCK_CITY3_DST        -240
#define CLOCK_CITY3_DST_START  3
#define CLOCK_CITY3_DST_END    11

#define CLOCK_CITY4_LABEL      "Halifax"
#define CLOCK_CITY4_STD        -240
#define CLOCK_CITY4_DST        -180
#define CLOCK_CITY4_DST_START  3
#define CLOCK_CITY4_DST_END    11
