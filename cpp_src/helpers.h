#pragma once

#include <functional>
#include <optional>
#include <string>

// Pointers
#define uptr std::unique_ptr
#define sptr std::shared_ptr
#define opto std::optional

const std::string PANSI = "\033[";

// Reset
inline std::string ansi_reset() { return PANSI + "0m"; }

// Text Attributes
inline std::string ansi_bold() { return PANSI + "1m"; }
inline std::string ansi_faint() { return PANSI + "2m"; }
inline std::string ansi_italic() {
  return PANSI + "3m";
} // Not widely supported
inline std::string ansi_underline() { return PANSI + "4m"; }
inline std::string ansi_slow_blink() { return PANSI + "5m"; }
inline std::string ansi_rapid_blink() { return PANSI + "6m"; }
inline std::string ansi_inverse() { return PANSI + "7m"; }
inline std::string ansi_conceal() { return PANSI + "8m"; }
inline std::string ansi_crossed_out() { return PANSI + "9m"; }

// Text Attribute Off
inline std::string ansi_bold_off() {
  return PANSI + "21m";
} // Also 22m for faint off
inline std::string ansi_italic_off() { return PANSI + "23m"; }
inline std::string ansi_underline_off() { return PANSI + "24m"; }
inline std::string ansi_blink_off() { return PANSI + "25m"; }
inline std::string ansi_inverse_off() { return PANSI + "27m"; }
inline std::string ansi_conceal_off() { return PANSI + "28m"; }
inline std::string ansi_crossed_out_off() { return PANSI + "29m"; }

// Foreground Colors
inline std::string ansi_black() { return PANSI + "30m"; }
inline std::string ansi_red() { return PANSI + "31m"; }
inline std::string ansi_green() { return PANSI + "32m"; }
inline std::string ansi_yellow() { return PANSI + "33m"; }
inline std::string ansi_blue() { return PANSI + "34m"; }
inline std::string ansi_magenta() { return PANSI + "35m"; }
inline std::string ansi_cyan() { return PANSI + "36m"; }
inline std::string ansi_white() { return PANSI + "37m"; }
inline std::string ansi_default_foreground() { return PANSI + "39m"; }

// Background Colors
inline std::string ansi_bg_black() { return PANSI + "40m"; }
inline std::string ansi_bg_red() { return PANSI + "41m"; }
inline std::string ansi_bg_green() { return PANSI + "42m"; }
inline std::string ansi_bg_yellow() { return PANSI + "43m"; }
inline std::string ansi_bg_blue() { return PANSI + "44m"; }
inline std::string ansi_bg_magenta() { return PANSI + "45m"; }
inline std::string ansi_bg_cyan() { return PANSI + "46m"; }
inline std::string ansi_bg_white() { return PANSI + "47m"; }
inline std::string ansi_default_background() { return PANSI + "49m"; }

// Bright Foreground Colors
inline std::string ansi_bright_black() { return PANSI + "90m"; }
inline std::string ansi_bright_red() { return PANSI + "91m"; }
inline std::string ansi_bright_green() { return PANSI + "92m"; }
inline std::string ansi_bright_yellow() { return PANSI + "93m"; }
inline std::string ansi_bright_blue() { return PANSI + "94m"; }
inline std::string ansi_bright_magenta() { return PANSI + "95m"; }
inline std::string ansi_bright_cyan() { return PANSI + "96m"; }
inline std::string ansi_bright_white() { return PANSI + "97m"; }

// Bright Background Colors
inline std::string ansi_bg_bright_black() { return PANSI + "100m"; }
inline std::string ansi_bg_bright_red() { return PANSI + "101m"; }
inline std::string ansi_bg_bright_green() { return PANSI + "102m"; }
inline std::string ansi_bg_bright_yellow() { return PANSI + "103m"; }
inline std::string ansi_bg_bright_blue() { return PANSI + "104m"; }
inline std::string ansi_bg_bright_magenta() { return PANSI + "105m"; }
inline std::string ansi_bg_bright_cyan() { return PANSI + "106m"; }
inline std::string ansi_bg_bright_white() { return PANSI + "107m"; }
