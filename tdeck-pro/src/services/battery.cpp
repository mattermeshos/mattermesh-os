#include "battery.h"
#include <Arduino.h>
#include <Wire.h>
#include "board/board.h"
#include <PowersBQ25896.tpp>
#include <Preferences.h>
namespace device {
static BatteryState state;static uint32_t last=0;static bool initialized=false;
static Preferences calibration;static int fullMillivolts=BATTERY_FULL_MV;
// Read existing charger driver only. Never change charging/voltage/current policy.
static PowersBQ25896 charger(Wire,BOARD_I2C_SDA,BOARD_I2C_SCL,BOARD_I2C_ADDR_BQ25896);
static bool read(uint8_t address,uint8_t reg,uint8_t *bytes,uint8_t n){Wire.beginTransmission(address);Wire.write(reg);if(Wire.endTransmission(false)!=0||Wire.requestFrom(address,n)!=n)return false;for(int i=0;i<n;i++)bytes[i]=Wire.read();return true;}
void batteryLoop(){if(!initialized){calibration.begin("mmbattery",false);int saved=calibration.getInt("full_mv",BATTERY_FULL_MV);if(validFullCalibration(saved))fullMillivolts=saved;}if(initialized&&millis()-last<10000)return;initialized=true;last=millis();uint8_t b[2];state=BatteryState();if(read(BOARD_I2C_ADDR_BQ27220,0x2c,b,2)){int soc=b[0]|(b[1]<<8);if(soc<=100){state.soc=soc;state.gauge=true;}}if(read(BOARD_I2C_ADDR_BQ27220,0x08,b,2)){int v=b[0]|(b[1]<<8);if(v>=2500&&v<=5000)state.voltage=v;}auto status=charger.chargeStatus();if(status!=PowersBQ25896::CHARGE_STATE_UNKOWN){state.charger=true;state.full=status==PowersBQ25896::CHARGE_STATE_DONE;state.charging=status==PowersBQ25896::CHARGE_STATE_PRE_CHARGE||status==PowersBQ25896::CHARGE_STATE_FAST_CHARGE;}if(state.voltage<0){int v=charger.getBattVoltage();if(v>=2500&&v<=5000)state.voltage=v;}state.percent=batteryPercent(state.soc,state.gauge,state.voltage,state.full,fullMillivolts);state.estimated=state.percent>=0&&!state.full;}
PowerSource batteryPowerSource(){auto status=charger.chargeStatus();if(status==PowersBQ25896::CHARGE_STATE_UNKOWN)return PowerSource::Unknown;return charger.isVbusIn()?PowerSource::USB:PowerSource::Battery;}
bool batteryShutdown(){if(batteryPowerSource()!=PowerSource::Battery)return false;charger.shutdown();return true;}
int batteryFullMillivolts(){return fullMillivolts;}
bool batteryCalibrateFull(){
 if(charger.chargeStatus()!=PowersBQ25896::CHARGE_STATE_DONE)return false;
 int sum=0;uint8_t bytes[2];for(int i=0;i<5;i++){if(!read(BOARD_I2C_ADDR_BQ27220,0x08,bytes,2))return false;int mv=bytes[0]|(bytes[1]<<8);if(!validFullCalibration(mv))return false;sum+=mv;delay(10);}
 int measured=(sum+2)/5;if(calibration.putInt("full_mv",measured)!=sizeof(int32_t))return false;fullMillivolts=measured;state.voltage=measured;state.full=true;state.percent=100;state.estimated=false;return true;
}
BatteryState batteryState(){return state;}
}
