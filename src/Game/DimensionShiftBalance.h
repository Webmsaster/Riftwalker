#pragma once

#include <algorithm>
#include <array>

struct DimensionShiftFloorBalance {
    int riftCount = 3;
    int safeLeadRifts = 0;
    float dimBRatio = 0.5f;
    float dimBEntropyPerSecond = 0.0f;
    float dimBShardMultiplier = 1.0f;
    float dimBEntropyRepairBonus = 0.0f;
};

inline constexpr std::array<DimensionShiftFloorBalance, 12> kDimensionShiftFloorBalance = {{
    // Floor 1: tutorial floor, mostly DIM-A and low punishment.
    {3, 2, 0.25f, 0.10f, 1.05f, 2.0f},
    // Floor 2: still onboarding, introduce one meaningful DIM-B pull.
    {4, 2, 0.25f, 0.15f, 1.10f, 3.0f},
    // Floor 3: first boss test, one volatile objective is enough.
    {4, 1, 0.35f, 0.20f, 1.15f, 3.0f},
    // Floor 4: deliberate shifts start.
    {5, 1, 0.40f, 0.30f, 1.20f, 4.0f},
    // Floor 5: near-even split, reward starts to matter.
    {5, 0, 0.50f, 0.38f, 1.25f, 5.0f},
    // Floor 6: first real build/shift check.
    {6, 0, 0.50f, 0.50f, 1.30f, 6.0f},
    // Floor 7: risk/reward becomes central.
    {6, 0, 0.60f, 0.65f, 1.40f, 7.0f},
    // Floor 8: DIM-B dominance is now normal.
    {6, 0, 0.65f, 0.75f, 1.45f, 8.0f},
    // Floor 9: hard boss/skill test under real DIM-B pressure.
    {6, 0, 0.70f, 0.90f, 1.55f, 9.0f},
    // Floor 10: endgame pressure, DIM-B is common and valuable.
    {7, 0, 0.72f, 1.05f, 1.65f, 10.0f},
    // Floor 11: nearly all late objectives lean into DIM-B.
    {7, 0, 0.78f, 1.15f, 1.75f, 12.0f},
    // Floor 12: finale, DIM-B dominates but DIM-A still exists as relief.
    {6, 0, 0.83f, 1.30f, 1.85f, 14.0f},
}};

inline const DimensionShiftFloorBalance& getDimensionShiftFloorBalance(int floor) {
    int clampedFloor = std::clamp(floor, 1, static_cast<int>(kDimensionShiftFloorBalance.size()));
    return kDimensionShiftFloorBalance[clampedFloor - 1];
}

inline int getDimensionShiftRiftCount(int floor) {
    return getDimensionShiftFloorBalance(floor).riftCount;
}

inline int getDimensionShiftDimBRiftCount(int floor, int totalRifts) {
    if (totalRifts <= 0) return 0;

    const auto& balance = getDimensionShiftFloorBalance(floor);
    int safeLeadRifts = std::clamp(balance.safeLeadRifts, 0, totalRifts);
    int maxDimBRifts = totalRifts - safeLeadRifts;
    if (maxDimBRifts <= 0 || balance.dimBRatio <= 0.0f) return 0;

    int dimBRifts = static_cast<int>(balance.dimBRatio * static_cast<float>(totalRifts) + 0.5f);
    return std::clamp(dimBRifts, 1, maxDimBRifts);
}

inline int getDimensionShiftRequiredDimension(int floor, int totalRifts, int riftIndex) {
    if (totalRifts <= 0) return 1;
    if (riftIndex < 0) return 1;

    const auto& balance = getDimensionShiftFloorBalance(floor);
    int safeLeadRifts = std::clamp(balance.safeLeadRifts, 0, totalRifts);
    int dimBRifts = getDimensionShiftDimBRiftCount(floor, totalRifts);
    if (dimBRifts <= 0 || riftIndex < safeLeadRifts) return 1;

    int dimSlots = totalRifts - safeLeadRifts;
    if (dimSlots <= 0) return 1;

    int localIndex = std::clamp(riftIndex - safeLeadRifts, 0, dimSlots - 1);
    int beforeQuota = (localIndex * dimBRifts) / dimSlots;
    int afterQuota = ((localIndex + 1) * dimBRifts) / dimSlots;
    return afterQuota > beforeQuota ? 2 : 1;
}

inline int getDimensionShiftDimBShardBonusPercent(int floor) {
    const auto& balance = getDimensionShiftFloorBalance(floor);
    return std::max(0, static_cast<int>((balance.dimBShardMultiplier - 1.0f) * 100.0f + 0.5f));
}
