#pragma once
#include "transition_recorder.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include <esp_timer.h>
#include <algorithm>
#include <cstdio>
#include <new>

class TraceCapture {
 public:
  void setup() {
    if (recorder_) return;
    std::snprintf(boot_id_,sizeof(boot_id_),"%08x%08x",
                  unsigned(esphome::random_uint32()),unsigned(esphome::random_uint32()));
    // Capture is optional: allocation failure must not consume camera/internal RAM.
    esphome::RAMAllocator<uint8_t> allocator(esphome::RAMAllocator<uint8_t>::ALLOC_EXTERNAL);
    auto *memory=allocator.allocate(sizeof(heater_trace::Recorder));
    if (!memory) { ESP_LOGW("heater_trace","PSRAM unavailable; trace capture disabled"); return; }
    recorder_=new (memory) heater_trace::Recorder();
    ESP_LOGI("heater_trace","Transition capture ready: %u bytes in PSRAM",unsigned(sizeof(*recorder_)));
  }
  void record(uint32_t now, heater_trace::Kind kind, uint32_t a, uint32_t b,
              float signal, uint8_t flags=0) {
    if (recorder_) recorder_->record({now,a,b,scaled(signal),kind,flags});
  }
  void observe(uint32_t now, const char *status, const char *reason, unsigned fault,
               float threshold) {
    if (!recorder_) return;
    const auto state=heater_trace::status_code(status);
    recorder_->observe(now,state,state==heater_trace::Status::UNKNOWN ?
      heater_trace::reason_code(reason) : heater_trace::Reason::NONE, uint8_t(fault),scaled(threshold));
  }
  void export_page(JsonObject root, int sequence, int offset) {
    root["schema"]=1; root["boot_id"]=boot_id_;
    root["uptime_ms"]=uint64_t(esp_timer_get_time()/1000);
    root["capture_enabled"]=recorder_!=nullptr;
    if (!recorder_) { root["error"]="capture_unavailable"; return; }
    recorder_->tick(uint32_t(millis()));
    root["newest"]=recorder_->newest(); root["oldest"]=recorder_->oldest();
    root["overwritten"]=recorder_->overwritten();
    root["state"]=uint8_t(recorder_->status());
    if (sequence<0 || offset<0) { root["error"]="invalid_page"; return; }
    if (!sequence) {
      root["pre_ms"]=heater_trace::Recorder::PRE_MS;
      root["post_ms"]=heater_trace::Recorder::POST_MS;
      auto ready=root["ready_ids"].to<JsonArray>();
      unsigned pending=0;
      for (uint32_t id=recorder_->oldest();id<=recorder_->newest();++id) {
        const auto *w=recorder_->get(id);
        if (w && w->ready) ready.add(id); else if (w) ++pending;
      }
      root["pending"]=pending; return;
    }
    const auto *w=recorder_->get(uint32_t(sequence));
    if (!w) { root["error"]="trace_evicted_or_missing"; return; }
    if (!w->ready) { root["error"]="trace_pending"; return; }
    if (offset>w->count) { root["error"]="invalid_page"; return; }
    root["id"]=w->id; root["trigger_ms"]=w->trigger;
    root["from"]=uint8_t(w->from); root["to"]=uint8_t(w->to);
    root["reason"]=uint8_t(w->reason); root["fault_code"]=w->fault_code;
    root["threshold10"]=w->threshold10; root["count"]=w->count;
    root["dropped"]=w->dropped; root["pre_truncated"]=w->pre_truncated;
    root["offset"]=offset;
    const int end=std::min(offset+16,int(w->count));
    auto events=root["events"].to<JsonArray>();
    for (int i=offset;i<end;++i) {
      const auto &e=w->events[i]; auto row=events.add<JsonArray>();
      row.add(e.at); row.add(uint8_t(e.kind)); row.add(e.a); row.add(e.b);
      row.add(e.signal10); row.add(e.flags);
    }
    root["next_offset"]=end<w->count ? end : -1;
  }
 private:
  static uint16_t scaled(float f) { return uint16_t(std::min(65535.0f,std::max(0.0f,f*10))); }
  heater_trace::Recorder *recorder_{nullptr};
  char boot_id_[17]{};
};
inline TraceCapture trace_capture;
