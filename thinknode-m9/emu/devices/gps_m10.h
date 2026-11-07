#pragma once
namespace gpsm10 {
// Position of the emulated MIA-M10Q (degrees, metres); on=false: no module answers on Serial2.
void configure(bool on,double latitude,double longitude,double altitude);
void attach();   // after configure, before setup()
}
