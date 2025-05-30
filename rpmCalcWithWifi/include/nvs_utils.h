#ifndef NVS_UTILS_H
#define NVS_UTILS_H

#include <stdint.h>
#include <vector>   // ✅ Fix std::vector error

// These are C++ symbols and must stay outside extern "C"
extern const int MAX_RANGES;
extern int32_t numRanges;
extern int32_t modeRanges[12][2];
extern int32_t modeServoPositions[12];

// Only C-compatible functions go in here
#ifdef __cplusplus
extern "C" {
#endif

void initNVS();
void eraseNVS();
void storeRanges();
bool loadRanges();
void listNVSContents();
void setDefaultRanges();
int determineMode(int rpm);
int getServoPositionForMode(int mode);

#ifdef __cplusplus
}  // extern "C"
#endif

// C++-only function
#ifdef __cplusplus
void updateAndStoreRanges(const std::vector<int>& thresholds);
#endif

#endif  // NVS_UTILS_H