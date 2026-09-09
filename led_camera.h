#pragma once
#include "blink_decoder.h"
#include "trace_export.h"
#include "jpeg_decoder.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

class LedCamera {
 public:
  BlinkDecoder decoder;
  FaultDecoder fault_decoder;
  float signal{0}, brightness{0}, fps{0};
  uint32_t last_interval{0}, last_width{0}, pulse_count{0}, long_gaps{0};
  const char *image_error{nullptr};

  void reset() { decoder.reset(); fault_decoder.reset(); lit_ = false; }
  float take_peak() { float value = peak_; peak_ = 0; return value; }
  float frame_rate(uint32_t now) const { return now - last_frame_at_ > 500 ? 0 : fps; }

  void process(const uint8_t *data, size_t length, uint32_t now,
               float threshold,
               bool enabled) {
    // Validate input dimensions and give the JPEG decoder a bounded output buffer.
    if (!is_160x120_jpeg(data, length)) {
      if (image_error == nullptr || std::strcmp(image_error,"Expected 160x120 JPEG")!=0)
        trace_capture.record(now, heater_trace::Kind::IMAGE_ERROR, 7, 0, signal);
      image_error = "Expected 160x120 JPEG";
      reset();
      return;
    }
    // Decode a 20x15 overview of the entire frame. A full-frame LED does
    // not need 19,200 RGB pixels; the smaller decode catches shorter pulses.
    esp_jpeg_image_cfg_t cfg{};
    cfg.indata = const_cast<uint8_t *>(data);
    cfg.indata_size = length;
    cfg.outbuf = rgb_;
    cfg.outbuf_size = sizeof(rgb_);
    cfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
    cfg.out_scale = JPEG_IMAGE_SCALE_1_8;
    cfg.advanced.working_buffer = jpeg_work_;
    cfg.advanced.working_buffer_size = sizeof(jpeg_work_);
    esp_jpeg_image_output_t decoded{};
    if (esp_jpeg_decode(&cfg, &decoded) != ESP_OK || decoded.width != 20 || decoded.height != 15) {
      if (image_error == nullptr || std::strcmp(image_error,"JPEG decode failed")!=0)
        trace_capture.record(now, heater_trace::Kind::IMAGE_ERROR, 8, 0, signal);
      image_error = "JPEG decode failed"; reset(); return;
    }
    if (image_error != nullptr) trace_capture.record(now, heater_trace::Kind::IMAGE_ERROR, 0, 0, signal);
    image_error = nullptr;
    uint32_t green_sum = 0, light_sum = 0;
    for (int i = 0; i < 20 * 15; ++i) {
      const uint8_t *p = rgb_ + 3 * i;
      green_sum += std::max(0, int(p[1]) - int(std::max(p[0], p[2])));
      light_sum += (uint32_t(p[0]) + p[1] + p[2]);
    }
    signal = green_sum / float(20 * 15);
    brightness = light_sum / float(20 * 15 * 3);
    peak_ = std::max(peak_, signal);
    if (last_frame_at_ && now - last_frame_at_ > 150) {
      ++long_gaps;
      trace_capture.record(now, heater_trace::Kind::FRAME_GAP, now-last_frame_at_, long_gaps, signal);
    }
    last_frame_at_ = now;
    bool previous_lit = lit_;
    if (lit_) { if (signal < threshold * 0.6f) lit_ = false; }
    else if (signal >= threshold) lit_ = true;
    if (lit_ && !previous_lit) {
      if (have_rising_) last_interval = now - last_rising_;
      last_rising_ = now;
      have_rising_ = true;
      ++pulse_count;
      trace_capture.record(now, heater_trace::Kind::RISE, last_interval, pulse_count, signal);
      ESP_LOGD("led_timing", "Flash %u: interval %u ms, signal %.1f", unsigned(pulse_count), unsigned(last_interval), signal);
    } else if (!lit_ && previous_lit && have_rising_) {
      last_width = now - last_rising_;
      trace_capture.record(now, heater_trace::Kind::FALL, last_width, pulse_count, signal);
      ESP_LOGD("led_timing", "Flash width %u ms", unsigned(last_width));
    }
    ++frames_;
    if (now - fps_time_ >= 2000) {
      fps = 1000.0f * frames_ / (now - fps_time_);
      frames_ = 0; fps_time_ = now;
    }
    if (enabled) { decoder.sample(now, lit_); fault_decoder.sample(now, lit_); }
    else { decoder.reset(); fault_decoder.reset(); }
    const auto &normal = decoder.observation();
    if (normal.sequence != last_normal_sequence_) {
      last_normal_sequence_ = normal.sequence;
      trace_capture.record(now, heater_trace::Kind::NORMAL_GROUP, normal.period,
        uint32_t(normal.flashes) | (uint32_t(normal.streak)<<8) | (uint32_t(normal.confirmed)<<16), signal, normal.valid);
    }
    const auto &fault = fault_decoder.observation();
    if (fault.sequence != last_fault_sequence_) {
      last_fault_sequence_ = fault.sequence;
      trace_capture.record(now, heater_trace::Kind::FAULT_GROUP, fault.period,
        uint32_t(fault.flashes) | (uint32_t(fault.streak)<<8) | (uint32_t(fault.confirmed)<<16), signal, fault.valid);
    }
    if (now-last_trace_health_ >= 1000) {
      last_trace_health_ = now;
      trace_capture.record(now, heater_trace::Kind::HEALTH, uint32_t(fps*100), long_gaps, signal, enabled);
    }
  }

 private:
  // Check JPEG dimensions before decoding. A resolution edit fails closed.
  static bool is_160x120_jpeg(const uint8_t *d, size_t n) {
    if (n < 4 || d[0] != 0xff || d[1] != 0xd8) return false;
    size_t i = 2;
    while (i + 3 < n) {
      if (d[i++] != 0xff) return false;
      while (i < n && d[i] == 0xff) ++i;
      if (i >= n) return false;
      const uint8_t marker = d[i++];
      if (marker == 0xda || marker == 0xd9) return false;
      if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;
      if (i + 2 > n) return false;
      size_t len = (size_t(d[i]) << 8) | d[i + 1];
      if (len < 2 || len > n - i) return false;
      if (marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
        return len >= 8 && ((d[i + 3] << 8) | d[i + 4]) == 120 &&
               ((d[i + 5] << 8) | d[i + 6]) == 160;
      }
      i += len;
    }
    return false;
  }
  uint8_t rgb_[20 * 15 * 3]{};
  uint8_t jpeg_work_[3100]{};
  bool lit_{false};
  uint32_t frames_{0}, fps_time_{0}, last_rising_{0};
  bool have_rising_{false};
  float peak_{0};
  uint32_t last_frame_at_{0};
  uint32_t last_normal_sequence_{0}, last_fault_sequence_{0}, last_trace_health_{0};
};
inline LedCamera led_camera;
