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

inline constexpr std::array<DimensionShiftFloorBalance, 30> kDimensionShiftFloorBalance = {{
    // === ZONE 1: Fractured Threshold (Floors 1-6) — Tutorial & onboarding ===
    // Rift counts reduced for pacing: fewer rifts = faster floors, more combat focus
    // Floor 1: tutorial floor, mostly DIM-A and low punishment.
    {2, 1, 0.20f, 0.08f, 1.05f, 2.0f},
    // Floor 2: still onboarding, introduce one meaningful DIM-B pull.
    {2, 1, 0.25f, 0.12f, 1.08f, 2.5f},
    // Floor 3: first mid-boss test, gentle DIM-B introduction.
    {3, 1, 0.30f, 0.15f, 1.10f, 3.0f},
    // Floor 4: deliberate shifts start, player learns risk/reward.
    {3, 1, 0.35f, 0.20f, 1.15f, 3.5f},
    // Floor 5: near-even split, reward starts to matter.
    {3, 1, 0.40f, 0.25f, 1.18f, 4.0f},
    // Floor 6: zone boss — meaningful DIM-B test.
    {4, 0, 0.45f, 0.30f, 1.22f, 5.0f},

    // === ZONE 2: Shifting Depths (Floors 7-12) — Escalation ===
    // Floor 7: breather after zone boss, but stakes rising.
    {3, 0, 0.45f, 0.35f, 1.25f, 5.5f},
    // Floor 8: hazards intensify, DIM-B becomes rewarding.
    {4, 0, 0.50f, 0.40f, 1.30f, 6.0f},
    // Floor 9: mid-boss with real DIM-B pressure.
    {4, 0, 0.50f, 0.48f, 1.35f, 6.5f},
    // Floor 10: DIM-B pulls are now standard.
    {4, 0, 0.55f, 0.55f, 1.40f, 7.0f},
    // Floor 11: near-dominance of DIM-B objectives.
    {4, 0, 0.60f, 0.62f, 1.45f, 8.0f},
    // Floor 12: zone boss — DIM-B mastery check.
    {5, 0, 0.65f, 0.70f, 1.50f, 9.0f},

    // === ZONE 3: Resonant Core (Floors 13-18) — Midgame peak ===
    // Floor 13: breather, but all enemy types unlocked.
    {4, 0, 0.60f, 0.65f, 1.50f, 9.0f},
    // Floor 14: DIM-B is now the norm, DIM-A is the escape.
    {5, 0, 0.65f, 0.75f, 1.55f, 10.0f},
    // Floor 15: mid-boss, heavy DIM-B pressure.
    {5, 0, 0.68f, 0.82f, 1.60f, 10.5f},
    // Floor 16: high-risk high-reward.
    {5, 0, 0.70f, 0.90f, 1.65f, 11.0f},
    // Floor 17: near-total DIM-B commitment.
    {5, 0, 0.72f, 0.98f, 1.70f, 12.0f},
    // Floor 18: zone boss — ultimate DIM-B mastery.
    {6, 0, 0.75f, 1.05f, 1.75f, 13.0f},

    // === ZONE 4: Entropy Cascade (Floors 19-24) — Endgame ===
    // Floor 19: breather, but entropy pressure starts building.
    {5, 0, 0.72f, 1.00f, 1.75f, 13.0f},
    // Floor 20: DIM-B dominates, entropy drains fast.
    {5, 0, 0.75f, 1.10f, 1.80f, 14.0f},
    // Floor 21: mid-boss, punishing entropy.
    {6, 0, 0.78f, 1.20f, 1.85f, 15.0f},
    // Floor 22: near-total DIM-B, massive rewards.
    {6, 0, 0.80f, 1.30f, 1.90f, 16.0f},
    // Floor 23: almost all rifts in DIM-B.
    {6, 0, 0.82f, 1.40f, 1.95f, 17.0f},
    // Floor 24: zone boss — entropy gauntlet.
    {6, 0, 0.85f, 1.50f, 2.00f, 18.0f},

    // === ZONE 5: The Sovereign's Domain (Floors 25-30) — Finale ===
    // Floor 25: breather before the end. Still brutal.
    {6, 0, 0.82f, 1.45f, 2.00f, 18.0f},
    // Floor 26: DIM-B is almost mandatory for rewards.
    {6, 0, 0.85f, 1.55f, 2.10f, 19.0f},
    // Floor 27: mid-boss, dual threat.
    {6, 0, 0.87f, 1.65f, 2.20f, 20.0f},
    // Floor 28: maximum entropy pressure.
    {7, 0, 0.88f, 1.75f, 2.30f, 22.0f},
    // Floor 29: penultimate floor — survival gauntlet.
    {7, 0, 0.90f, 1.85f, 2.40f, 24.0f},
    // Floor 30: finale — the Sovereign awaits.
    {7, 0, 0.92f, 2.00f, 2.50f, 26.0f},
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

// === Zone system: 5 zones of 6 floors each ===
inline int getZone(int floor) {
    return std::clamp((floor - 1) / 6, 0, 4); // 0-4
}

inline int getFloorInZone(int floor) {
    return ((floor - 1) % 6) + 1; // 1-6
}

inline bool isBossFloor(int floor) {
    return floor > 0 && floor % 6 == 0; // Floor 6, 12, 18, 24, 30
}

inline bool isMidBossFloor(int floor) {
    return floor > 0 && floor % 6 == 3; // Floor 3, 9, 15, 21, 27
}

inline bool isBreatherFloor(int floor) {
    // First floor of each zone (except zone 1) is a breather after the zone boss
    int fiz = getFloorInZone(floor);
    return fiz == 1 && getZone(floor) > 0; // Floor 7, 13, 19, 25
}

// Zone-based stat multipliers for enemies (logarithmic scaling)
// Returns {hpMult, dmgMult, speedMult}
struct ZoneScaling {
    float hpMult;
    float dmgMult;
    float speedMult;
    float eliteChance;   // 0-100
    float miniBossChance; // 0-100
};

inline ZoneScaling getZoneScaling(int floor) {
    int zone = getZone(floor);
    int fiz = getFloorInZone(floor);

    // Base multipliers per zone (logarithmic growth: big early jumps, smaller later)
    constexpr float zoneHP[]    = {1.0f, 1.4f, 1.9f, 2.5f, 3.2f};
    constexpr float zoneDMG[]   = {1.0f, 1.25f, 1.55f, 1.9f, 2.3f};
    constexpr float zoneSpeed[] = {1.0f, 1.05f, 1.10f, 1.15f, 1.20f};
    constexpr float zoneElite[] = {5.0f, 15.0f, 25.0f, 35.0f, 45.0f};
    constexpr float zoneMini[]  = {0.0f, 10.0f, 20.0f, 35.0f, 50.0f};

    // Within-zone scaling: gradual ramp within the zone (+5% HP, +3% DMG per floor-in-zone)
    float inZoneHP  = 1.0f + (fiz - 1) * 0.05f;
    float inZoneDMG = 1.0f + (fiz - 1) * 0.03f;
    float inZoneSpd = 1.0f + (fiz - 1) * 0.01f;

    // Breather floors are slightly easier (0.85x of zone baseline)
    float breatherMult = isBreatherFloor(floor) ? 0.85f : 1.0f;

    return {
        zoneHP[zone] * inZoneHP * breatherMult,
        zoneDMG[zone] * inZoneDMG * breatherMult,
        zoneSpeed[zone] * inZoneSpd,
        zoneElite[zone] + fiz * 2.0f,
        zoneMini[zone] + fiz * 3.0f,
    };
}
