/*
 * BLOOD BANK MANAGEMENT SYSTEM  -  Modern Windows GUI (Win32 + GDI, C++)
 * -----------------------------------------------------------------
 * GUI version of your console project. Works in Dev-C++ 5.11 (C++98 style,
 * no extra libraries or linker flags needed). Everything on screen is custom drawn.
 *
 * Same features as the console version:
 *   Add donor, display donors, search by blood type, update donor, delete donor,
 *   eligibility check (men 90 days, women 120 days), validation of contact/Gmail,
 *   and the same "donors.txt" file format (old data loads automatically).
 *
 * New in the GUI:
 *   Blood group tiles with live counts (click to filter), name search, donor details
 *   panel, eligible-donor list, blood group report, and today's date taken from the PC
 *   instead of a hard-coded date.
 */

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
using namespace std;

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

const char* BLOOD[8]  = {"A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-"};
const char* DATA_FILE = "donors.txt";

// =================================================================
// Classes (same idea as the console project)
// =================================================================
class Date
{
public:
    int day, month, year;
    Date() : day(1), month(1), year(2000) {}
    Date(int d, int m, int y) : day(d), month(m), year(y) {}
};

class Donor
{
public:
    string name;
    string gender;          // "M" or "F"
    string bloodType;
    string contactNumber;
    string gmailAddress;
    Date lastDonationDate;
    string status;

    Donor() : status("Not Eligible") {}
};

// =================================================================
// Date related functions (from the console project)
// =================================================================
const int monthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int countLeapYears(Date d)
{
    int years = d.year;
    if (d.month <= 2) years--;
    return years / 4 - years / 100 + years / 400;
}

int getDifference(Date dt1, Date dt2)
{
    long int n1 = dt1.year * 365 + dt1.day;
    for (int i = 0; i < dt1.month - 1; i++) n1 += monthDays[i];
    n1 += countLeapYears(dt1);

    long int n2 = dt2.year * 365 + dt2.day;
    for (int i = 0; i < dt2.month - 1; i++) n2 += monthDays[i];
    n2 += countLeapYears(dt2);

    return (int)(n2 - n1);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }

int daysInMonth(int m, int y)
{
    if (m == 2 && isLeap(y)) return 29;
    return monthDays[m - 1];
}

Date today;

void updateToday()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    today = Date((int)t.wDay, (int)t.wMonth, (int)t.wYear);
}

// ---------- eligibility ----------
int requiredDays(const Donor& d) { return d.gender == "F" ? 120 : 90; }
int daysSince(const Donor& d)    { return getDifference(d.lastDonationDate, today); }
bool isEligible(const Donor& d)  { return daysSince(d) >= requiredDays(d); }
int daysLeft(const Donor& d)     { return requiredDays(d) - daysSince(d); }

void refreshStatus(Donor& d)
{
    d.status = isEligible(d) ? "Eligible for Blood Donation." : "Not Eligible";
}

// ---------- small helpers ----------
string num(int v) { ostringstream o; o << v; return o.str(); }

string toLower(string s)
{
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

string toUpper(string s)
{
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

string clean(string s)      // trim, and remove the characters the file format uses
{
    for (size_t i = 0; i < s.size(); i++) if (s[i] == ',' || s[i] == '|') s[i] = ' ';
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

string dateText(const Date& d)
{
    char buf[32];
    sprintf(buf, "%02d-%02d-%04d", d.day, d.month, d.year);
    return buf;
}

int bloodIndex(const string& s)
{
    string u = toUpper(s);
    for (int i = 0; i < 8; i++) if (u == BLOOD[i]) return i;
    return -1;
}

bool validDate(const Date& d)
{
    if (d.year < 1900 || d.month < 1 || d.month > 12) return false;
    if (d.day < 1 || d.day > daysInMonth(d.month, d.year)) return false;
    return getDifference(d, today) >= 0;           // not in the future
}

// =================================================================
// Application data
// =================================================================
vector<Donor> donors;
int  curGender = 0;        // 0 = M, 1 = F
int  curBT     = -1;       // blood type chosen in the form (0..7), -1 = none
int  curFilter = -1;       // blood group filter for the table, -1 = all
int  selIdx    = -1;       // donor currently selected in the table (index in 'donors')
int  editIdx   = -1;       // donor being edited, -1 = adding a new donor
string filterText = "";
int  shownCount = 0;
int  bloodCount[8];
int  eligibleCount = 0;

void saveData()
{
    ofstream out(DATA_FILE);
    if (!out) return;
    for (size_t i = 0; i < donors.size(); i++)
    {
        const Donor& d = donors[i];
        out << d.name << "," << d.gender << "," << d.bloodType << ","
            << d.contactNumber << "," << d.gmailAddress << ","
            << d.lastDonationDate.day << "-" << d.lastDonationDate.month << "-"
            << d.lastDonationDate.year << "," << d.status << endl;
    }
}

void loadData()
{
    ifstream in(DATA_FILE);
    if (!in) return;
    donors.clear();
    string line;
    while (getline(in, line))
    {
        stringstream ss(line);
        Donor d;
        string dt;
        if (!getline(ss, d.name, ',')) continue;
        getline(ss, d.gender, ',');
        getline(ss, d.bloodType, ',');
        getline(ss, d.contactNumber, ',');
        getline(ss, d.gmailAddress, ',');
        getline(ss, dt, ',');
        getline(ss, d.status, ',');
        int dd = 0, mm = 0, yy = 0;
        if (sscanf(dt.c_str(), "%d%*c%d%*c%d", &dd, &mm, &yy) != 3) continue;
        d.lastDonationDate = Date(dd, mm, yy);
        if (d.name.empty()) continue;
        donors.push_back(d);
    }
}

void calcCounts()
{
    for (int i = 0; i < 8; i++) bloodCount[i] = 0;
    eligibleCount = 0;
    for (size_t i = 0; i < donors.size(); i++)
    {
        int b = bloodIndex(donors[i].bloodType);
        if (b >= 0) bloodCount[b]++;
        if (isEligible(donors[i])) eligibleCount++;
    }
}

// =================================================================
// GUI : colours, fonts, ids
// =================================================================
#define C_BG        RGB(250, 244, 245)
#define C_CARD      RGB(255, 255, 255)
#define C_LINE      RGB(240, 224, 226)
#define C_FIELD     RGB(251, 246, 247)
#define C_FIELDLINE RGB(222, 203, 206)
#define C_DARK      RGB(46, 28, 32)
#define C_MUTED     RGB(120, 98, 104)
#define C_RED       RGB(220, 38, 38)
#define C_GREEN     RGB(34, 197, 94)
#define C_AMBER     RGB(245, 158, 11)
#define C_BLUE      RGB(37, 99, 235)

enum {
    ID_GM = 100, ID_GF,
    ID_BT0 = 102,                       // 102..109 blood type chips in the form
    ID_ADD = 110, ID_CLEAR,
    ID_FALL = 112,                      // 112 = "All", 113..120 = blood group filter tiles
    ID_BTNSEARCH = 121, ID_SHOWALL, ID_EDIT, ID_DELETE, ID_ELIG, ID_REPORT,
    ID_BTN_LAST = ID_REPORT,
    ID_NAME = 200, ID_CONTACT, ID_GMAIL, ID_DATE, ID_SEARCH, ID_LIST, ID_OUT
};

HWND hWnd, hName, hContact, hGmail, hDate, hSearch, hList, hOut;
HFONT hUI, hBold, hSmallB, hCardTitle, hTitleF, hBig, hMono;
HBRUSH hBrField;
int hoverId = 0;

const int LIST_X = 392;
const int ROW_H  = 34;

struct Lbl { int x, y, kind; const char* text; };
Lbl lbls[40];  int nLbl = 0;
RECT frames[16]; int nFrames = 0;

void addLbl(int x, int y, int kind, const char* t)
{
    lbls[nLbl].x = x; lbls[nLbl].y = y; lbls[nLbl].kind = kind; lbls[nLbl].text = t; nLbl++;
}
void addFrame(int x, int y, int w, int h)
{
    frames[nFrames].left = x; frames[nFrames].top = y;
    frames[nFrames].right = x + w; frames[nFrames].bottom = y + h; nFrames++;
}

// ---------- drawing helpers ----------
void fillRc(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r = {x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void rrect(HDC dc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, x, y, x + w + 1, y + h + 1, r * 2, r * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void circle(HDC dc, int x, int y, int d, COLORREF fill, COLORREF border)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    Ellipse(dc, x, y, x + d, y + d);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void txt(HDC dc, HFONT f, COLORREF c, int x, int y, int w, int h, const string& s, UINT flags)
{
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    RECT r = {x, y, x + w, y + h};
    DrawTextA(dc, s.c_str(), -1, &r, flags | DT_NOPREFIX);
    SelectObject(dc, of);
}

void gradient(HDC dc, int x, int y, int w, int h, COLORREF a, COLORREF b)
{
    for (int i = 0; i < w; i += 3)
    {
        int t = (i * 255) / w;
        int rr = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 255;
        int gg = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 255;
        int bb = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 255;
        fillRc(dc, x + i, y, 3, h, RGB(rr, gg, bb));
    }
}

void magnifier(HDC dc, int x, int y, int size, int pw, COLORREF col)
{
    HPEN p = CreatePen(PS_SOLID, pw, col);
    HGDIOBJ op = SelectObject(dc, p);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, x, y, x + size, y + size);
    MoveToEx(dc, x + size - size / 6, y + size - size / 6, NULL);
    LineTo(dc, x + size + size / 3, y + size + size / 3);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(p);
}

// a blood drop: triangle on top of a circle
void drop(HDC dc, int x, int y, int s, COLORREF col)
{
    HBRUSH b = CreateSolidBrush(col);
    HPEN p = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    POINT tri[3];
    tri[0].x = x + s / 2; tri[0].y = y;
    tri[1].x = x + s / 8; tri[1].y = y + (s * 5) / 9;
    tri[2].x = x + s - s / 8; tri[2].y = y + (s * 5) / 9;
    Polygon(dc, tri, 3);
    Ellipse(dc, x, y + s / 3, x + s, y + s);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

COLORREF shade(COLORREF c, int amt)
{
    int r = GetRValue(c) + amt, g = GetGValue(c) + amt, b = GetBValue(c) + amt;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

void card(HDC dc, int x, int y, int w, int h)
{
    rrect(dc, x, y + 2, w, h, 14, RGB(240, 226, 228), RGB(240, 226, 228));   // soft shadow
    rrect(dc, x, y, w, h, 14, C_CARD, C_LINE);
}

void statCard(HDC dc, int x, int y, int w, int h, COLORREF accent, const char* glyph,
              const string& number, const char* label)
{
    card(dc, x, y, w, h);
    circle(dc, x + 18, y + 16, 44, accent, accent);
    txt(dc, hBig, RGB(255, 255, 255), x + 18, y + 16, 44, 44, glyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hBig, C_DARK, x + 76, y + 8, w - 90, 38, number, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hUI, C_MUTED, x + 76, y + 46, w - 90, 20, label, DT_LEFT | DT_SINGLELINE);
}

void pill(HDC dc, int x, int y, int w, int h, COLORREF bg, COLORREF fg, const string& text)
{
    rrect(dc, x, y, w, h, h / 2, bg, bg);
    txt(dc, hSmallB, fg, x, y, w, h, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------- the donor details panel (bottom right) ----------
void drawDetails(HDC dc)
{
    rrect(dc, 888, 572, 316, 154, 8, C_FIELD, C_FIELDLINE);
    if (selIdx < 0 || selIdx >= (int)donors.size())
    {
        drop(dc, 1018, 596, 36, RGB(236, 200, 204));
        txt(dc, hUI, C_MUTED, 904, 650, 284, 60, "Select a donor in the table to see the full details here.",
            DT_CENTER | DT_WORDBREAK);
        return;
    }
    const Donor& d = donors[selIdx];
    bool ok = isEligible(d);

    circle(dc, 902, 584, 54, C_RED, C_RED);
    txt(dc, hCardTitle, RGB(255, 255, 255), 902, 584, 54, 54, d.bloodType, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hCardTitle, C_DARK, 966, 586, 228, 24, d.name, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    string sub = (d.gender == "F" ? "Female" : "Male");
    sub += "  |  " + num(daysSince(d)) + " days since last donation";
    txt(dc, hUI, C_MUTED, 966, 612, 230, 18, sub, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    txt(dc, hSmallB, C_MUTED, 902, 650, 70, 16, "PHONE", DT_LEFT | DT_SINGLELINE);
    txt(dc, hUI, C_DARK, 972, 648, 220, 18, d.contactNumber, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    txt(dc, hSmallB, C_MUTED, 902, 670, 70, 16, "EMAIL", DT_LEFT | DT_SINGLELINE);
    txt(dc, hUI, C_DARK, 972, 668, 220, 18, d.gmailAddress, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    txt(dc, hSmallB, C_MUTED, 902, 690, 70, 16, "LAST GAVE", DT_LEFT | DT_SINGLELINE);
    txt(dc, hUI, C_DARK, 972, 688, 220, 18, dateText(d.lastDonationDate), DT_LEFT | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, 902, 708, 70, 16, "STATUS", DT_LEFT | DT_SINGLELINE);
    string st = ok ? "Eligible - can donate now" : ("Eligible in " + num(daysLeft(d)) + " days");
    txt(dc, hBold, ok ? RGB(21, 128, 61) : RGB(180, 83, 9), 972, 706, 220, 18, st, DT_LEFT | DT_SINGLELINE);
}

// ---------- the whole window background ----------
void paintAll(HDC dc, int W, int H)
{
    fillRc(dc, 0, 0, W, H, C_BG);

    // ===== header =====
    gradient(dc, 0, 0, W, 76, RGB(127, 29, 29), RGB(239, 68, 68));
    rrect(dc, 20, 14, 48, 48, 12, RGB(255, 255, 255), RGB(255, 255, 255));
    drop(dc, 31, 22, 26, C_RED);
    txt(dc, hTitleF, RGB(255, 255, 255), 82, 8, 700, 38, "Blood Bank Management System", DT_LEFT | DT_SINGLELINE);
    txt(dc, hUI, RGB(255, 222, 222), 84, 46, 700, 20, "Register donors  |  Search by blood group  |  Track eligibility", DT_LEFT | DT_SINGLELINE);
    pill(dc, 936, 24, 104, 28, RGB(255, 255, 255), C_RED, "OOP Classes");
    pill(dc, 1050, 24, 80, 28, RGB(255, 255, 255), C_RED, "File I/O");
    pill(dc, 1140, 24, 80, 28, RGB(255, 255, 255), C_RED, "Vector");

    // ===== stat cards =====
    int total = (int)donors.size();
    int covered = 0;
    for (int i = 0; i < 8; i++) if (bloodCount[i] > 0) covered++;
    statCard(dc, 20,  90, 288, 76, C_RED,   "#", num(total),                  "Registered donors");
    statCard(dc, 324, 90, 288, 76, C_GREEN, "+", num(eligibleCount),          "Eligible to donate now");
    statCard(dc, 628, 90, 288, 76, C_AMBER, "!", num(total - eligibleCount),  "Waiting period");
    statCard(dc, 932, 90, 288, 76, C_BLUE,  "B", num(covered) + " / 8",       "Blood groups available");

    // ===== two big cards =====
    card(dc, 20, 182, 340, 558);
    card(dc, 376, 182, 844, 558);

    // ===== labels and titles =====
    for (int i = 0; i < nLbl; i++)
    {
        if (lbls[i].kind == 1)
            txt(dc, hCardTitle, C_DARK, lbls[i].x, lbls[i].y, 300, 22, lbls[i].text, DT_LEFT | DT_SINGLELINE);
        else
            txt(dc, hSmallB, C_MUTED, lbls[i].x, lbls[i].y, 240, 16, lbls[i].text, DT_LEFT | DT_SINGLELINE);
    }

    // ===== text field frames =====
    for (int i = 0; i < nFrames; i++)
        rrect(dc, frames[i].left, frames[i].top, frames[i].right - frames[i].left,
              frames[i].bottom - frames[i].top, 8, C_FIELD, C_FIELDLINE);

    magnifier(dc, 404, 261, 10, 2, C_MUTED);

    string cnt = "Showing " + num(shownCount) + " of " + num(total);
    txt(dc, hUI, C_MUTED, 1030, 260, 174, 20, cnt, DT_RIGHT | DT_SINGLELINE);

    // ===== table header strip =====
    fillRc(dc, LIST_X, 292, 812, 28, RGB(249, 240, 241));
    UINT f = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
    txt(dc, hSmallB, C_MUTED, LIST_X + 14,  292, 30,  28, "#",             f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 48,  292, 150, 28, "NAME",          f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 204, 292, 56,  28, "GENDER",        f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 262, 292, 60,  28, "BLOOD",         f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 330, 292, 110, 28, "CONTACT",       f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 448, 292, 100, 28, "LAST DONATION", f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 556, 292, 90,  28, "STATUS",        f);
    txt(dc, hSmallB, C_MUTED, LIST_X + 664, 292, 140, 28, "NEXT DONATION", f);

    // ===== tip box (left card bottom) =====
    rrect(dc, 36, 662, 308, 62, 10, RGB(254, 242, 242), RGB(254, 202, 202));
    txt(dc, hUI, RGB(153, 27, 27), 48, 668, 286, 52,
        "Men can donate again after 90 days, women after 120 days. Status updates from today's date.",
        DT_LEFT | DT_WORDBREAK);

    // ===== activity frame + details panel =====
    rrect(dc, 392, 572, 480, 154, 8, C_FIELD, C_FIELDLINE);
    drawDetails(dc);
}

// ---------- button colours ----------
void btnLook(int id, COLORREF* fill, COLORREF* textc, COLORREF* border)
{
    COLORREF white = RGB(255, 255, 255);
    *fill = white; *textc = C_MUTED; *border = C_FIELDLINE;      // default: un-selected toggle look

    if (id == ID_GM) { if (curGender == 0) { *fill = C_BLUE;              *textc = white; *border = C_BLUE; }              return; }
    if (id == ID_GF) { if (curGender == 1) { *fill = RGB(219, 39, 119);   *textc = white; *border = RGB(219, 39, 119); }   return; }
    if (id >= ID_BT0 && id < ID_BT0 + 8)
    {
        if (curBT == id - ID_BT0) { *fill = C_RED; *textc = white; *border = C_RED; }
        return;
    }
    if (id >= ID_FALL && id <= ID_FALL + 8)
    {
        if (curFilter == id - ID_FALL - 1) { *fill = C_RED; *textc = white; *border = C_RED; }
        return;
    }

    COLORREF c = C_RED;
    switch (id)
    {
        case ID_ADD:       c = C_RED;              break;
        case ID_BTNSEARCH: c = C_RED;              break;
        case ID_EDIT:      c = C_BLUE;             break;
        case ID_DELETE:    c = RGB(71, 85, 105);   break;
        case ID_ELIG:      c = RGB(22, 163, 74);   break;
        case ID_REPORT:    c = RGB(124, 58, 237);  break;
        case ID_CLEAR:     *fill = white; *textc = C_DARK; *border = C_FIELDLINE; return;
        case ID_SHOWALL:   *fill = white; *textc = C_DARK; *border = C_FIELDLINE; return;
    }
    *fill = c; *textc = white; *border = c;
}

void drawButton(DRAWITEMSTRUCT* d)
{
    int id = (int)d->CtlID;
    HDC dc = d->hDC;
    RECT rc = d->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    fillRc(dc, rc.left, rc.top, w, h, C_CARD);

    COLORREF fill, textc, border;
    btnLook(id, &fill, &textc, &border);

    bool pressed = (d->itemState & ODS_SELECTED) != 0;
    bool hover = (hoverId == id);
    if (pressed)    fill = shade(fill, -38);
    else if (hover) fill = shade(fill, -16);

    rrect(dc, rc.left, rc.top, w - 1, h - 1, 8, fill, border);

    if (id >= ID_FALL && id <= ID_FALL + 8)        // blood group tile: big label + small count
    {
        int k = id - ID_FALL;                        // 0 = All
        string top = (k == 0) ? "All" : BLOOD[k - 1];
        int c = (k == 0) ? (int)donors.size() : bloodCount[k - 1];
        string bottom = num(c) + (c == 1 ? " donor" : " donors");
        txt(dc, hCardTitle, textc, rc.left, rc.top + 3, w, 22, top, DT_CENTER | DT_SINGLELINE);
        txt(dc, hSmallB, textc, rc.left, rc.top + 25, w, 16, bottom, DT_CENTER | DT_SINGLELINE);
        return;
    }

    char buf[100];
    GetWindowTextA(d->hwndItem, buf, sizeof(buf));
    txt(dc, hBold, textc, rc.left, rc.top + (pressed ? 1 : 0), w, h, buf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------- table rows ----------
void drawRow(DRAWITEMSTRUCT* d)
{
    if (d->itemID == (UINT)-1) return;
    HDC dc = d->hDC;
    RECT rc = d->rcItem;
    int di = (int)d->itemData;
    if (di < 0 || di >= (int)donors.size()) return;
    const Donor& dn = donors[di];

    bool sel = (d->itemState & ODS_SELECTED) != 0;
    COLORREF bg = sel ? RGB(254, 242, 242) : ((d->itemID % 2) ? RGB(253, 250, 250) : RGB(255, 255, 255));
    int x = rc.left, y = rc.top, h = rc.bottom - rc.top;
    fillRc(dc, x, y, rc.right - rc.left, h, bg);
    fillRc(dc, x, rc.bottom - 1, rc.right - rc.left, 1, RGB(245, 233, 235));
    if (sel) fillRc(dc, x, y, 4, h, C_RED);

    UINT f = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    COLORREF soft = RGB(90, 70, 76);
    txt(dc, hUI,   C_MUTED, x + 14,  y, 30,  h, num(di + 1), f);
    txt(dc, hBold, C_DARK,  x + 48,  y, 152, h, dn.name, f);
    txt(dc, hUI,   soft,    x + 204, y, 56,  h, dn.gender == "F" ? "Female" : "Male", f);
    pill(dc, x + 262, y + (h - 22) / 2, 52, 22, RGB(254, 226, 226), RGB(185, 28, 28), dn.bloodType);
    txt(dc, hUI,   soft,    x + 330, y, 114, h, dn.contactNumber, f);
    txt(dc, hUI,   soft,    x + 448, y, 100, h, dateText(dn.lastDonationDate), f);

    bool ok = isEligible(dn);
    int py = y + (h - 22) / 2;
    if (ok) pill(dc, x + 556, py, 88, 22, RGB(220, 252, 231), RGB(21, 128, 61), "ELIGIBLE");
    else    pill(dc, x + 556, py, 88, 22, RGB(254, 243, 199), RGB(180, 83, 9),  "NOT YET");

    if (ok) txt(dc, hBold, RGB(21, 128, 61), x + 664, y, 144, h, "Ready now", f);
    else    txt(dc, hUI,   soft,             x + 664, y, 144, h, "in " + num(daysLeft(dn)) + " days", f);
}

// ---------- controls ----------
HWND mk(const char* cls, const char* text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                             hWnd, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)hUI, TRUE);
    return c;
}

HWND btn(const char* text, int x, int y, int w, int h, int id)
{
    return mk("BUTTON", text, WS_TABSTOP | BS_OWNERDRAW, x, y, w, h, id);
}

HWND field(int x, int y, int w, int id, int indent)
{
    addFrame(x, y, w, 30);
    return mk("EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, x + 10 + indent, y + 6, w - 20 - indent, 20, id);
}

string getText(HWND h)
{
    char buf[512];
    GetWindowTextA(h, buf, sizeof(buf));
    return clean(buf);
}

void setOut(const string& s)
{
    string t;
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '\n') t += "\r\n"; else t += s[i];
    }
    SetWindowTextA(hOut, t.c_str());
}

int selectedIndex()
{
    int idx = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
    if (idx == LB_ERR) return -1;
    return (int)SendMessageA(hList, LB_GETITEMDATA, idx, 0);
}

void redrawCtl(int id)
{
    HWND c = GetDlgItem(hWnd, id);
    if (c) InvalidateRect(c, NULL, FALSE);
}
void redrawGroup(int a, int b) { for (int i = a; i <= b; i++) redrawCtl(i); }

void refreshList()
{
    updateToday();
    for (size_t i = 0; i < donors.size(); i++) refreshStatus(donors[i]);
    calcCounts();

    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    string key = toLower(filterText);
    shownCount = 0;
    for (size_t i = 0; i < donors.size(); i++)
    {
        if (curFilter >= 0 && bloodIndex(donors[i].bloodType) != curFilter) continue;
        if (!key.empty() && toLower(donors[i].name).find(key) == string::npos) continue;
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)i);       // item data = index in 'donors'
        shownCount++;
    }
    selIdx = -1;
    redrawGroup(ID_FALL, ID_FALL + 8);
    InvalidateRect(hWnd, NULL, FALSE);
}

void changed() { saveData(); refreshList(); }

void setAddButtonText()
{
    SetWindowTextA(GetDlgItem(hWnd, ID_ADD), editIdx >= 0 ? "Save Changes" : "+  Register Donor");
    redrawCtl(ID_ADD);
}

void clearForm()
{
    SetWindowTextA(hName, "");
    SetWindowTextA(hContact, "");
    SetWindowTextA(hGmail, "");
    SetWindowTextA(hDate, dateText(today).c_str());
    curGender = 0; curBT = -1; editIdx = -1;
    redrawGroup(ID_GM, ID_GF);
    redrawGroup(ID_BT0, ID_BT0 + 7);
    setAddButtonText();
}

// ---------- actions ----------
void warn(const char* msg) { MessageBoxA(hWnd, msg, "Please check", MB_ICONWARNING); }

void onAdd()
{
    // ----- same validation rules as the console version -----
    string name = getText(hName);
    if (name.empty())                { warn("Please enter the donor's name."); return; }
    if (curBT < 0)                   { warn("Please choose a blood type."); return; }

    string contact = getText(hContact);
    bool digits = true;
    for (size_t i = 0; i < contact.size(); i++) if (!isdigit((unsigned char)contact[i])) digits = false;
    if (contact.length() != 11 || !digits) { warn("Invalid contact number. Please enter exactly 11 digits."); return; }

    string gmail = getText(hGmail);
    if (gmail.length() <= 10 || gmail.substr(gmail.length() - 10) != "@gmail.com")
    { warn("Invalid Gmail address. It must end with @gmail.com."); return; }

    int dd = 0, mm = 0, yy = 0;
    string ds = getText(hDate);
    if (sscanf(ds.c_str(), "%d%*c%d%*c%d", &dd, &mm, &yy) != 3)
    { warn("Enter the last donation date like 29-2-2023 (dd-mm-yyyy)."); return; }
    Date dt(dd, mm, yy);
    if (!validDate(dt))              { warn("That date is not valid (or it is in the future)."); return; }

    Donor d;
    d.name = name;
    d.gender = (curGender == 1) ? "F" : "M";
    d.bloodType = BLOOD[curBT];
    d.contactNumber = contact;
    d.gmailAddress = gmail;
    d.lastDonationDate = dt;
    refreshStatus(d);

    ostringstream o;
    if (editIdx >= 0 && editIdx < (int)donors.size())
    {
        donors[editIdx] = d;
        o << "Donor information updated successfully!\n";
    }
    else
    {
        donors.push_back(d);
        o << "Donor added successfully!\n";
    }
    o << d.name << "  (" << d.bloodType << ")\n";
    if (isEligible(d)) o << "Status: Eligible for blood donation.";
    else               o << "Status: Not eligible yet - " << daysLeft(d) << " more days.";
    setOut(o.str());

    clearForm();
    changed();
}

void onEdit()
{
    int i = selectedIndex();
    if (i < 0 || i >= (int)donors.size())
    {
        MessageBoxA(hWnd, "Select a donor in the table first.", "Edit", MB_ICONINFORMATION);
        return;
    }
    const Donor& d = donors[i];
    SetWindowTextA(hName, d.name.c_str());
    SetWindowTextA(hContact, d.contactNumber.c_str());
    SetWindowTextA(hGmail, d.gmailAddress.c_str());
    SetWindowTextA(hDate, dateText(d.lastDonationDate).c_str());
    curGender = (d.gender == "F") ? 1 : 0;
    curBT = bloodIndex(d.bloodType);
    editIdx = i;
    redrawGroup(ID_GM, ID_GF);
    redrawGroup(ID_BT0, ID_BT0 + 7);
    setAddButtonText();
    setOut("Editing " + d.name + ".\nChange the details on the left, then click Save Changes.");
}

void onDelete()
{
    int i = selectedIndex();
    if (i < 0 || i >= (int)donors.size())
    {
        MessageBoxA(hWnd, "Select a donor in the table first.", "Delete", MB_ICONINFORMATION);
        return;
    }
    string nm = donors[i].name;
    if (MessageBoxA(hWnd, ("Delete donor \"" + nm + "\" from the database?").c_str(), "Confirm delete",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    donors.erase(donors.begin() + i);
    if (editIdx == i) clearForm();
    else if (editIdx > i) editIdx--;
    setOut("Donor " + nm + " has been deleted from the database.");
    changed();
}

void onEligible()
{
    ostringstream o;
    o << "ELIGIBLE DONORS (can donate today)\n----------------------------------\n";
    int n = 0;
    for (size_t i = 0; i < donors.size(); i++)
    {
        if (!isEligible(donors[i])) continue;
        n++;
        o << donors[i].bloodType << "\t" << donors[i].name << "\t" << donors[i].contactNumber << "\n";
    }
    if (n == 0) o << "Nobody is eligible right now.\n";
    else        o << "\nTotal: " << n << " donor(s)";
    setOut(o.str());
}

void onReport()
{
    int eligPer[8];
    for (int i = 0; i < 8; i++) eligPer[i] = 0;
    for (size_t i = 0; i < donors.size(); i++)
    {
        int b = bloodIndex(donors[i].bloodType);
        if (b >= 0 && isEligible(donors[i])) eligPer[b]++;
    }
    ostringstream o;
    o << "BLOOD GROUP REPORT\n------------------\n";
    for (int i = 0; i < 8; i++)
    {
        char buf[64];
        sprintf(buf, "%-4s ", BLOOD[i]);
        o << buf << string(bloodCount[i], '#') << " " << bloodCount[i]
          << "  (eligible: " << eligPer[i] << ")\n";
    }
    for (int i = 0; i < 8; i++)
        if (bloodCount[i] == 0) { o << "\nShortage: no donor for " << BLOOD[i] << "\n"; }
    setOut(o.str());
}

void onSearch()  { filterText = getText(hSearch); refreshList(); }
void onShowAll()
{
    filterText = ""; curFilter = -1;
    SetWindowTextA(hSearch, "");
    refreshList();
}

void showSelected()
{
    selIdx = selectedIndex();
    InvalidateRect(hWnd, NULL, FALSE);
}

// ---------- build the window ----------
void createControls()
{
    hUI        = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hBold      = CreateFontA(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hSmallB    = CreateFontA(-12, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hCardTitle = CreateFontA(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hTitleF    = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hBig       = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hMono      = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    hBrField   = CreateSolidBrush(C_FIELD);

    updateToday();

    // ================= LEFT CARD : register a donor =================
    addLbl(36, 196, 1, "Register a donor");

    addLbl(36, 232, 0, "FULL NAME");
    hName = field(36, 250, 308, ID_NAME, 0);

    addLbl(36, 290, 0, "GENDER");
    btn("Male",   36, 308, 152, 30, ID_GM);
    btn("Female", 192, 308, 152, 30, ID_GF);

    addLbl(36, 348, 0, "BLOOD TYPE");
    for (int i = 0; i < 8; i++)
        btn(BLOOD[i], 36 + (i % 4) * 79, 366 + (i / 4) * 32, 71, 28, ID_BT0 + i);

    addLbl(36, 436, 0, "CONTACT NUMBER (11 DIGITS)");
    hContact = field(36, 454, 308, ID_CONTACT, 0);

    addLbl(36, 494, 0, "GMAIL ADDRESS");
    hGmail = field(36, 512, 308, ID_GMAIL, 0);

    addLbl(36, 552, 0, "LAST DONATION (DD-MM-YYYY)");
    hDate = field(36, 570, 308, ID_DATE, 0);
    SetWindowTextA(hDate, dateText(today).c_str());

    btn("+  Register Donor", 36, 612, 196, 38, ID_ADD);
    btn("Clear Form",       240, 612, 104, 38, ID_CLEAR);

    // ================= RIGHT CARD : filter tiles + table =================
    btn("", 392, 198, 82, 44, ID_FALL);
    for (int i = 0; i < 8; i++)
        btn("", 392 + (i + 1) * 91, 198, 82, 44, ID_FALL + 1 + i);

    hSearch = field(392, 252, 430, ID_SEARCH, 26);
    btn("Search",   830, 252, 88, 30, ID_BTNSEARCH);
    btn("Show All", 926, 252, 96, 30, ID_SHOWALL);

    hList = mk("LISTBOX", "", WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
               LIST_X, 320, 812, 170, ID_LIST);
    SendMessageA(hList, LB_SETITEMHEIGHT, 0, ROW_H);

    btn("Edit Selected",   392, 500, 197, 36, ID_EDIT);
    btn("Delete Selected", 597, 500, 197, 36, ID_DELETE);
    btn("Eligible Donors", 802, 500, 197, 36, ID_ELIG);
    btn("Blood Report",   1007, 500, 197, 36, ID_REPORT);

    // ================= RIGHT CARD : activity + details =================
    addLbl(392, 546, 1, "Activity");
    hOut = mk("EDIT", "", WS_VSCROLL | ES_MULTILINE | ES_READONLY, 400, 580, 464, 138, ID_OUT);
    SendMessageA(hOut, WM_SETFONT, (WPARAM)hMono, TRUE);
    addLbl(888, 546, 1, "Donor details");

    setOut("Welcome! Register a donor on the left, or click a row in the table.");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
            hWnd = hwnd;
            createControls();
            loadData();
            refreshList();
            SetTimer(hwnd, 1, 60, NULL);        // hover effect polling
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            int W = rc.right, H = rc.bottom;
            HDC mem = CreateCompatibleDC(hdc);                 // double buffering = no flicker
            HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
            HGDIOBJ old = SelectObject(mem, bmp);
            paintAll(mem, W, H);
            BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT* m = (MEASUREITEMSTRUCT*)lParam;
            if (m->CtlType == ODT_LISTBOX) { m->itemHeight = ROW_H; return TRUE; }
            return FALSE;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)lParam;
            if (d->CtlType == ODT_BUTTON)       drawButton(d);
            else if (d->CtlType == ODT_LISTBOX) drawRow(d);
            return TRUE;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, C_DARK);
            SetBkColor(dc, C_FIELD);
            return (LRESULT)hBrField;
        }

        case WM_TIMER:
        {
            POINT p; GetCursorPos(&p);
            HWND h = WindowFromPoint(p);
            int nid = 0;
            if (h && h != hwnd && GetParent(h) == hwnd)
            {
                int cid = GetDlgCtrlID(h);
                if (cid >= ID_GM && cid <= ID_BTN_LAST) nid = cid;
            }
            if (nid != hoverId)
            {
                int old = hoverId;
                hoverId = nid;
                if (old) redrawCtl(old);
                if (nid) redrawCtl(nid);
            }
            return 0;
        }

        case WM_SETCURSOR:
            if (hoverId != 0 && LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_COMMAND:
        {
            int id = LOWORD(wParam), code = HIWORD(wParam);
            if (id == ID_LIST && code == LBN_SELCHANGE) { showSelected(); return 0; }
            if (code != BN_CLICKED) return 0;

            if (id == ID_GM || id == ID_GF)
            {
                curGender = (id == ID_GF) ? 1 : 0;
                redrawGroup(ID_GM, ID_GF);
                return 0;
            }
            if (id >= ID_BT0 && id < ID_BT0 + 8)
            {
                curBT = id - ID_BT0;
                redrawGroup(ID_BT0, ID_BT0 + 7);
                return 0;
            }
            if (id >= ID_FALL && id <= ID_FALL + 8)           // blood group filter tile
            {
                curFilter = id - ID_FALL - 1;
                refreshList();
                return 0;
            }

            switch (id)
            {
                case ID_ADD:       onAdd();       break;
                case ID_CLEAR:     clearForm(); setOut("Form cleared."); break;
                case ID_EDIT:      onEdit();      break;
                case ID_DELETE:    onDelete();    break;
                case ID_ELIG:      onEligible();  break;
                case ID_REPORT:    onReport();    break;
                case ID_BTNSEARCH: onSearch();    break;
                case ID_SHOWALL:   onShowAll();   break;
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            saveData();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    FreeConsole();   // hides the black console window if compiled as a console app

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "BloodBankGUI";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT r = {0, 0, 1240, 760};
    AdjustWindowRect(&r, style, FALSE);

    HWND w = CreateWindowA("BloodBankGUI", "Blood Bank Management System", style,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, hInst, NULL);
    ShowWindow(w, nShow);
    UpdateWindow(w);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0))
    {
        if (!IsDialogMessage(w, &m))
        {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    return (int)m.wParam;
}
