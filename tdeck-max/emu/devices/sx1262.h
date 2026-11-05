#pragma once
#include "emu_hal.h"
#include "air/air_client.h"
#include <memory>

class Sx1262Model : public emu::SpiDevice {
public:
  Sx1262Model();
  ~Sx1262Model() override;
  Sx1262Model(const Sx1262Model&) = delete;
  Sx1262Model& operator=(const Sx1262Model&) = delete;
  // Main thread; air (if non-null) must outlive the model. One model per AirPort.
  void attach(int cs, int busy, int rst, int dio1, AirPort* air);
  // Optional board power domain (defaults on for existing boards/tests). Main thread only.
  void setPowered(bool powered);
  void select(bool selected) override;
  uint8_t transfer(uint8_t out) override;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
