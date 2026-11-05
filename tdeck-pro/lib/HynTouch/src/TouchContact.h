#pragma once
#include <stdint.h>
struct HynTouchContact {
 bool down=false;int16_t x=0,y=0;
 void report(bool positionReport,uint8_t count,int16_t px,int16_t py){if(!positionReport)return;down=count>0;if(down){x=px;y=py;}}
 void reset(){down=false;x=y=0;}
};
