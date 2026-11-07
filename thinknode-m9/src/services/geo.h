#pragma once
#include <cmath>
#include <cstdio>
#include <string>

// Contacts page: distance and heading from our GPS fix to a contact whose advert carried a position.
namespace mm {
// Great-circle distance in metres between two WGS84 points given in degrees.
inline double geoDistanceMeters(double lat1,double lon1,double lat2,double lon2){constexpr double R=6371000.0,D=M_PI/180.0;double p1=lat1*D,p2=lat2*D,dp=(lat2-lat1)*D,dl=(lon2-lon1)*D;double a=std::sin(dp/2)*std::sin(dp/2)+std::cos(p1)*std::cos(p2)*std::sin(dl/2)*std::sin(dl/2);return 2*R*std::atan2(std::sqrt(a),std::sqrt(1-a));}
// Initial bearing from point 1 to point 2 in whole degrees (0 = north, clockwise) and its compass point (N, NE, ...).
inline int geoBearing(double lat1,double lon1,double lat2,double lon2){constexpr double D=M_PI/180.0;double p1=lat1*D,p2=lat2*D,dl=(lon2-lon1)*D;double y=std::sin(dl)*std::cos(p2),x=std::cos(p1)*std::sin(p2)-std::sin(p1)*std::cos(p2)*std::cos(dl);double b=std::atan2(y,x)/D;if(b<0)b+=360;return int(std::lround(b))%360;}// rounded: geoCompass gives the old double-based point
inline const char *geoCompass(int bearing){static const char *names[]={"N","NE","E","SE","S","SW","W","NW"};return names[int((bearing+22.5)/45)%8];}
inline const char *geoHeading(double lat1,double lon1,double lat2,double lon2){return geoCompass(geoBearing(lat1,lon1,lat2,lon2));}
// "850 m NE", "2.4 km N", "120 km W" ("2.0 km" with no heading).
inline std::string formatDistance(double meters,const char *heading){char text[24];if(meters<1000)snprintf(text,sizeof(text),"%d m",int(meters+0.5));else if(meters<10000)snprintf(text,sizeof(text),"%.1f km",meters/1000);else snprintf(text,sizeof(text),"%d km",int(meters/1000+0.5));return *heading?std::string(text)+" "+heading:std::string(text);}
}
