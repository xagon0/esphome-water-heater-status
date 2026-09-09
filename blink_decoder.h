#pragma once
#include <cstdint>

// WT8840-family interpretation: single flash ~3 s = idle; rapid double
// strobe ~3 s = call for heat. This is the control's report, not flame proof.
class BlinkDecoder {
 public:
  void reset() {
    initialized_ = have_group_ = pending_ = high_ = false;
    streak_ = kind_ = flashes_ = confirmed_ = 0;
    state_ = "Acquiring";
  }

  void sample(uint32_t now, bool high) {
    if (!initialized_) {
      initialized_ = true;
      last_frame_ = high_since_ = now;
      high_ = high;
      return;
    }
    if (now - last_frame_ > 150) {
      reset();
      initialized_ = true;
      last_frame_ = high_since_ = now;
      high_ = high;
      state_ = "Frame gap";
      return;
    }
    last_frame_ = now;
    finish_group(now);
    if (high && !high_) {
      high_since_ = now;
      if (!pending_) {
        group_period_ = have_group_ ? now - group_start_ : 0;
        group_start_ = now;
        have_group_ = pending_ = true;
        flashes_ = 1;
      } else {
        const uint32_t spacing = now - last_edge_;
        if (spacing < 50 || spacing > 600) flashes_ = 99;
        else if (flashes_ < 99) ++flashes_;
      }
      last_edge_ = now;
    }
    high_ = high;
    if (high && now - high_since_ > 500) {
      state_ = "LED continuously on";
      streak_ = kind_ = confirmed_ = 0;
      pending_ = false;
    }
    expire(now);
  }

  const char *state(uint32_t now) {
    if (!initialized_ || now - last_frame_ > 500) return "Camera unavailable";
    finish_group(now);
    expire(now);
    return state_;
  }

 private:
  void finish_group(uint32_t now) {
    if (!pending_ || high_ || now - last_edge_ <= 650) return;
    pending_ = false;
    const unsigned candidate = (flashes_ == 1 || flashes_ == 2) ? flashes_ : 0;
    // Require four complete, matching groups. Fault codes with one-second
    // spacing cannot establish a run of rapid doubles or 3-second singles.
    if (candidate && candidate == kind_ && group_period_ >= 2700 && group_period_ <= 3600) {
      if (streak_ < 4) ++streak_;
    } else {
      kind_ = candidate;
      streak_ = candidate ? 1 : 0;
    }
    // Keep the established result while a different valid normal pattern
    // is acquiring. One missed half-strobe must not cause state flicker.
    // Irregular timing, extra flashes, frame loss and silence still clear it.
    if (!candidate || (group_period_ && (group_period_ < 2700 || group_period_ > 3600))) confirmed_ = 0;
    if (streak_ >= 4) confirmed_ = kind_;
    state_ = confirmed_ ? (confirmed_ == 1 ? "Standby" : "Heating") : "Acquiring / unrecognized pattern";
  }
  void expire(uint32_t now) {
    if (have_group_ && !high_ && now - last_edge_ > 4500) {
      state_ = "No recognized pattern";
      streak_ = kind_ = confirmed_ = 0;
    }
  }
  bool initialized_{false}, have_group_{false}, pending_{false}, high_{false};
  unsigned streak_{0}, kind_{0}, flashes_{0}, confirmed_{0};
  uint32_t last_frame_{0}, last_edge_{0}, high_since_{0}, group_start_{0}, group_period_{0};
  const char *state_{"Acquiring"};
};

// Recognize documented WT8840 abnormal codes without confusing the fast
// normal heating strobe with two slow (one-second-spaced) fault flashes.
class FaultDecoder {
 public:
  void reset() {
    initialized_ = high_ = pending_ = have_edge_ = false;
    count_ = streak_ = code_ = 0;
  }
  void sample(uint32_t now, bool high) {
    if (!initialized_ || now - last_frame_ > 150) {
      reset(); initialized_ = true; high_ = high;
      last_frame_ = high_since_ = now; return;
    }
    last_frame_ = now;
    finish_group(now);
    if (high && !high_) {
      high_since_ = now;
      if (!pending_) {
        const uint32_t gap = now - last_edge_;
        gap_valid_ = !have_edge_ || (gap >= 2500 && gap <= 4500);
        pending_ = slow_ = true;
        count_ = 1;
      } else {
        const uint32_t spacing = now - last_edge_;
        if (spacing < 750 || spacing > 1250) slow_ = false;
        if (count_ < 255) ++count_;
        if (!slow_ || count_ > 8) streak_ = code_ = 0;
      }
      have_edge_ = true;
      last_edge_ = now;
    }
    high_ = high;
    if (high && now - high_since_ > 500) { streak_ = code_ = 0; pending_ = false; }
    if (have_edge_ && now - last_edge_ > 5500) streak_ = code_ = 0;
  }
  bool active(uint32_t now) {
    if (!initialized_ || now - last_frame_ > 500 || now - last_edge_ > 5500) return false;
    finish_group(now);
    return code_ != 0;
  }
  unsigned code() const { return code_; }
 private:
  static bool known(unsigned count) { return count == 2 || count == 4 || count == 5 || count == 7 || count == 8; }
  void finish_group(uint32_t now) {
    if (!pending_ || high_ || now - last_edge_ <= 1500) return;
    pending_ = false;
    if (slow_ && gap_valid_ && known(count_)) {
      if (streak_ < 2) ++streak_;
      if (streak_ >= 2) code_ = count_;
    } else {
      streak_ = code_ = 0;
    }
  }
  bool initialized_{false}, high_{false}, pending_{false}, have_edge_{false};
  bool slow_{false}, gap_valid_{false};
  unsigned count_{0}, streak_{0}, code_{0};
  uint32_t last_frame_{0}, last_edge_{0}, high_since_{0};
};
