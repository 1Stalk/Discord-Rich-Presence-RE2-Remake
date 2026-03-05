// ============================================================
// Discord Rich Presence Plugin for Resident Evil 2 Remake (RE2R)
// REFramework Plugin - place in reframework/plugins/
// Reads game state from Lua bridge file for dynamic presence
//
// DEV MODE: Lua script is standalone (reframework/autorun/)
// Use /release workflow to embed Lua back for distribution.
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>

// ============================================================
// Configuration
// ============================================================
static constexpr const char *DISCORD_CLIENT_ID = "1479055945161117736";
static constexpr const char *GAME_NAME = "Resident Evil 2";
static constexpr int UPDATE_INTERVAL_MS = 5000; // 5 seconds

// Path to status file written by Lua bridge script
// Relative to game exe directory
static constexpr const char *STATUS_FILE =
    "reframework\\data\\DiscordPresence\\discord_status.txt";

static constexpr int LUA_VERSION = 1;
static constexpr const char *LUA_VERSION_TAG = "-- DISCORD_PRESENCE_VERSION=1";
static constexpr const char *LUA_FILE =
    "reframework\\autorun\\discord_presence.lua";

static constexpr const char *LUA_SOURCE = R"LUAEOF(
-- DISCORD_PRESENCE_VERSION=1
-- Author: 1Stalk
-- Discord Presence Bridge for Resident Evil 2 Remake

-- ================================================================
-- Paths and poll rate
-- ================================================================
local CONFIG_PATH      = "DiscordPresence/config.ini"
local TRANSLATION_PATH = "DiscordPresence/Discord_Presence_RE2R_Translation.ini"
local STATUS_PATH      = "DiscordPresence/discord_status.txt"
local POLL_INTERVAL    = 150  -- ~2.5 seconds at 60fps

-- Compiled-in default format strings (used when INI is absent or a key is missing)
local fmt_details = "{character} | {status} | {location}"
local fmt_state   = "{difficulty} | {scenario}"

-- ================================================================
-- Default Files Content
-- ================================================================

local DEFAULT_INI = [=====[
; ================================================================
;  Discord Rich Presence - Display Configuration
;  Mod by 1Stalk | Resident Evil 2 Remake
; ================================================================
;
; AVAILABLE VARIABLES:
;   {character}  - Current character:  Leon / Claire / Hunk / Tofu
;   {status}     - Character status:   HP state (Fine, Caution, Danger, Dead)
;   {scenario}   - Current scenario:   Only for "2nd Run"
;   {difficulty} - Game difficulty:    Assisted / Standard / Hardcore
;   {location}   - Current location:   Police Station, Sewers, NEST, etc.
;
;   {hp}         - HP label:           Fine / Caution / Danger / Poison
;
; Leave a line empty to hide it from Discord entirely.
;
; EXAMPLES:
;   details = {character} | {status}
;   details = Playing as {character}
;   state   = {difficulty} | {scenario}
;   state   = {difficulty}
; ================================================================

[display]
details = {character} | {status} | {location}
state   = {difficulty} | {scenario}
]=====]

local DEFAULT_TRANSLATION_INI = [=====[
; ================================================================
;  Discord Rich Presence RE2R — Translation File
; ================================================================
;
; How to use:
;   Replace the text AFTER the = sign with your translation.
;   Do not change the text BEFORE the =.
;   Save the file, then in REFramework click "Reset Scripts".
;
; Example (Russian):
;   Leon       = Леон
;   Claire     = Клэр
;   Fine       = В норме
;   GasStation = Бензоколонка
; ================================================================

; ================================================================

[characters]
Leon     = Leon
Claire   = Claire
Ada      = Ada
Sherry   = Sherry
Hunk     = HUNK
Tofu     = Tofu
Playing  = Playing

[hp_states]
Fine     = Fine
Caution  = Caution
Danger   = Danger
Poison   = Poison
Dead     = Dead

[scenarios]
Leon_B   = 2nd Run
Claire_B = 2nd Run
The_4th_Survivor  = The 4th Survivor
The_Tofu_Survivor = The Tofu Survivor

[difficulties]
Assisted = Assisted
Standard = Standard
Hardcore = Hardcore

[locations]
Invalid = Unknown
CityArea = Streets of Raccoon City
Factory = Factory
Laboratory = NEST Laboratory
Mountain = Arklay Mountains
Opening = Streets of Raccoon City
Orphanage = Orphanage
Police = Police Station
SewagePlant = Sewers
Sewer = Sewers
Playground = Playground
DLC_Laboratory = NEST Laboratory
DLC_Aida = Orphanage
DLC_Hunk = Sewers
Opening2 = Streets of Raccoon City
GasStation = Gas Station
RPD = Police Station
WasteWater = Sewers
WaterPlant = Sewers
EV011 = Cutscene
EV050 = Cutscene
LaboratoryUndermost = NEST Laboratory
Transportation = Transportation
GasStation2 = Gas Station
OrphanAsylum = Orphanage
OrphanApproach = Behind R.P.D.
CrocodiliaArea = Sewers
Title = Main Menu
Movie = Cutscene
RPD_B1 = Underground Facility
Opening3 = Streets of Raccoon City
GameOver = Game Over
Result = Results Screen
Ending = Ending Screen
LOCATION_NUM = Unknown
]=====]

-- ================================================================
-- Utilities
-- ================================================================
local function safe_call(fn)
    local ok, r = pcall(fn)
    if ok then return r end
    return nil
end

-- ================================================================
-- INI Configuration
-- ================================================================
local function init_config()
    local f = io.open(CONFIG_PATH, "r")
    if not f then
        local fw = io.open(CONFIG_PATH, "w")
        if fw then fw:write(DEFAULT_INI); fw:close() end
        return
    end
    -- Parse [display] section
    local in_display = false
    for line in f:lines() do
        local trimmed = line:match("^%s*(.-)%s*$") or ""
        if trimmed:sub(1,1) == ";" or trimmed == "" then
            -- ignore
        elseif trimmed:lower():match("^%[display%]") then
            in_display = true
        elseif trimmed:match("^%[") then
            in_display = false
        elseif in_display then
            local k, v = trimmed:match("^(%a+)%s*=%s*(.*)$")
            if k == "details" and v ~= nil then fmt_details = v end
            if k == "state"   and v ~= nil then fmt_state   = v end
        end
    end
    f:close()
end

-- ================================================================
-- Location Names Mapping
-- ================================================================
local LOCATION_NAMES = {
    [0]  = "Invalid",
    [1]  = "CityArea",
    [2]  = "Factory",
    [3]  = "Laboratory",
    [4]  = "Mountain",
    [5]  = "Opening",
    [6]  = "Orphanage",
    [7]  = "Police",
    [8]  = "SewagePlant",
    [9]  = "Sewer",
    [10] = "Playground",
    [11] = "DLC_Laboratory",
    [12] = "DLC_Aida",
    [13] = "DLC_Hunk",
    [14] = "Opening2",
    [15] = "GasStation",
    [16] = "RPD",
    [17] = "WasteWater",
    [18] = "WaterPlant",
    [19] = "EV011",
    [20] = "EV050",
    [21] = "LaboratoryUndermost",
    [22] = "Transportation",
    [23] = "GasStation2",
    [24] = "OrphanAsylum",
    [25] = "OrphanApproach",
    [26] = "CrocodiliaArea",
    [27] = "Title",
    [28] = "Movie",
    [29] = "RPD_B1",
    [30] = "Opening3",
    [31] = "GameOver",
    [32] = "Result",
    [33] = "Ending",
    [34] = "LOCATION_NUM"
}

-- ================================================================
-- Translation System
-- ================================================================
local T = {}

local function init_translations()
    local f = io.open(TRANSLATION_PATH, "r")
    if not f then
        local fw = io.open(TRANSLATION_PATH, "w")
        if fw then fw:write(DEFAULT_TRANSLATION_INI); fw:close() end
        f = io.open(TRANSLATION_PATH, "r")
        if not f then return end -- failed to create
    end

    for line in f:lines() do
        local trimmed = line:match("^%s*(.-)%s*$") or ""
        if trimmed:sub(1,1) ~= ";" and trimmed ~= "" and not trimmed:match("^%[") then
            -- key = value
            local k, v = trimmed:match("^([^=]+)=(.*)$")
            if k and v then
                local key = k:match("^%s*(.-)%s*$")
                local val = v:match("^%s*(.-)%s*$")
                T[key] = val
            end
        end
    end
    f:close()
end

-- Translation helper: returns translated string, or original if not found
local function tr(key)
    return T[key] or key
end

-- ================================================================
-- Status file writer (new format: two plain lines)
-- ================================================================
local last_written = nil

local function write_mainmenu()
    if last_written == "mainmenu" then return end
    last_written = "mainmenu"
    local f = io.open(STATUS_PATH, "w")
    if f then f:write("mainmenu"); f:close() end
end

local function write_status_lines(details, state)
    details  = (details or ""):match("^%s*(.-)%s*$")
    state    = (state   or ""):match("^%s*(.-)%s*$")
    
    local content = details .. "\n" .. state
    
    if content == last_written then return end
    last_written = content
    local f = io.open(STATUS_PATH, "w")
    if f then f:write(content); f:close() end
end

-- ================================================================
-- Template expansion: replaces {variable} with value from vars table
-- ================================================================
local function expand(template, vars)
    if not template or template == "" then return "" end
    return (template:gsub("{(%w+)}", function(key)
        return vars[key] or ""
    end))
end

-- ================================================================
-- Game data: Character detection
-- ================================================================
local function get_character()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        local stype = safe_call(function() return mfm:call("get_LoadSurvivorType()") end)
        if stype ~= nil then
            local t = tonumber(stype)
            if t == 0 then return "Leon" end
            if t == 1 then return "Claire" end
            if t == 2 then return "Ada" end
            if t == 3 then return "Sherry" end
        end
        
        -- Fallback to properties if SurvivorType is not one of the main 4 (e.g. for Hunk/Tofu)
        if safe_call(function() return mfm:call("get_IsLeon()") end) then return "Leon" end
        if safe_call(function() return mfm:call("get_IsClaire()") end) then return "Claire" end
        if safe_call(function() return mfm:call("get_IsHunk()") end) then return "Hunk" end
        if safe_call(function() return mfm:call("get_IsTofu()") end) then return "Tofu" end
    end
    return nil
end

-- ================================================================
-- Game data: Scenario detection
-- ================================================================
local function get_scenario()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        if safe_call(function() return mfm:call("get_IsLeonB()") end) then return "Leon_B" end
        if safe_call(function() return mfm:call("get_IsClaireB()") end) then return "Claire_B" end
        if safe_call(function() return mfm:call("get_IsHunk()") end) then return "The_4th_Survivor" end
        if safe_call(function() return mfm:call("get_IsTofu()") end) then return "The_Tofu_Survivor" end
    end
    return nil
end

-- ================================================================
-- Game data: Difficulty detection
-- ================================================================
local function get_difficulty()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        local diff = safe_call(function() return mfm:call("get_CurrentDifficulty()") end)
        if tostring(diff) == "0" then return "Assisted" end
        if tostring(diff) == "1" then return "Standard" end
        if tostring(diff) == "2" then return "Hardcore" end
    end
    return nil
end

-- ================================================================
-- Game data: Location detection
-- ================================================================
local LOCATION_NAMES = {
    [0]="Invalid", [1]="CityArea", [2]="Factory", [3]="Laboratory", [4]="Mountain", [5]="Opening",
    [6]="Orphanage", [7]="Police", [8]="SewagePlant", [9]="Sewer", [10]="Playground", [11]="DLC_Laboratory",
    [12]="DLC_Aida", [13]="DLC_Hunk", [14]="Opening2", [15]="GasStation", [16]="RPD", [17]="WasteWater",
    [18]="WaterPlant", [19]="EV011", [20]="EV050", [21]="LaboratoryUndermost", [22]="Transportation",
    [23]="GasStation2", [24]="OrphanAsylum", [25]="OrphanApproach", [26]="CrocodiliaArea", [27]="Title",
    [28]="Movie", [29]="RPD_B1", [30]="Opening3", [31]="GameOver", [32]="Result", [33]="Ending", [34]="LOCATION_NUM"
}

local function get_location()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        local loc_id = safe_call(function() return mfm:call("get_LoadLocation()") end)
        if loc_id ~= nil then
            local loc_num = tonumber(loc_id)
            if loc_num and LOCATION_NAMES[loc_num] then
                return LOCATION_NAMES[loc_num]
            end
        end
    end
    return nil
end

-- ================================================================
-- Game data: HP status
-- ================================================================
local function get_hp_status()
    local pm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.PlayerManager") end)
    if pm then
        local is_dead = safe_call(function() return pm:call("get_IsDead()") end)
        if is_dead then return "Dead" end
        
        local ratio = safe_call(function() return pm:call("get_CurrentHPRatio()") end)
        if ratio then
            if ratio > 0.66 then return "Fine" end
            if ratio > 0.33 then return "Caution" end
            return "Danger"
        end
    end
    return nil
end

-- ================================================================
-- Game data: In-game check
-- ================================================================
local function is_in_game()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        return safe_call(function() return mfm:call("get_IsInGame()") end) == true
    end
    return false
end

-- ================================================================
-- Game data: Location detection
-- ================================================================
local function get_location()
    local mfm = safe_call(function() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end)
    if mfm then
        local loc_id = safe_call(function() return mfm:call("get_LoadLocation()") end)
        if loc_id ~= nil then
            local loc_num = tonumber(loc_id)
            if loc_num and LOCATION_NAMES[loc_num] then
                return LOCATION_NAMES[loc_num]
            end
        end
    end
    return nil
end

-- ================================================================
-- Init: load config and translations once on script startup
-- ================================================================
init_config()
init_translations()

-- ================================================================
-- Main poll loop
-- ================================================================
local frame_count = 0

re.on_pre_application_entry("UpdateScene", function()
    frame_count = frame_count + 1
    if frame_count < POLL_INTERVAL then return end
    frame_count = 0

    -- Main menu check
    if not is_in_game() then
        write_mainmenu()
        return
    end

    -- Get game data
    local character  = get_character()
    local scenario   = get_scenario()
    local difficulty = get_difficulty()
    local hp_status  = get_hp_status()
    local location   = get_location()

    -- Build display strings
    local char_name  = character and tr(character) or tr("Playing")
    local status_str = hp_status and tr(hp_status) or ""
    local scen_str   = scenario  and tr(scenario)  or ""
    local diff_str   = difficulty and tr(difficulty) or ""
    local loc_str    = location  and tr(location)  or ""

    -- Build template variable table
    local vars = {
        character  = char_name,
        status     = status_str,
        scenario   = scen_str,
        difficulty = diff_str,
        location   = loc_str,
        hp         = hp_status and tr(hp_status) or "",
    }

    local function clean_seps(s)
        return s:gsub("^%s*[|%-]%s*", ""):gsub("%s*[|%-]%s*$", ""):gsub("%s*([|%-])%s*%1%s*", " %1 ")
    end

    write_status_lines(clean_seps(expand(fmt_details, vars)), clean_seps(expand(fmt_state, vars)))
end)

)LUAEOF";

static std::string get_lua_path() {
  char exe_dir[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
  char *sep = strrchr(exe_dir, '\\');
  if (sep)
    *(sep + 1) = '\0';
  return std::string(exe_dir) + LUA_FILE;
}

static bool lua_needs_update(const std::string &path) {
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return true;
  char buf[64] = {};
  DWORD n = 0;
  DWORD tag_len = static_cast<DWORD>(strlen(LUA_VERSION_TAG));
  ReadFile(h, buf, tag_len, &n, nullptr);
  CloseHandle(h);
  return (n < tag_len) || (strncmp(buf, LUA_VERSION_TAG, tag_len) != 0);
}

static void write_lua_file(const std::string &path) {
  std::string dir = path.substr(0, path.rfind('\\'));
  CreateDirectoryA(dir.c_str(), nullptr);
  HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return;
  DWORD src_len = static_cast<DWORD>(strlen(LUA_SOURCE));
  DWORD written = 0;
  WriteFile(h, LUA_SOURCE, src_len, &written, nullptr);
  CloseHandle(h);
}

static void extract_lua_if_needed() {
  char exe_dir[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
  char *sep = strrchr(exe_dir, '\\');
  if (sep)
    *(sep + 1) = '\0';
  std::string base(exe_dir);
  CreateDirectoryA((base + "reframework\\data\\DiscordPresence").c_str(),
                   nullptr);
  std::string lua_path = base + LUA_FILE;
  if (lua_needs_update(lua_path)) {
    write_lua_file(lua_path);
  }
}

static std::string read_status_file() {
  char exe_dir[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
  char *last_sep = strrchr(exe_dir, '\\');
  if (last_sep)
    *(last_sep + 1) = '\0';

  std::string path = std::string(exe_dir) + STATUS_FILE;

  HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hFile == INVALID_HANDLE_VALUE)
    return "";

  char buf[512] = {};
  DWORD bytesRead = 0;
  ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
  CloseHandle(hFile);

  return std::string(buf, bytesRead);
}

// ============================================================
// Presence state
// ============================================================

struct PresenceState {
  const char *details;
  const char *state;
};

static std::string g_details_buf;
static std::string g_state_buf;

static PresenceState get_presence_from_status(const std::string &content) {
  if (content.empty() || content == "mainmenu")
    return {nullptr, "Main Menu"};

  auto nl = content.find('\n');
  if (nl == std::string::npos) {
    g_state_buf = content;
    while (!g_state_buf.empty() &&
           (g_state_buf.back() == '\r' || g_state_buf.back() == ' '))
      g_state_buf.pop_back();
    return {nullptr, g_state_buf.c_str()};
  }

  g_details_buf = content.substr(0, nl);
  g_state_buf = content.substr(nl + 1);

  auto trim_cr = [](std::string &s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
      s.pop_back();
    size_t pos = 0;
    while ((pos = s.find('\r', pos)) != std::string::npos) {
      s.erase(pos, 1);
    }
  };
  trim_cr(g_details_buf);
  trim_cr(g_state_buf);

  const char *details = g_details_buf.empty() ? nullptr : g_details_buf.c_str();
  const char *state = g_state_buf.empty() ? "Main Menu" : g_state_buf.c_str();
  return {details, state};
}

// ============================================================
// Discord IPC - Named Pipe Protocol
// Each packet: [op:uint32][len:uint32][json:len bytes]
// ============================================================

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static int g_nonce = 0;
static std::atomic<bool> g_running{false};
static std::thread g_thread;

static bool ipc_write(HANDLE pipe, uint32_t op, const char *json,
                      uint32_t json_len) {
  uint8_t hdr[8];
  memcpy(hdr, &op, 4);
  memcpy(hdr + 4, &json_len, 4);
  DWORD written = 0;
  if (!WriteFile(pipe, hdr, 8, &written, nullptr) || written != 8)
    return false;
  if (!WriteFile(pipe, json, json_len, &written, nullptr) ||
      written != json_len)
    return false;
  return true;
}

static bool ipc_read(HANDLE pipe, uint32_t &op, std::string &json) {
  uint8_t hdr[8] = {};
  DWORD n = 0;
  if (!ReadFile(pipe, hdr, 8, &n, nullptr) || n != 8)
    return false;

  memcpy(&op, hdr, 4);
  uint32_t len = 0;
  memcpy(&len, hdr + 4, 4);

  if (len == 0 || len > 65536) {
    json.clear();
    return true;
  }
  json.resize(len);
  if (!ReadFile(pipe, &json[0], len, &n, nullptr) || n != len)
    return false;
  return true;
}

static void ipc_drain(HANDLE pipe) {
  DWORD avail = 0;
  while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr) &&
         avail >= 8) {
    uint32_t op = 0;
    std::string tmp;
    if (!ipc_read(pipe, op, tmp))
      break;
  }
}

static HANDLE discord_connect() {
  for (int i = 0; i <= 9; i++) {
    std::string path = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
    WaitNamedPipeA(path.c_str(), 1000);

    HANDLE pipe = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
      continue;

    std::string hs = "{\"v\":1,\"client_id\":\"";
    hs += DISCORD_CLIENT_ID;
    hs += "\"}";
    if (!ipc_write(pipe, 0, hs.c_str(), static_cast<uint32_t>(hs.size()))) {
      CloseHandle(pipe);
      continue;
    }

    uint32_t resp_op = 0;
    std::string resp_json;
    if (!ipc_read(pipe, resp_op, resp_json)) {
      CloseHandle(pipe);
      continue;
    }

    return pipe;
  }
  return INVALID_HANDLE_VALUE;
}

static std::string json_escape(const char *s) {
  std::string out;
  for (; *s; ++s) {
    if (*s == '"')
      out += "\\\"";
    else if (*s == '\\')
      out += "\\\\";
    else if (*s == '\n')
      out += "\\n";
    else if (*s == '\r')
      out += "\\r";
    else if (*s == '\t')
      out += "\\t";
    else
      out += *s;
  }
  return out;
}

static bool discord_set_activity(HANDLE pipe, const char *details,
                                 const char *state, int64_t start_ts) {
  ++g_nonce;

  char json[1024];
  int len;

  if (details && *details) {
    std::string details_esc = json_escape(details);
    std::string state_esc = json_escape(state);
    len = snprintf(json, sizeof(json),
                   "{"
                   "\"cmd\":\"SET_ACTIVITY\","
                   "\"args\":{"
                   "\"pid\":%lu,"
                   "\"activity\":{"
                   "\"details\":\"%s\","
                   "\"state\":\"%s\","
                   "\"timestamps\":{\"start\":%lld}"
                   "}"
                   "},"
                   "\"nonce\":\"%d\""
                   "}",
                   static_cast<unsigned long>(GetCurrentProcessId()),
                   details_esc.c_str(), state_esc.c_str(),
                   static_cast<long long>(start_ts), g_nonce);
  } else {
    std::string state_esc = json_escape(state);
    len =
        snprintf(json, sizeof(json),
                 "{"
                 "\"cmd\":\"SET_ACTIVITY\","
                 "\"args\":{"
                 "\"pid\":%lu,"
                 "\"activity\":{"
                 "\"state\":\"%s\","
                 "\"timestamps\":{\"start\":%lld}"
                 "}"
                 "},"
                 "\"nonce\":\"%d\""
                 "}",
                 static_cast<unsigned long>(GetCurrentProcessId()),
                 state_esc.c_str(), static_cast<long long>(start_ts), g_nonce);
  }

  if (len <= 0 || len >= static_cast<int>(sizeof(json)))
    return false;

  ipc_drain(pipe);
  if (!ipc_write(pipe, 1, json, static_cast<uint32_t>(len)))
    return false;

  uint32_t resp_op = 0;
  std::string resp;
  if (!ipc_read(pipe, resp_op, resp))
    return false;

  return true;
}

// ============================================================
// Background Worker Thread
// ============================================================

static void discord_thread_func() {
  const int64_t session_start = static_cast<int64_t>(time(nullptr));
  std::string last_status = "";

  while (g_running) {
    if (g_pipe == INVALID_HANDLE_VALUE) {
      g_pipe = discord_connect();
      if (g_pipe == INVALID_HANDLE_VALUE) {
        for (int i = 0; i < 150 && g_running; i++)
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      last_status = "__force__";
    }

    std::string status = read_status_file();

    if (status != last_status) {
      PresenceState presence = get_presence_from_status(status);

      if (!discord_set_activity(g_pipe, presence.details, presence.state,
                                session_start)) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
        continue;
      }
      last_status = status;
    }

    const int chunks = UPDATE_INTERVAL_MS / 100;
    for (int i = 0; i < chunks && g_running; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (g_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
  }
}

// ============================================================
// REFramework Plugin Entry Point
// ============================================================
struct REFrameworkPluginInitializeParam;

extern "C" {
__declspec(dllexport) bool
reframework_plugin_initialize(const REFrameworkPluginInitializeParam *) {
  extract_lua_if_needed();

  g_running = true;
  g_thread = std::thread(discord_thread_func);
  return true;
}
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_DETACH && g_running) {
    g_running = false;
  }
  return TRUE;
}
