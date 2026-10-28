#pragma once
#include <AudioOutputI2S.h>
#include "../board/board.h"
// Single shared PCM5102A I2S output, used by both the notification-tone player (audio.cpp) and the MP3
// player (music.cpp): the ESP32 has one I2S peripheral per port, so only one AudioOutputI2S may own it.
namespace device {
inline AudioOutputI2S &i2sOutput(){
 static AudioOutputI2S out(0,AudioOutputI2S::EXTERNAL_I2S);
 static bool configured=false;
 if(!configured){configured=true;out.SetPinout(BOARD_I2S_BCLK,BOARD_I2S_LRC,BOARD_I2S_DOUT);}
 return out;
}
}
