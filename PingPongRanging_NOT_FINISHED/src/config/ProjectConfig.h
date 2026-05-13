#pragma once
#include <Arduino.h>

constexpr int     PACKET_SZ   = 4;
constexpr int     TOTAL_CHNL  = 40;
constexpr float   DEFAULT_BW  = 812.5f;
constexpr uint8_t DEFAULT_SF  = 7;
constexpr float   DEFAULT_RF  = 2400.0f;
constexpr int     DEFAULT_SZ  = 256;
constexpr uint8_t DEFAULT_SC  = 3;

inline constexpr int BW[4] = {406250, 812500, 1625000, 203125};

// move secrets out later (NVS/provisioning)
inline constexpr const char WIFI_SSID[] = "yers";
inline constexpr const char WIFI_PASS[] = "12345678";
inline constexpr const char SERVER_URL[] = "http://10.42.0.1:5000/reading";
