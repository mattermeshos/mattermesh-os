#pragma once
#include "emu_hal.h"
#include "air/air_client.h"
#include <memory>
// Semtech LR1110 at SPI level, the command set RadioLib 7.8 and MeshCore's CustomLR1110 use for LoRa (16-bit opcodes,
// replies in a second transaction, IRQ word after the status bytes), on the same emu-air as the SX1262 model.
class Lr1110Model : public emu::SpiDevice {
public:
  Lr1110Model();
  void attach(int cs,int busy,int rst,int dio9,AirPort* air);
  void select(bool selected) override;
  uint8_t transfer(uint8_t out) override;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
