#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <shlobj.h>
#include <dwmapi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

namespace fs = std::filesystem;

namespace Cfg {
    static const std::vector<std::string> BL = {
        "meteor-client","meteor","wurst","xray","freecam",
        "radium","impact","glazed","autototem","crystal",
        "clickcrystal","zink","liquidbounce","aristois",
        "wolfram","sigma","ghost","inertia","rise","flux",
        "future","novoline","rusherhack","killaura",
        "aimbot","nofall","scaffold","antiknockback",
        "speedhack","bhop","reach","spoofer",
    };
}

static const int WW = 900;
static const int WH = 630;

static fs::path GetResultPath() {
    char ap[MAX_PATH] = {};
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, ap);
    fs::path dir = fs::path(ap) / "SlowfizScanner";
    fs::create_directories(dir);
    return dir / "result.txt";
}

static std::string GetDailyCode() {
    time_t t = time(nullptr); tm lt{};
    localtime_s(&lt, &t);
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%02d%02d%04d",
        lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
    std::string s(buf);
    std::reverse(s.begin(), s.end());
    return s;
}

namespace Col {
    static ImVec4 BG = { 0.05f,0.06f,0.08f,1.0f };
    static ImVec4 PANEL = { 0.08f,0.10f,0.13f,1.0f };
    static ImVec4 CARD = { 0.10f,0.13f,0.17f,1.0f };
    static ImVec4 CARD2 = { 0.13f,0.16f,0.21f,1.0f };
    static ImVec4 LINE = { 0.15f,0.20f,0.27f,1.0f };
    static ImVec4 ACC = { 0.22f,0.56f,1.00f,1.0f };
    static ImVec4 ACCH = { 0.36f,0.70f,1.00f,1.0f };
    static ImVec4 GREEN = { 0.18f,0.82f,0.44f,1.0f };
    static ImVec4 YELLOW = { 1.00f,0.80f,0.18f,1.0f };
    static ImVec4 RED = { 0.96f,0.26f,0.26f,1.0f };
    static ImVec4 ORANGE = { 1.00f,0.58f,0.16f,1.0f };
    static ImVec4 DIM = { 0.44f,0.50f,0.60f,1.0f };
    static ImVec4 TEXT = { 0.90f,0.93f,0.97f,1.0f };
    static ImVec4 BTN = { 0.14f,0.40f,0.86f,1.0f };
}

enum class Risk { NONE, CLEAN, MAYBE, EVIDENCE };

struct Finding {
    std::string launcher, type, detail, path;
    bool certain;
};

struct ScanResult {
    Risk   risk = Risk::NONE;
    std::string username;
    std::vector<Finding> findings;
    bool   hitSlowfiz = false, hitModBlock = false, hitModFile = false;
    std::string summary;
};

enum class Screen { TUTORIAL, IGN, MAIN, RESCAN_CODE, SCREENSHOT };

struct {
    Screen screen = Screen::TUTORIAL;
    int    tutPage = 0;

    char   ign[48] = {};
    bool   ignSet = false;

    bool   consented = false;
    bool   scanning = false;
    bool   done = false;
    float  prog = 0.0f;
    std::string progMsg;
    ScanResult  result;
    std::mutex  mtx;

    std::vector<std::string> log;
    bool scrollLog = false;

    bool   autoTab = false;

    char   codeInput[32] = {};
    bool   codeFail = false;

    bool   showCfg = false;
    float  t = 0.0f;
} G;

static std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s;
}
static bool CI(const std::string& h, const std::string& n) {
    return Lower(h).find(Lower(n)) != std::string::npos;
}
static void AddLog(const std::string& s) {
    std::lock_guard<std::mutex> lk(G.mtx); G.log.push_back(s); G.scrollLog = true;
}
static std::string ReadFileTxt(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); if (!f) return "";
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
static std::string Env(const char* v) {
    char b[MAX_PATH] = {}; DWORD n = GetEnvironmentVariableA(v, b, MAX_PATH);
    return n ? std::string(b) : "";
}

static std::string RiskLabel(Risk r) {
    switch (r) {
    case Risk::EVIDENCE: return "EVIDENCE WITHOUT DOUBT";
    case Risk::MAYBE:    return "MAYBE FLAGS";
    case Risk::CLEAN:    return "CLEAN";
    default:             return "UNKNOWN";
    }
}

static void SaveResult(const ScanResult& r) {
    fs::path p = GetResultPath();
    std::ofstream f(p);
    if (!f) return;

    time_t t = time(nullptr); tm lt{}; localtime_s(&lt, &t);
    char db[32] = {};
    snprintf(db, sizeof(db), "%02d.%02d.%04d %02d:%02d",
        lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min);

    f << "SLOWFIZ SCANNER — SCAN RESULT\n";
    f << "==============================\n";
    f << "Date:     " << db << "\n";
    f << "IGN:      " << G.ign << "\n";
    f << "Username: " << (r.username.empty() ? "Unknown" : r.username) << "\n";
    f << "Verdict:  " << RiskLabel(r.risk) << "\n";
    f << "Summary:  " << r.summary << "\n";
    f << "Slowfiz:  " << (r.hitSlowfiz ? "Connected" : "Not Detected") << "\n";
    f << "Findings: " << r.findings.size() << "\n";
    f << "------------------------------\n";
    for (auto& fi : r.findings) {
        f << "[" << fi.type << "] " << (fi.certain ? "EVIDENCE" : "MAYBE") << " | "
            << fi.launcher << " | " << fi.detail << "\n";
    }
    f << "==============================\n";
    f << "SCANNED\n";
}

static bool LoadPrevResult() {
    std::string raw = ReadFileTxt(GetResultPath());
    if (raw.empty() || raw.find("SCANNED") == std::string::npos) return false;

    auto field = [&](const char* key)->std::string {
        auto pos = raw.find(key);
        if (pos == std::string::npos) return "";
        pos += strlen(key);
        size_t end = raw.find('\n', pos);
        std::string v = raw.substr(pos, end - pos);
        while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
        return v;
        };

    strncpy_s(G.ign, field("IGN:      ").c_str(), sizeof(G.ign) - 1);
    G.ignSet = true;

    ScanResult r;
    r.username = field("Username: ");
    r.summary = field("Summary:  ");
    r.hitSlowfiz = (field("Slowfiz:  ") == "Connected");
    std::string vrd = field("Verdict:  ");
    if (vrd == "EVIDENCE WITHOUT DOUBT") r.risk = Risk::EVIDENCE;
    else if (vrd == "MAYBE FLAGS")       r.risk = Risk::MAYBE;
    else if (vrd == "CLEAN")             r.risk = Risk::CLEAN;
    else r.risk = Risk::NONE;

    std::istringstream ss(raw); std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] != '[') continue;
        size_t tb = line.find(']');
        if (tb == std::string::npos) continue;
        std::string type = line.substr(1, tb - 1);
        std::string rest = line.substr(tb + 2);
        size_t p1 = rest.find(" | ");
        if (p1 == std::string::npos) continue;
        std::string verdict2 = rest.substr(0, p1); rest = rest.substr(p1 + 3);
        size_t p2 = rest.find(" | ");
        if (p2 == std::string::npos) continue;
        std::string launcher = rest.substr(0, p2);
        std::string detail = rest.substr(p2 + 3);
        while (!detail.empty() && (detail.back() == '\r' || detail.back() == '\n')) detail.pop_back();
        r.findings.push_back({ launcher,type,detail,"",verdict2 == "EVIDENCE" });
    }

    G.result = r;
    G.done = true;
    return true;
}

static std::vector<std::string> ExtractModBlock(const std::string& raw) {
    std::vector<std::string> mods;
    std::string marker = "]: Loading ";
    size_t pos = raw.find(marker);
    while (pos != std::string::npos) {
        size_t numEnd = raw.find(" mods:", pos + marker.size());
        if (numEnd == std::string::npos) { pos = raw.find(marker, pos + 1); continue; }
        size_t lineEnd = raw.find('\n', numEnd);
        if (lineEnd == std::string::npos) break;
        size_t cur = lineEnd + 1;
        while (cur < raw.size()) {
            size_t next = raw.find('\n', cur);
            if (next == std::string::npos) next = raw.size();
            std::string line = raw.substr(cur, next - cur);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t fs2 = line.find_first_not_of(" \t");
            if (fs2 == std::string::npos) { cur = next + 1; continue; }
            std::string tr = line.substr(fs2);
            if (!tr.empty() && tr[0] == '[') break;
            if (tr.size() > 2 && tr[0] == '-' && tr[1] == ' ') {
                std::string rest = tr.substr(2);
                size_t sp = rest.find(' ');
                std::string mid = (sp != std::string::npos) ? rest.substr(0, sp) : rest;
                if (!mid.empty()) mods.push_back(Lower(mid));
            }
            cur = next + 1;
        }
        pos = raw.find(marker, pos + 1);
    }
    return mods;
}

static bool IsChatLine(const std::string& line) {
    return (CI(line, "[Render thread/INFO]") && CI(line, "[CHAT]")) ||
        (CI(line, "/INFO]") && CI(line, "[CHAT]"));
}

static void DoScan() {
    auto setP = [](float p, const std::string& m) {
        std::lock_guard<std::mutex> lk(G.mtx); G.prog = p; G.progMsg = m; };

    ScanResult r; r.risk = Risk::CLEAN;
    auto A = Env("APPDATA"), U = Env("USERPROFILE");

    struct L { std::string name, base; };
    std::vector<L> launchers = {
        {"Official Minecraft",  A + "\\.minecraft"},
        {"Modrinth",            A + "\\ModrinthApp\\profiles"},
        {"CurseForge",          U + "\\curseforge\\minecraft\\Instances"},
        {"Feather Client",      A + "\\.feather\\instances"},
        {"NoRisk Client",       A + "\\.norisk"},
        {"Prism Launcher",      A + "\\PrismLauncher\\instances"},
        {"MultiMC",             A + "\\MultiMC\\instances"},
        {"Lunar Client",        U + "\\.lunarclient\\offline\\multiver"},
        {"Badlion Client",      A + "\\.minecraft"},
    };

    std::vector<std::pair<std::string, fs::path>> dirs;
    for (auto& l : launchers) {
        fs::path base(l.base); std::error_code ec;
        if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) continue;
        if (fs::exists(base / "logs", ec) || fs::exists(base / "mods", ec))
            dirs.push_back({ l.name,base });
        for (fs::recursive_directory_iterator it(base,
            fs::directory_options::skip_permission_denied, ec);
            it != fs::recursive_directory_iterator(); ++it) {
            if (ec) { ec.clear(); continue; }
            if (it.depth() > 6) { it.pop(); continue; }
            if (!it->is_directory()) continue;
            fs::path p = it->path();
            if (fs::exists(p / "logs", ec) || fs::exists(p / "mods", ec))
                dirs.push_back({ l.name + " [" + p.filename().string() + "]",p });
        }
    }
    std::sort(dirs.begin(), dirs.end(), [](auto& a, auto& b) {return a.second < b.second; });
    dirs.erase(std::unique(dirs.begin(), dirs.end(),
        [](auto& a, auto& b) {return a.second == b.second; }), dirs.end());

    setP(0.05f, "Locating installations...");
    AddLog("[INFO] Scan started — IGN: " + std::string(G.ign));
    AddLog("[INFO] " + std::to_string(dirs.size()) + " installation(s) found.");

    if (dirs.empty()) {
        AddLog("[WARN] No Minecraft installations found.");
        setP(1.0f, "Done — no installations found.");
        std::lock_guard<std::mutex> lk(G.mtx);
        G.result = r; G.scanning = false; G.done = true; G.autoTab = true; return;
    }

    float step = 0.88f / (float)std::max((int)dirs.size(), 1);
    float prog = 0.06f;

    for (auto& [launcher, dir] : dirs) {
        AddLog("[SCAN] " + launcher + " → " + dir.string());
        setP(prog, "Scanning " + launcher + "...");

        fs::path ld = dir / "logs";
        if (fs::exists(ld)) {
            std::error_code ec;
            for (auto& e : fs::recursive_directory_iterator(ld, ec)) {
                if (!e.is_regular_file()) continue;
                auto ext = Lower(e.path().extension().string());
                if (ext != ".log" && ext != ".txt") continue;
                std::string raw = ReadFileTxt(e.path()); if (raw.empty()) continue;

                if (CI(raw, "slowfiz.net") || CI(raw, "Connecting to slowfiz")) {
                    r.hitSlowfiz = true;
                    AddLog("[MATCH] slowfiz.net connection → " + e.path().filename().string());
                    r.findings.push_back({ launcher,"LOG","Connection to slowfiz.net",
                        e.path().string(),false });
                }
                if (r.username.empty()) {
                    for (const char* pat : { "Logged in as ","Setting user: ","Username: " }) {
                        auto pos = raw.find(pat);
                        if (pos != std::string::npos) {
                            size_t s = pos + strlen(pat), end = raw.find_first_of(" \r\n", s);
                            if (end == std::string::npos) end = raw.size();
                            r.username = raw.substr(s, end - s);
                            if (!r.username.empty()) { AddLog("[INFO] Username: " + r.username); break; }
                        }
                    }
                }

                auto modBlock = ExtractModBlock(raw);
                if (!modBlock.empty()) {
                    AddLog("[INFO] Mod block: " + std::to_string(modBlock.size()) + " mod(s) — " + e.path().filename().string());
                    for (auto& modId : modBlock) {
                        for (auto& bl : Cfg::BL) {
                            if (CI(modId, bl)) {
                                r.hitModBlock = true;
                                bool inChat = false;
                                std::istringstream ls(raw); std::string ln;
                                while (std::getline(ls, ln)) {
                                    if (IsChatLine(ln) && CI(ln, modId)) {
                                        inChat = true;
                                        AddLog("[INFO] Ignoring '" + modId + "' — found in chat");
                                        break;
                                    }
                                }
                                if (!inChat) {
                                    std::string det = "Mod '" + modId + "' matched '" + bl + "' in mod block";
                                    AddLog("[ALERT] " + det);
                                    r.findings.push_back({ launcher,"MOD_LOG",det,
                                        e.path().string(),false });
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        fs::path md = dir / "mods";
        if (fs::exists(md)) {
            std::error_code ec;
            for (auto& e : fs::recursive_directory_iterator(md, ec)) {
                if (!e.is_regular_file()) continue;
                std::string fn = Lower(e.path().filename().string());
                for (auto& bl : Cfg::BL) {
                    if (CI(fn, bl)) {
                        r.hitModFile = true;
                        std::string det = "Mod file: " + e.path().filename().string();
                        AddLog("[ALERT] " + det);
                        r.findings.push_back({ launcher,"MOD_FILE",det,
                            e.path().string(),false });
                    }
                }
            }
        }
        prog += step; setP(prog, launcher + " — complete");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto& f : r.findings)
        if (f.type == "MOD_LOG") f.certain = r.hitSlowfiz;

    bool anyModLog = std::any_of(r.findings.begin(), r.findings.end(),
        [](const Finding& f) {return f.type == "MOD_LOG"; });

    if (anyModLog && r.hitSlowfiz) {
        r.risk = Risk::EVIDENCE;
        r.summary = "Blacklisted mod in logs AND slowfiz.net connection confirmed.";
    }
    else if (r.hitModBlock || r.hitModFile) {
        r.risk = Risk::MAYBE;
        r.summary = "Suspicious mod(s) found — no definitive evidence.";
    }
    else if (r.hitSlowfiz) {
        r.risk = Risk::MAYBE;
        r.summary = "Connection to slowfiz.net detected — no blacklisted mods.";
    }
    else {
        r.risk = Risk::CLEAN;
        r.summary = "No suspicious mods or connections found.";
    }

    AddLog("[DONE] " + r.summary);
    setP(1.0f, "Scan complete.");
    {
        std::lock_guard<std::mutex> lk(G.mtx);
        G.result = r; G.scanning = false; G.done = true; G.autoTab = true;
    }

    SaveResult(r);
}

static void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12; s.ChildRounding = 10; s.FrameRounding = 7;
    s.PopupRounding = 10;  s.ScrollbarRounding = 7; s.GrabRounding = 6;
    s.TabRounding = 7;     s.WindowBorderSize = 0; s.FrameBorderSize = 0;
    s.ChildBorderSize = 1; s.ItemSpacing = { 8,8 }; s.FramePadding = { 12,8 };
    s.WindowPadding = { 18,15 }; s.ScrollbarSize = 8; s.GrabMinSize = 6;
    s.TabBarBorderSize = 0;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = Col::BG;
    c[ImGuiCol_ChildBg] = Col::PANEL;
    c[ImGuiCol_PopupBg] = Col::PANEL;
    c[ImGuiCol_Border] = Col::LINE;
    c[ImGuiCol_FrameBg] = Col::CARD;
    c[ImGuiCol_FrameBgHovered] = Col::CARD2;
    c[ImGuiCol_FrameBgActive] = { 0.18f,0.26f,0.38f,1 };
    c[ImGuiCol_TitleBg] = Col::BG;
    c[ImGuiCol_TitleBgActive] = Col::BG;
    c[ImGuiCol_TitleBgCollapsed] = Col::BG;
    c[ImGuiCol_ScrollbarBg] = Col::BG;
    c[ImGuiCol_ScrollbarGrab] = { 0.18f,0.24f,0.34f,1 };
    c[ImGuiCol_ScrollbarGrabHovered] = Col::ACC;
    c[ImGuiCol_ScrollbarGrabActive] = Col::ACCH;
    c[ImGuiCol_CheckMark] = Col::ACC;
    c[ImGuiCol_SliderGrab] = Col::ACC;
    c[ImGuiCol_SliderGrabActive] = Col::ACCH;
    c[ImGuiCol_Button] = Col::BTN;
    c[ImGuiCol_ButtonHovered] = { 0.22f,0.52f,0.98f,1 };
    c[ImGuiCol_ButtonActive] = { 0.12f,0.36f,0.76f,1 };
    c[ImGuiCol_Header] = { 0.16f,0.42f,0.84f,0.28f };
    c[ImGuiCol_HeaderHovered] = { 0.16f,0.42f,0.84f,0.48f };
    c[ImGuiCol_HeaderActive] = Col::ACC;
    c[ImGuiCol_Separator] = Col::LINE;
    c[ImGuiCol_Tab] = { 0.08f,0.10f,0.13f,1 };
    c[ImGuiCol_TabHovered] = { 0.16f,0.42f,0.84f,0.7f };
    c[ImGuiCol_TabActive] = { 0.14f,0.40f,0.86f,1 };
    c[ImGuiCol_TabUnfocused] = Col::BG;
    c[ImGuiCol_TabUnfocusedActive] = { 0.12f,0.34f,0.70f,1 };
    c[ImGuiCol_Text] = Col::TEXT;
    c[ImGuiCol_TextDisabled] = Col::DIM;
    c[ImGuiCol_PlotHistogram] = Col::ACC;
    c[ImGuiCol_PlotHistogramHovered] = Col::ACCH;
    c[ImGuiCol_TableHeaderBg] = { 0.10f,0.14f,0.20f,1 };
    c[ImGuiCol_TableBorderStrong] = Col::LINE;
    c[ImGuiCol_TableBorderLight] = { 0.11f,0.15f,0.22f,1 };
    c[ImGuiCol_TableRowBg] = { 0,0,0,0 };
    c[ImGuiCol_TableRowBgAlt] = { 0.08f,0.10f,0.14f,0.7f };
}

static void Centered(const char* txt, ImVec4 col, float scale = 1.0f) {
    if (scale != 1) ImGui::SetWindowFontScale(scale);
    float w = ImGui::CalcTextSize(txt).x * scale;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - w) * 0.5f);
    ImGui::TextColored(col, "%s", txt);
    if (scale != 1) ImGui::SetWindowFontScale(1.0f);
}
static void AccentLine() {
    ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(p, { p.x + w,p.y + 1 },
        IM_COL32(28, 98, 210, 200), IM_COL32(58, 150, 255, 65),
        IM_COL32(58, 150, 255, 65), IM_COL32(28, 98, 210, 200));
    ImGui::Dummy({ 0,3 });
}
static bool WideBtn(const char* label, ImVec4 col, float h = 40) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        { col.x + 0.09f,col.y + 0.09f,col.z + 0.09f,1 });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        { col.x - 0.06f,col.y - 0.06f,col.z - 0.06f,1 });
    bool r = ImGui::Button(label, { ImGui::GetContentRegionAvail().x,h });
    ImGui::PopStyleColor(3); return r;
}
static bool SecondaryBtn(const char* label, float h = 34) {
    return WideBtn(label, { 0.12f,0.15f,0.20f,1 }, h);
}

static void BeginCard(float w, float h) {
    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * .5f, cy = io.DisplaySize.y * .5f;
    ImGui::SetNextWindowPos({ cx,cy }, ImGuiCond_Always, { .5f,.5f });
    ImGui::SetNextWindowSize({ w,h }, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::LINE);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14);
    ImGui::BeginChild("##card", { w,h }, true);
}
static void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
}
static bool BeginBG() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0,0 }); ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::BG);
    return ImGui::Begin("##bg", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
}
static void EndBG() { ImGui::End(); ImGui::PopStyleColor(); }

static void UI_Tutorial() {
    BeginBG();
    BeginCard(580, 410);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 base = ImGui::GetWindowPos();
        for (int i = 0; i < 3; i++) {
            float dx = 272 + i * 22.0f;
            ImVec4 dc = (i == G.tutPage) ? Col::ACC : Col::LINE;
            dl->AddCircleFilled({ base.x + dx,base.y + 20 }, 5,
                IM_COL32((int)(dc.x * 255), (int)(dc.y * 255), (int)(dc.z * 255), 255));
        }
    }
    ImGui::Dummy({ 0,34 });

    if (G.tutPage == 0) {
        ImGui::SetWindowFontScale(1.7f); Centered("Welcome", Col::ACC);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy({ 0,14 }); AccentLine(); ImGui::Dummy({ 0,16 });
        ImGui::SetCursorPosX(28);
        ImGui::TextWrapped(
            "This tool scans your Minecraft installations for suspicious or "
            "blacklisted mods and checks whether your game has connected to "
            "slowfiz.net.\n\n"
            "The scan runs entirely on this PC. No files are uploaded or sent "
            "anywhere.\n\n"
            "Your result will be saved locally so the slowfiz.net staff can "
            "review it during your support ticket.");

    }
    else if (G.tutPage == 1) {
        ImGui::SetWindowFontScale(1.4f); Centered("What gets scanned?", Col::TEXT);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy({ 0,14 }); AccentLine(); ImGui::Dummy({ 0,16 });
        struct Row { const char* icon; const char* text; };
        Row rows[] = {
            {"📋","Log files — checked for the 'Loading mods' block to identify which mods were active"},
            {"📁","Mods folder — file names matched against a known cheat mod list"},
            {"🌐","Log entries — checked for any connection to slowfiz.net"},
            {"🕹","Supported launchers: Official · Modrinth · CurseForge · Prism · MultiMC · Feather · NoRisk · Lunar · Badlion"},
        };
        for (auto& row : rows) {
            ImGui::SetCursorPosX(28);
            ImGui::TextColored(Col::ACCH, "%s", row.icon);
            ImGui::SameLine(58); ImGui::TextWrapped("%s", row.text);
            ImGui::Dummy({ 0,8 });
        }

    }
    else {
        ImGui::SetWindowFontScale(1.4f); Centered("Before we continue", Col::TEXT);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy({ 0,14 }); AccentLine(); ImGui::Dummy({ 0,16 });
        ImGui::SetCursorPosX(28);
        ImGui::TextWrapped(
            "By tapping 'I Agree' you confirm that:\n\n"
            "  ✓  You allow the scanner to read your local Minecraft files\n\n"
            "  ✓  You understand the result is saved on this PC\n\n"
            "  ✓  You will take a screenshot of the result and attach it to "
            "your slowfiz.net support ticket for staff review");
    }

    float btnY = 410 - 62;
    ImGui::SetCursorPos({ 24,(float)btnY });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0,0,0,0 });
    ImGui::BeginChild("##nav", { 532,50 }, false);
    if (G.tutPage > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, Col::CARD2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.18f,0.22f,0.30f,1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::CARD);
        if (ImGui::Button("← Back", { 115,38 })) G.tutPage--;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 12);
        float w = 532 - 115 - 12;
        if (G.tutPage < 2) {
            if (WideBtn("Next →", Col::BTN, 38)) G.tutPage++;
        }
        else {
            if (WideBtn("I Agree — Let's Start", Col::BTN, 38)) G.screen = Screen::IGN;
        }
        (void)w;
    }
    else {
        if (WideBtn("Next →", Col::BTN, 38)) G.tutPage++;
    }
    ImGui::EndChild(); ImGui::PopStyleColor();

    EndCard(); EndBG();
}

static void UI_IGN() {
    BeginBG();
    BeginCard(440, 230);

    ImGui::Dummy({ 0,20 });
    ImGui::SetWindowFontScale(1.3f); Centered("Your Minecraft IGN", Col::TEXT);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy({ 0,5 });
    Centered("Enter the name you use on slowfiz.net", Col::DIM);
    ImGui::Dummy({ 0,18 }); AccentLine(); ImGui::Dummy({ 0,16 });

    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputText("##ignf", G.ign, sizeof(G.ign),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (strlen(G.ign) == 0 && !ImGui::IsItemActive()) {
        ImVec2 p = ImGui::GetItemRectMin(); p.x += 14; p.y += 6;
        ImGui::GetWindowDrawList()->AddText(p, IM_COL32(78, 90, 108, 255), "e.g. Notch");
    }
    ImGui::Dummy({ 0,14 });
    bool dis = (strlen(G.ign) < 2);
    if (dis) ImGui::BeginDisabled();
    if (WideBtn("Continue →", Col::BTN) || enter)
        if (strlen(G.ign) >= 2) { G.ignSet = true; G.screen = Screen::MAIN; }
    if (dis) ImGui::EndDisabled();

    EndCard(); EndBG();
}

static void UI_RescanCode() {
    BeginBG();
    BeginCard(440, 260);

    ImGui::Dummy({ 0,22 });
    ImGui::SetWindowFontScale(1.3f); Centered("New Scan", Col::TEXT);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy({ 0,5 });
    Centered("Enter today's staff code to run a fresh scan.", Col::DIM);
    ImGui::Dummy({ 0,18 }); AccentLine(); ImGui::Dummy({ 0,16 });

    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputText("##rc", G.codeInput, sizeof(G.codeInput),
        ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
    if (strlen(G.codeInput) == 0 && !ImGui::IsItemActive()) {
        ImVec2 p = ImGui::GetItemRectMin(); p.x += 14; p.y += 6;
        ImGui::GetWindowDrawList()->AddText(p, IM_COL32(78, 90, 108, 255), "Enter code...");
    }
    ImGui::Dummy({ 0,8 });
    if (G.codeFail)
        ImGui::TextColored(Col::RED, "  ✗  Wrong code. Ask staff if you need it.");
    else
        ImGui::Dummy({ 0,20 });
    ImGui::Dummy({ 0,6 });
    if (WideBtn("Unlock", Col::BTN) || enter) {
        if (std::string(G.codeInput) == GetDailyCode()) {
            G.done = false; G.result = {}; G.log.clear();
            G.prog = 0; G.codeFail = false; G.consented = false;
            memset(G.codeInput, 0, sizeof(G.codeInput));
            G.screen = Screen::MAIN;
        }
        else {
            G.codeFail = true;
            memset(G.codeInput, 0, sizeof(G.codeInput));
        }
    }
    ImGui::Dummy({ 0,6 });
    if (SecondaryBtn("← Back to Results"))
        G.screen = Screen::MAIN;

    EndCard(); EndBG();
}

static void UI_Screenshot() {
    BeginBG();
    BeginCard(540, 390);

    ImGui::Dummy({ 0,24 });
    ImGui::SetWindowFontScale(2.0f); Centered("📸", Col::ACC);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy({ 0,8 });
    ImGui::SetWindowFontScale(1.35f); Centered("Submit Your Results", Col::TEXT);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy({ 0,16 }); AccentLine(); ImGui::Dummy({ 0,18 });

    ImGui::SetCursorPosX(26);
    ImGui::TextWrapped(
        "Please take a screenshot of the Results tab and attach it to your "
        "slowfiz.net support ticket.");
    ImGui::Dummy({ 0,12 });

    ImGui::SetCursorPosX(26);
    ImGui::TextColored(Col::DIM, "How to screenshot: ");
    ImGui::SameLine(0, 0); ImGui::TextColored(Col::ACCH, "Win + Shift + S");
    ImGui::SameLine(0, 0); ImGui::TextColored(Col::DIM, "  or  ");
    ImGui::SameLine(0, 0); ImGui::TextColored(Col::ACCH, "Print Screen");
    ImGui::Dummy({ 0,18 });

    {
        auto& r = G.result;
        ImVec4 fc = (r.risk == Risk::EVIDENCE) ? Col::RED :
            (r.risk == Risk::MAYBE) ? Col::YELLOW : Col::GREEN;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            { fc.x * .12f,fc.y * .12f,fc.z * .12f,1 });
        ImGui::BeginChild("##vb", { -1,52 }, true);
        ImGui::Dummy({ 0,4 });
        char vl[80]; snprintf(vl, sizeof(vl), "Verdict: %s", RiskLabel(r.risk).c_str());
        float vw = ImGui::CalcTextSize(vl).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - vw) * .5f);
        ImGui::TextColored(fc, "%s", vl);
        float sw = ImGui::CalcTextSize(r.summary.c_str()).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - sw) * .5f);
        ImGui::TextColored(Col::DIM, "%s", r.summary.c_str());
        ImGui::EndChild(); ImGui::PopStyleColor();
    }

    ImGui::Dummy({ 0,18 });
    if (WideBtn("View Results →", Col::BTN, 38))
        G.screen = Screen::MAIN;

    EndCard(); EndBG();
}

static void UI_ScanTab() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.08f,0.13f,0.22f,0.5f });
    ImGui::BeginChild("##sh", { -1,50 }, false);
    ImGui::Dummy({ 0,5 }); ImGui::SetCursorPosX(14);
    ImGui::SetWindowFontScale(1.05f); ImGui::TextColored(Col::ACC, "Scan");
    ImGui::SetWindowFontScale(1.0f); ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
    ImGui::TextColored(Col::DIM, "—  Check your Minecraft installations for suspicious mods");
    ImGui::SameLine(ImGui::GetWindowWidth() - 170);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
    ImGui::TextColored(Col::DIM, "IGN: "); ImGui::SameLine(0, 0);
    ImGui::TextColored(Col::ACCH, "%s", G.ign);
    ImGui::EndChild(); ImGui::PopStyleColor();
    ImGui::Dummy({ 0,12 });

    ImGui::Checkbox("  I consent to scanning my local Minecraft data", &G.consented);
    ImGui::Dummy({ 0,5 });
    ImGui::TextColored(Col::DIM,
        "  Official  •  Modrinth  •  CurseForge  •  Feather  •  NoRisk"
        "  •  Prism  •  MultiMC  •  Lunar  •  Badlion");
    ImGui::Dummy({ 0,16 });

    bool canScan = G.consented && !G.scanning && !G.done;
    if (!canScan) ImGui::BeginDisabled();
    if (WideBtn(G.scanning ? "  Scanning..." : "  Start Scan", Col::BTN, 46) && canScan) {
        G.scanning = true; G.done = false; G.prog = 0; G.log.clear();
        G.result = {}; G.autoTab = false;
        std::thread(DoScan).detach();
    }
    if (!canScan) ImGui::EndDisabled();

    if (G.done) {
        ImGui::Dummy({ 0,8 });
        const char* lnk = "Request a new scan";
        float lw = ImGui::CalcTextSize(lnk).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - lw) * .5f);
        ImGui::TextColored(Col::ACC, "%s", lnk);
        if (ImGui::IsItemClicked()) G.screen = Screen::RESCAN_CODE;
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImGui::Dummy({ 0,12 });

    if (G.scanning || G.done) {
        ImVec4 pc = (G.done) ? (
            G.result.risk == Risk::EVIDENCE ? Col::RED :
            G.result.risk == Risk::MAYBE ? Col::YELLOW : Col::GREEN)
            : Col::ACC;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, pc);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.08f,0.10f,0.14f,1 });
        ImGui::ProgressBar(G.prog, { -1,16 }, "");
        ImGui::PopStyleColor(2);
        ImGui::Dummy({ 0,3 }); ImGui::SetCursorPosX(4);
        ImGui::TextColored(Col::DIM, "%s", G.progMsg.c_str());
        ImGui::Dummy({ 0,6 });
    }

    if (!G.log.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.04f,0.05f,0.06f,1 });
        ImGui::BeginChild("##lg", { -1,ImGui::GetContentRegionAvail().y - 6 }, false);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4,2 });
        for (auto& line : G.log) {
            ImVec4 c = Col::DIM;
            if (line.rfind("[ALERT]", 0) == 0)      c = Col::RED;
            else if (line.rfind("[MATCH]", 0) == 0) c = Col::ORANGE;
            else if (line.rfind("[WARN]", 0) == 0)  c = Col::YELLOW;
            else if (line.rfind("[DONE]", 0) == 0)  c = Col::GREEN;
            else if (line.rfind("[SCAN]", 0) == 0)  c = Col::TEXT;
            ImGui::TextColored(c, "%s", line.c_str());
        }
        if (G.scrollLog) { ImGui::SetScrollHereY(1); G.scrollLog = false; }
        ImGui::PopStyleVar();
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
}

static void UI_ResultsTab() {
    if (!G.done) {
        ImGui::Dummy({ 0,60 }); Centered("No results yet — run a scan first.", Col::DIM); return;
    }
    auto& r = G.result;

    ImVec4 bg, fg; const char* label; const char* ico;
    switch (r.risk) {
    case Risk::EVIDENCE:
        bg = { 0.26f,0.04f,0.04f,0.92f }; fg = Col::RED;
        label = "EVIDENCE WITHOUT DOUBT"; ico = "⛔"; break;
    case Risk::MAYBE:
        bg = { 0.22f,0.17f,0.02f,0.92f }; fg = Col::YELLOW;
        label = "MAYBE FLAGS"; ico = "⚠"; break;
    default:
        bg = { 0.04f,0.19f,0.09f,0.92f }; fg = Col::GREEN;
        label = "CLEAN"; ico = "✓"; break;
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, fg);
    ImGui::BeginChild("##rb", { -1,70 }, true);
    ImGui::Dummy({ 0,4 });
    char big[80]; snprintf(big, sizeof(big), "%s  %s", ico, label);
    ImGui::SetWindowFontScale(1.5f);
    float lw = ImGui::CalcTextSize(big).x * 1.5f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - lw) * .5f - 6);
    ImGui::TextColored(fg, "%s", big);
    ImGui::SetWindowFontScale(1.0f);
    float sw = ImGui::CalcTextSize(r.summary.c_str()).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - sw) * .5f);
    ImGui::TextColored(Col::DIM, "%s", r.summary.c_str());
    ImGui::EndChild(); ImGui::PopStyleColor(2);
    ImGui::Dummy({ 0,10 });

    float cw = (ImGui::GetContentRegionAvail().x - 26) / 4.0f;
    auto card = [&](const char* id, const char* title, const char* val, ImVec4 vc) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::CARD);
        ImGui::BeginChild(id, { cw,58 }, false);
        ImGui::Dummy({ 0,2 }); ImGui::SetCursorPosX(10);
        ImGui::TextColored(Col::DIM, "%s", title);
        ImGui::SetCursorPosX(10); ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored(vc, "%s", val);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild(); ImGui::PopStyleColor();
        };
    std::string cnt = std::to_string(r.findings.size());
    std::string evCnt = std::to_string(
        std::count_if(r.findings.begin(), r.findings.end(), [](auto& f) {return f.certain; }));
    card("##c1", "Player IGN", G.ign, Col::ACC);
    ImGui::SameLine(0, 8);
    card("##c2", "Findings", cnt.c_str(), r.findings.empty() ? Col::GREEN : Col::RED);
    ImGui::SameLine(0, 8);
    card("##c3", "Evidence", evCnt.c_str(), r.hitSlowfiz ? Col::RED : Col::ORANGE);
    ImGui::SameLine(0, 8);
    card("##c4", "slowfiz.net", r.hitSlowfiz ? "Connected" : "Not Detected",
        r.hitSlowfiz ? Col::RED : Col::GREEN);
    ImGui::Dummy({ 0,8 });

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.09f,0.13f,0.22f,0.55f });
    ImGui::BeginChild("##cta", { -1,38 }, false);
    ImGui::SetCursorPos({ 12,9 });
    ImGui::TextColored(Col::ACCH, "📸");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(Col::TEXT, "Take a screenshot of this tab and attach it to your ticket.");
    ImGui::SameLine(ImGui::GetWindowWidth() - 165);
    ImGui::SetCursorPosY(4);
    if (ImGui::Button("Screenshot Guide", { 155,28 }))
        G.screen = Screen::SCREENSHOT;
    ImGui::EndChild(); ImGui::PopStyleColor();
    ImGui::Dummy({ 0,6 });

    if (!r.findings.empty()) {
        float th = ImGui::GetContentRegionAvail().y - 6;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.04f,0.05f,0.06f,1 });
        ImGui::BeginChild("##ft", { -1,th }, false);
        if (ImGui::BeginTable("##tbl", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, { 0,th - 4 })) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Launcher", ImGuiTableColumnFlags_WidthStretch, 0.20f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.12f);
            ImGui::TableSetupColumn("Verdict", ImGuiTableColumnFlags_WidthStretch, 0.14f);
            ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 0.54f);
            ImGui::TableHeadersRow();
            for (auto& f : r.findings) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(Col::ACC, "%s", f.launcher.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(f.type == "MOD_FILE" ? Col::ORANGE : Col::YELLOW, "%s", f.type.c_str());
                ImGui::TableSetColumnIndex(2);
                if (f.certain) ImGui::TextColored(Col::RED, "EVIDENCE");
                else          ImGui::TextColored(Col::YELLOW, "MAYBE");
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(Col::TEXT, "%s", f.detail.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    else {
        ImGui::Dummy({ 0,20 });
        Centered("No suspicious findings.", Col::GREEN, 1.05f);
    }
}

static void UI_HiddenConfig() {
    if (!G.showCfg) return;
    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * .5f, cy = io.DisplaySize.y * .5f;
    ImGui::SetNextWindowPos({ cx,cy }, ImGuiCond_Always, { .5f,.5f });
    ImGui::SetNextWindowSize({ 420,160 }, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::PANEL);
    bool open = true;
    ImGui::Begin("Config##h", &open,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    AccentLine(); ImGui::Dummy({ 0,8 });

    ImGui::Text("Today's rescan code:"); ImGui::SameLine();
    ImGui::TextColored(Col::ACC, "%s", GetDailyCode().c_str());

    ImGui::Dummy({ 0,14 });
    if (ImGui::Button("Delete result file (reset)", { -1,34 })) {
        fs::remove(GetResultPath()); G.showCfg = false;
    }
    ImGui::SameLine();
    if (!open) G.showCfg = false;
    ImGui::End(); ImGui::PopStyleColor();
}

static int g_pendingTab = -1;

static void UI_Main() {
    ImGuiIO& io = ImGui::GetIO();
    if (G.autoTab) { g_pendingTab = 1; G.autoTab = false; }

    if (io.KeyCtrl && io.KeyShift && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_C, false))
        G.showCfg = !G.showCfg;

    ImGui::SetNextWindowPos({ 0,0 }); ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::BG);
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.05f,0.07f,0.10f,1 });
    ImGui::BeginChild("##ttb", { -1,42 }, false);
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        wp, { wp.x + io.DisplaySize.x,wp.y + 2 },
        IM_COL32(18, 76, 202, 230), IM_COL32(46, 136, 255, 85),
        IM_COL32(46, 136, 255, 85), IM_COL32(18, 76, 202, 230));
    ImGui::SetCursorPos({ 14,12 });
    ImGui::SetWindowFontScale(1.08f); ImGui::TextColored(Col::ACC, "SLOWFIZ");
    ImGui::SetWindowFontScale(1.0f); ImGui::SameLine(0, 6);
    ImGui::SetCursorPosY(15); ImGui::TextColored(Col::DIM, "SCANNER");
    ImGui::SameLine(0, 16); ImGui::SetCursorPosY(16); ImGui::TextColored(Col::LINE, "│");
    ImGui::SameLine(0, 16); ImGui::SetCursorPosY(16); ImGui::TextColored(Col::DIM, "slowfiz.net");
    ImGui::EndChild(); ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_TabActive, { 0.14f,0.40f,0.86f,1 });
    ImGui::PushStyleColor(ImGuiCol_Tab, { 0.07f,0.09f,0.12f,1 });
    ImGui::PushStyleColor(ImGuiCol_TabHovered, { 0.14f,0.36f,0.76f,0.7f });
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 5);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 14,7 });

    if (ImGui::BeginTabBar("##tabs")) {
        ImGuiTabItemFlags sf = (g_pendingTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(" Scan ", nullptr, sf)) {
            if (g_pendingTab == 0) g_pendingTab = -1;
            ImGui::Dummy({ 0,8 }); UI_ScanTab(); ImGui::EndTabItem();
        }
        std::string rl = G.done ? " Results ● " : " Results ";
        ImGuiTabItemFlags rf = (g_pendingTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(rl.c_str(), nullptr, rf)) {
            if (g_pendingTab == 1) g_pendingTab = -1;
            ImGui::Dummy({ 0,8 }); UI_ResultsTab(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);
    ImGui::End(); ImGui::PopStyleColor();

    UI_HiddenConfig();
}

static ID3D11Device* g_Dev = nullptr;
static ID3D11DeviceContext* g_Ctx = nullptr;
static IDXGISwapChain* g_SC = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;

static bool InitD3D(HWND h) {
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 60,1 };
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = h; sd.SampleDesc = { 1,0 }; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, fls, 2, D3D11_SDK_VERSION, &sd, &g_SC, &g_Dev, &fl, &g_Ctx) != S_OK) return false;
    ID3D11Texture2D* pb = nullptr; g_SC->GetBuffer(0, IID_PPV_ARGS(&pb));
    g_Dev->CreateRenderTargetView(pb, nullptr, &g_RTV); pb->Release(); return true;
}
static void DestroyD3D() {
    if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; }
    if (g_SC) { g_SC->Release(); g_SC = nullptr; }
    if (g_Ctx) { g_Ctx->Release(); g_Ctx = nullptr; }
    if (g_Dev) { g_Dev->Release(); g_Dev = nullptr; }
}
static void CreateRT() {
    ID3D11Texture2D* pb = nullptr; g_SC->GetBuffer(0, IID_PPV_ARGS(&pb));
    g_Dev->CreateRenderTargetView(pb, nullptr, &g_RTV); pb->Release();
}
static void DestroyRT() { if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; } }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_SIZE && g_Dev && w != SIZE_MINIMIZED) {
        DestroyRT();
        g_SC->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0);
        CreateRT(); return 0;
    }
    if (m == WM_SYSCOMMAND && (w & 0xfff0) == SC_KEYMENU) return 0;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int) {
    WNDCLASSEX wc{ sizeof(WNDCLASSEX),CS_CLASSDC,WndProc,0,0,hI,
        nullptr,nullptr,nullptr,nullptr,_T("SFScanner"),nullptr };
    RegisterClassEx(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Slowfiz Scanner"),
        WS_OVERLAPPEDWINDOW, (sw - WW) / 2, (sh - WH) / 2, WW, WH, nullptr, nullptr, hI, nullptr);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    if (!InitD3D(hwnd)) { DestroyD3D(); UnregisterClass(wc.lpszClassName, hI); return 1; }
    ShowWindow(hwnd, SW_SHOWDEFAULT); UpdateWindow(hwnd);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImFontConfig fc; fc.SizePixels = 15.0f; io.Fonts->AddFontDefault(&fc);
    ApplyTheme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_Dev, g_Ctx);

    if (LoadPrevResult()) {
        G.screen = Screen::MAIN;
        g_pendingTab = 1;
    }

    const float CC[4] = { 0.05f,0.06f,0.08f,1.0f };
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg); continue;
        }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        G.t += io.DeltaTime;

        switch (G.screen) {
        case Screen::TUTORIAL:    UI_Tutorial();    break;
        case Screen::IGN:         UI_IGN();         break;
        case Screen::MAIN:        UI_Main();        break;
        case Screen::RESCAN_CODE: UI_RescanCode();  break;
        case Screen::SCREENSHOT:  UI_Screenshot();  break;
        }

        ImGui::Render();
        g_Ctx->OMSetRenderTargets(1, &g_RTV, nullptr);
        g_Ctx->ClearRenderTargetView(g_RTV, CC);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_SC->Present(1, 0);
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    DestroyD3D(); DestroyWindow(hwnd); UnregisterClass(wc.lpszClassName, hI);
    return 0;
}
