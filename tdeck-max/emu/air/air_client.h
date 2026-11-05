#pragma once
#include "air_protocol.h"
#include <memory>
#include <mutex>

class AirPort {
public:
  using Start = air::Medium::Start;
  using End = air::Medium::End;
  virtual ~AirPort() = default;
  virtual void transmit(const AirFrame&) = 0;
  virtual void poll() {}
  // Callbacks may originate on the NetAir reader thread. Do not touch GPIO/SPI there.
  void setReceiver(Start start, End end);
protected:
  void rxStart(const AirFrame&, double rssi, double snr);
  void rxEnd(const AirFrame&, bool ok);
private:
  std::mutex callbackMutex_;
  Start start_;
  End end_;
};

class LocalAir : public AirPort {
public:
  using Network = air::Medium;
  explicit LocalAir(std::string name, std::shared_ptr<Network> network);
  ~LocalAir() override;
  void transmit(const AirFrame&) override;
  void poll() override;
private:
  std::string name_;
  std::shared_ptr<Network> network_;
};

class NetAir : public AirPort {
public:
  explicit NetAir(std::string name, uint16_t port = 7300, std::string host = "127.0.0.1");
  ~NetAir() override;
  void transmit(const AirFrame&) override;
  bool connected() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
