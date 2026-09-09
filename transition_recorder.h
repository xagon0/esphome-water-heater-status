#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace heater_trace {
enum class Kind : uint8_t { RISE=1, FALL=2, HEALTH=3, FRAME_GAP=4, IMAGE_ERROR=5,
                            NORMAL_GROUP=6, FAULT_GROUP=7, STATUS=8, RESET=9 };
enum class Status : uint8_t { UNKNOWN=0, IDLE=1, RUNNING=2, FAULT=3, STARTUP=255 };
enum class Reason : uint8_t { NONE=0, ACQUIRING=1, FRAME_GAP=2, STALE=3, SILENCE=4,
                              SOLID=5, PATTERN=6, JPEG_SIZE=7, JPEG_DECODE=8, DISABLED=9 };
struct Event {
  uint32_t at{0}, a{0}, b{0};
  uint16_t signal10{0};
  Kind kind{Kind::HEALTH};
  uint8_t flags{0};
};
static_assert(sizeof(Event)==16, "Keep trace memory bounded");

inline Status status_code(const char *s) {
  if (std::strcmp(s,"Running")==0) return Status::RUNNING;
  if (std::strcmp(s,"Not running")==0) return Status::IDLE;
  if (std::strcmp(s,"Fault")==0) return Status::FAULT;
  return Status::UNKNOWN;
}
inline Reason reason_code(const char *s) {
  if (std::strcmp(s,"Acquiring")==0) return Reason::ACQUIRING;
  if (std::strcmp(s,"Frame gap")==0) return Reason::FRAME_GAP;
  if (std::strcmp(s,"Camera unavailable")==0) return Reason::STALE;
  if (std::strcmp(s,"No recognized pattern")==0) return Reason::SILENCE;
  if (std::strcmp(s,"LED continuously on")==0) return Reason::SOLID;
  if (std::strcmp(s,"Acquiring / unrecognized pattern")==0) return Reason::PATTERN;
  if (std::strcmp(s,"Expected 160x120 JPEG")==0) return Reason::JPEG_SIZE;
  if (std::strcmp(s,"JPEG decode failed")==0) return Reason::JPEG_DECODE;
  if (std::strcmp(s,"Classification disabled")==0) return Reason::DISABLED;
  return Reason::NONE;
}

class Recorder {
 public:
  static constexpr uint32_t PRE_MS=15000, POST_MS=20000;
  static constexpr size_t EVENTS=256, WINDOWS=64;
  struct Window {
    uint32_t id{0}, trigger{0};
    Status from{Status::STARTUP}, to{Status::UNKNOWN};
    Reason reason{Reason::NONE};
    uint8_t fault_code{0};
    uint16_t count{0}, dropped{0}, threshold10{0};
    bool ready{false}, pre_truncated{false};
    Event events[EVENTS]{};
  };

  void tick(uint32_t now) {
    for (auto &w: windows_)
      if (w.id && !w.ready && now-w.trigger>POST_MS) w.ready=true;
  }
  void record(const Event &e) {
    tick(e.at);
    ring_[head_]=e;
    head_=(head_+1)%EVENTS;
    if (ring_count_<EVENTS) ++ring_count_;
    for (auto &w: windows_) {
      if (w.id && !w.ready) append(w,e);
    }
  }
  bool observe(uint32_t now, Status state, Reason reason, uint8_t fault_code,
               uint16_t threshold10) {
    tick(now);
    if (state==last_status_) return false;
    const Status previous=last_status_;
    // Append to prior overlapping windows, then copy into this new window once.
    record({now, uint32_t(previous), uint32_t(state), 0, Kind::STATUS, uint8_t(reason)});
    auto &w=windows_[(next_id_-1)%WINDOWS];
    if (w.id) ++overwritten_;
    // Reset the header only; event slots beyond count are never read/exported.
    w.id=next_id_++; w.trigger=now; w.from=previous; w.to=state; w.reason=reason;
    w.fault_code=fault_code; w.count=0; w.dropped=0; w.threshold10=threshold10;
    w.ready=false;
    const size_t oldest=(head_+EVENTS-ring_count_)%EVENTS;
    w.pre_truncated=ring_count_==EVENTS && now-ring_[oldest].at<PRE_MS;
    for (size_t i=0;i<ring_count_;++i) {
      const auto &e=ring_[(oldest+i)%EVENTS];
      if (now-e.at<=PRE_MS) append(w,e);
    }
    last_status_=state;
    return true;
  }
  const Window *get(uint32_t id) const {
    if (!id) return nullptr;
    const auto &w=windows_[(id-1)%WINDOWS];
    return w.id==id ? &w : nullptr;
  }
  uint32_t newest() const { return next_id_-1; }
  uint32_t oldest() const { return next_id_>WINDOWS ? next_id_-WINDOWS : 1; }
  uint32_t overwritten() const { return overwritten_; }
  Status status() const { return last_status_; }
 private:
  static void append(Window &w, const Event &e) {
    if (w.count<EVENTS) w.events[w.count++]=e;
    else if (w.dropped<65535) ++w.dropped;
  }
  Event ring_[EVENTS]{};
  Window windows_[WINDOWS]{};
  size_t head_{0}, ring_count_{0};
  uint32_t next_id_{1}, overwritten_{0};
  Status last_status_{Status::STARTUP};
};
}  // namespace heater_trace
