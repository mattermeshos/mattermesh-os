#pragma once
// Local time zone for the clocks on screen and for the date / time typed by hand. GPS and Wi-Fi give UTC and the mesh
// carries UTC, so the zone only changes what is shown and how a typed time is read. Saved in NVS ("mmtime"/"tz") as
// the zone id, so the list can be reordered or grow; new zones take a new id.
namespace device {
struct TimeZone {int id;const char *name,*offset,*posix;};
// West to east, as a list is read.
constexpr TimeZone TIME_ZONES[]={
 {15,"Hawaii","UTC-10","HST10"},
 {16,"Alaska","UTC-9, summer -8","AKST9AKDT,M3.2.0,M11.1.0"},
 {17,"US Pacific","UTC-8, summer -7","PST8PDT,M3.2.0,M11.1.0"},
 {18,"US Mountain","UTC-7, summer -6","MST7MDT,M3.2.0,M11.1.0"},
 {19,"Arizona","UTC-7","MST7"},
 {20,"US Central","UTC-6, summer -5","CST6CDT,M3.2.0,M11.1.0"},
 {21,"Mexico City","UTC-6","CST6"},
 {22,"US Eastern","UTC-5, summer -4","EST5EDT,M3.2.0,M11.1.0"},
 {23,"Atlantic Canada","UTC-4, summer -3","AST4ADT,M3.2.0,M11.1.0"},
 {24,"Brazil, Argentina","UTC-3","<-03>3"},
 {0,"UTC","UTC+0","UTC0"},
 {1,"London, Dublin, Lisbon","UTC+0, summer +1","GMT0BST,M3.5.0/1,M10.5.0"},
 {2,"Rome, Paris, Berlin","UTC+1, summer +2","CET-1CEST,M3.5.0,M10.5.0/3"},
 {25,"South Africa","UTC+2","SAST-2"},
 {3,"Athens, Kyiv, Helsinki","UTC+2, summer +3","EET-2EEST,M3.5.0/3,M10.5.0/4"},
 {4,"Istanbul, Moscow","UTC+3","<+03>-3"},
 {5,"Dubai","UTC+4","<+04>-4"},
 {6,"India","UTC+5:30","IST-5:30"},
 {7,"Bangkok, Jakarta","UTC+7","<+07>-7"},
 {8,"China, Singapore","UTC+8","CST-8"},
 {10,"Perth","UTC+8","AWST-8"},
 {9,"Japan, Korea","UTC+9","JST-9"},
 {11,"Adelaide","UTC+9:30, summer +10:30","ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
 {12,"Brisbane","UTC+10","AEST-10"},
 {13,"Sydney, Melbourne","UTC+10, summer +11","AEST-10AEDT,M10.1.0,M4.1.0/3"},
 {14,"New Zealand","UTC+12, summer +13","NZST-12NZDT,M9.5.0,M4.1.0/3"},
};
constexpr int TIME_ZONE_COUNT=sizeof(TIME_ZONES)/sizeof(TIME_ZONES[0]),TIME_ZONE_DEFAULT=12;// Rome, Paris, Berlin
int timeZone();               // index into TIME_ZONES (NVS, default Central Europe)
void setTimeZone(int index);  // saves it and applies it to the clock at once (clamped to the list)
const char *timeZonePosix();  // the TZ string in use
}
