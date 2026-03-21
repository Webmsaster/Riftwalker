"""Generate all 74 Riftwalker SFX as .wav files using numpy/scipy.
Each sound is crafted for a roguelike platformer aesthetic (retro but polished).
Run: python tools/generate_sfx.py
Output: assets/sounds/*.wav
"""
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter
import os

RATE = 44100
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "sounds")

def ensure_dir():
    os.makedirs(OUT_DIR, exist_ok=True)

def normalize(sig, vol=0.8):
    mx = np.max(np.abs(sig))
    if mx > 0:
        sig = sig / mx * vol
    return sig

def to_wav(sig, name):
    sig = normalize(sig)
    sig16 = (sig * 32767).astype(np.int16)
    wavfile.write(os.path.join(OUT_DIR, f"{name}.wav"), RATE, sig16)

def sine(freq, dur, vol=1.0):
    t = np.linspace(0, dur, int(RATE * dur), False)
    return np.sin(2 * np.pi * freq * t) * vol

def square(freq, dur, vol=1.0):
    t = np.linspace(0, dur, int(RATE * dur), False)
    return np.sign(np.sin(2 * np.pi * freq * t)) * vol

def saw(freq, dur, vol=1.0):
    t = np.linspace(0, dur, int(RATE * dur), False)
    return (2 * (t * freq - np.floor(t * freq + 0.5))) * vol

def noise(dur, vol=1.0):
    return np.random.uniform(-vol, vol, int(RATE * dur))

def sweep(f0, f1, dur, vol=1.0, wave='sine'):
    t = np.linspace(0, dur, int(RATE * dur), False)
    freq = np.linspace(f0, f1, len(t))
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    if wave == 'square':
        return np.sign(np.sin(phase)) * vol
    return np.sin(phase) * vol

def env_linear(sig, attack=0.01, decay=0.0):
    n = len(sig)
    e = np.ones(n)
    att = int(attack * RATE)
    if att > 0:
        e[:att] = np.linspace(0, 1, att)
    if decay > 0:
        dec = int(decay * RATE)
        if dec > 0 and dec <= n:
            e[-dec:] = np.linspace(1, 0, dec)
    return sig * e

def env_adsr(sig, a=0.01, d=0.05, s=0.7, r=0.1):
    n = len(sig)
    e = np.ones(n)
    ai = int(a * RATE)
    di = int(d * RATE)
    ri = int(r * RATE)
    if ai > 0:
        e[:ai] = np.linspace(0, 1, ai)
    if di > 0 and ai + di < n:
        e[ai:ai+di] = np.linspace(1, s, di)
    if ai + di < n:
        e[ai+di:-ri if ri > 0 else n] = s
    if ri > 0:
        e[-ri:] = np.linspace(s, 0, ri)
    return sig * e

def lowpass(sig, cutoff, order=4):
    b, a = butter(order, cutoff / (RATE / 2), btype='low')
    return lfilter(b, a, sig)

def highpass(sig, cutoff, order=4):
    b, a = butter(order, cutoff / (RATE / 2), btype='high')
    return lfilter(b, a, sig)

def mix(*sigs):
    maxlen = max(len(s) for s in sigs)
    out = np.zeros(maxlen)
    for s in sigs:
        out[:len(s)] += s
    return out

def pad(sig, dur):
    target = int(RATE * dur)
    if len(sig) >= target:
        return sig[:target]
    return np.concatenate([sig, np.zeros(target - len(sig))])

def concat(*sigs):
    return np.concatenate(sigs)

# === PLAYER SOUNDS ===

def player_jump():
    s = sweep(200, 600, 0.12, 0.8) + sweep(150, 500, 0.12, 0.3, 'square')
    return env_linear(s, 0.005, 0.04)

def player_dash():
    s = mix(sweep(300, 80, 0.15, 0.6), noise(0.15, 0.3))
    return env_linear(lowpass(s, 3000), 0.005, 0.06)

def player_land():
    s = mix(sweep(150, 50, 0.08, 0.7), noise(0.08, 0.5))
    return env_linear(lowpass(s, 2000), 0.002, 0.04)

def player_hurt():
    s = mix(sweep(400, 150, 0.2, 0.7), square(100, 0.2, 0.3))
    return env_linear(s, 0.002, 0.1)

def player_death():
    s = mix(sweep(500, 30, 0.6, 0.7), noise(0.6, 0.4), sweep(300, 20, 0.6, 0.3))
    return env_adsr(lowpass(s, 2500), 0.01, 0.1, 0.5, 0.3)

# === COMBAT SOUNDS ===

def melee_swing():
    s = mix(sweep(200, 500, 0.1, 0.5), noise(0.1, 0.4))
    return env_linear(highpass(s, 500), 0.002, 0.04)

def melee_hit():
    s = mix(noise(0.08, 0.8), square(200, 0.08, 0.4), sine(150, 0.08, 0.3))
    return env_linear(lowpass(s, 4000), 0.001, 0.03)

def ranged_shot():
    s = mix(sweep(800, 200, 0.12, 0.6), noise(0.05, 0.5), square(400, 0.06, 0.3))
    return env_linear(s, 0.002, 0.05)

def enemy_hit():
    s = mix(noise(0.06, 0.7), square(250, 0.06, 0.4))
    return env_linear(lowpass(s, 5000), 0.001, 0.02)

def enemy_death():
    s = mix(sweep(400, 60, 0.25, 0.6), noise(0.25, 0.5), sweep(300, 40, 0.25, 0.3))
    return env_adsr(lowpass(s, 3000), 0.005, 0.05, 0.4, 0.15)

def critical_hit():
    s = mix(noise(0.1, 0.8), sweep(600, 1200, 0.1, 0.5), square(300, 0.1, 0.4))
    return env_linear(s, 0.001, 0.04)

def parry():
    s = mix(sine(800, 0.08, 0.7), sine(1200, 0.08, 0.4), noise(0.03, 0.3))
    return env_linear(s, 0.001, 0.03)

def parry_counter():
    s = mix(sweep(400, 1000, 0.12, 0.6), sine(600, 0.12, 0.4), noise(0.05, 0.3))
    return env_linear(s, 0.002, 0.05)

def charged_attack():
    s = mix(sweep(200, 800, 0.2, 0.7), square(300, 0.2, 0.3), noise(0.1, 0.3))
    return env_adsr(s, 0.01, 0.05, 0.6, 0.08)

def combo_milestone():
    s = mix(sine(600, 0.15, 0.5), sine(900, 0.15, 0.4), sine(1200, 0.1, 0.3))
    return env_linear(s, 0.005, 0.06)

def air_juggle():
    s = mix(sweep(300, 800, 0.1, 0.6), sine(500, 0.1, 0.3))
    return env_linear(s, 0.002, 0.04)

def enemy_stun():
    s = mix(sine(300, 0.15, 0.5), sine(450, 0.15, 0.3), noise(0.08, 0.2))
    return env_adsr(s, 0.005, 0.03, 0.4, 0.06)

def ground_slam():
    s = mix(sweep(200, 30, 0.3, 0.8), noise(0.3, 0.6), sine(60, 0.3, 0.4))
    return env_adsr(lowpass(s, 2000), 0.002, 0.05, 0.5, 0.15)

# === DIMENSION & RIFT SOUNDS ===

def dimension_switch():
    s = mix(sweep(1200, 400, 0.2, 0.5), sweep(400, 1200, 0.2, 0.3), sine(600, 0.2, 0.3))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.08)

def rift_repair():
    s = concat(sine(500, 0.1, 0.5), sine(700, 0.1, 0.5), sine(1000, 0.15, 0.6))
    return env_linear(s, 0.005, 0.06)

def rift_fail():
    s = concat(sine(400, 0.1, 0.5), sine(250, 0.15, 0.6))
    return env_linear(s, 0.005, 0.08)

# === UI SOUNDS ===

def pickup():
    s = concat(sine(800, 0.05, 0.5), sine(1000, 0.05, 0.5), sine(1200, 0.06, 0.6))
    return env_linear(s, 0.003, 0.03)

def menu_select():
    s = sine(600, 0.06, 0.4)
    return env_linear(s, 0.002, 0.02)

def menu_confirm():
    s = concat(sine(600, 0.05, 0.4), sine(900, 0.07, 0.5))
    return env_linear(s, 0.003, 0.03)

def level_complete():
    s = concat(sine(500, 0.1, 0.5), sine(700, 0.1, 0.5), sine(900, 0.1, 0.5), sine(1200, 0.2, 0.6))
    return env_linear(s, 0.005, 0.1)

# === HAZARD SOUNDS ===

def collapse_warning():
    s = mix(square(200, 0.4, 0.6), sine(150, 0.4, 0.4))
    return env_adsr(s, 0.01, 0.1, 0.6, 0.15)

def entropy_critical():
    s = mix(sweep(300, 100, 0.3, 0.7), square(150, 0.3, 0.4))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.12)

def spike_damage():
    s = mix(noise(0.06, 0.7), square(180, 0.06, 0.5))
    return env_linear(lowpass(s, 3000), 0.001, 0.02)

def fire_burn():
    s = mix(noise(0.12, 0.5), sweep(200, 400, 0.12, 0.3))
    return env_linear(lowpass(s, 2500), 0.005, 0.05)

def laser_hit():
    s = mix(sweep(1500, 800, 0.08, 0.6), noise(0.04, 0.3))
    return env_linear(s, 0.001, 0.03)

def ice_freeze():
    s = mix(sweep(2000, 500, 0.15, 0.5), noise(0.15, 0.4), sine(1000, 0.1, 0.3))
    return env_adsr(highpass(s, 800), 0.005, 0.03, 0.4, 0.06)

def electric_chain():
    s = mix(noise(0.12, 0.6), sweep(800, 2000, 0.06, 0.4), sweep(2000, 600, 0.06, 0.4))
    return env_linear(s, 0.001, 0.04)

# === BOSS SOUNDS ===

def boss_multishot():
    s = mix(sweep(600, 200, 0.15, 0.6), noise(0.08, 0.5), square(300, 0.1, 0.3))
    return env_linear(s, 0.002, 0.06)

def boss_shield_burst():
    s = mix(sweep(300, 1500, 0.2, 0.7), noise(0.2, 0.5), sine(200, 0.2, 0.3))
    return env_adsr(s, 0.005, 0.05, 0.5, 0.08)

def boss_teleport():
    s = mix(sweep(1500, 200, 0.15, 0.5), sweep(200, 1500, 0.15, 0.3), noise(0.1, 0.2))
    return env_adsr(s, 0.01, 0.03, 0.4, 0.05)

def crawler_drop():
    s = mix(sweep(100, 300, 0.1, 0.6), noise(0.1, 0.4))
    return env_linear(lowpass(s, 2000), 0.002, 0.04)

def summoner_summon():
    s = mix(sweep(200, 600, 0.25, 0.5), sine(400, 0.25, 0.3), noise(0.15, 0.2))
    return env_adsr(s, 0.02, 0.05, 0.4, 0.1)

def sniper_telegraph():
    s = mix(sine(1200, 0.3, 0.4), sine(1800, 0.3, 0.2))
    e = np.ones(len(s))
    # Pulsing effect
    t = np.linspace(0, 0.3, len(s))
    e *= 0.5 + 0.5 * np.sin(2 * np.pi * 8 * t)
    return env_linear(s * e, 0.005, 0.1)

# === VOID WYRM BOSS ===

def wyrm_dive():
    s = mix(sweep(600, 80, 0.3, 0.8), noise(0.3, 0.4), sine(100, 0.3, 0.3))
    return env_adsr(lowpass(s, 2500), 0.01, 0.05, 0.6, 0.12)

def wyrm_poison():
    s = mix(noise(0.2, 0.5), sweep(300, 600, 0.2, 0.4), sine(200, 0.2, 0.3))
    return env_adsr(lowpass(s, 2000), 0.01, 0.05, 0.5, 0.08)

def wyrm_barrage():
    parts = []
    for i in range(5):
        shot = env_linear(mix(sweep(500+i*100, 200, 0.06, 0.5), noise(0.03, 0.3)), 0.001, 0.02)
        parts.append(pad(shot, 0.08))
    return concat(*parts)

# === RIFT SHIELD ===

def rift_shield_activate():
    s = mix(sweep(400, 1200, 0.2, 0.5), sine(800, 0.2, 0.4), noise(0.1, 0.2))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.08)

def rift_shield_absorb():
    s = mix(sine(600, 0.1, 0.5), noise(0.1, 0.3), square(400, 0.1, 0.2))
    return env_linear(s, 0.002, 0.04)

def rift_shield_reflect():
    s = mix(sweep(500, 1500, 0.1, 0.6), sine(1000, 0.1, 0.3))
    return env_linear(s, 0.001, 0.04)

def rift_shield_burst():
    s = mix(sweep(800, 200, 0.2, 0.7), noise(0.2, 0.5), sine(300, 0.2, 0.3))
    return env_adsr(s, 0.005, 0.05, 0.5, 0.08)

# === PHASE STRIKE ===

def phase_strike_teleport():
    s = mix(sweep(2000, 300, 0.12, 0.5), sweep(300, 2000, 0.12, 0.3))
    return env_linear(s, 0.005, 0.04)

def phase_strike_hit():
    s = mix(noise(0.08, 0.7), sweep(800, 400, 0.08, 0.5), sine(300, 0.08, 0.3))
    return env_linear(s, 0.001, 0.03)

# === WORLD INTERACTION ===

def breakable_wall():
    s = mix(noise(0.2, 0.8), sweep(200, 50, 0.2, 0.5), sine(80, 0.15, 0.3))
    return env_adsr(lowpass(s, 3000), 0.002, 0.05, 0.4, 0.1)

def secret_room():
    s = concat(sine(600, 0.1, 0.4), sine(800, 0.1, 0.4), sine(1000, 0.1, 0.5), sine(1300, 0.15, 0.6))
    return env_linear(s, 0.005, 0.07)

def shrine_activate():
    s = mix(sweep(300, 800, 0.25, 0.5), sine(500, 0.25, 0.4), noise(0.1, 0.2))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.1)

def shrine_blessing():
    s = concat(sine(500, 0.08, 0.4), sine(700, 0.08, 0.5), sine(1000, 0.08, 0.5), sine(1400, 0.12, 0.6))
    return env_linear(s, 0.005, 0.05)

def shrine_curse():
    s = concat(sine(500, 0.08, 0.5), sine(350, 0.1, 0.5), sine(200, 0.15, 0.6))
    return env_linear(s, 0.005, 0.08)

def merchant_greet():
    s = concat(sine(400, 0.08, 0.4), sine(500, 0.08, 0.4), sine(600, 0.1, 0.5))
    return env_linear(s, 0.005, 0.04)

def anomaly_spawn():
    s = mix(sweep(100, 800, 0.3, 0.5), noise(0.3, 0.3), sine(300, 0.3, 0.3))
    return env_adsr(s, 0.02, 0.05, 0.4, 0.12)

# === DIMENSIONAL ARCHITECT BOSS ===

def arch_tile_swap():
    s = mix(square(400, 0.1, 0.4), sine(600, 0.1, 0.3), noise(0.05, 0.2))
    return env_linear(s, 0.002, 0.04)

def arch_construct():
    s = mix(sweep(200, 800, 0.2, 0.5), square(300, 0.2, 0.3), noise(0.1, 0.2))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.08)

def arch_rift_open():
    s = mix(sweep(800, 200, 0.25, 0.6), sweep(200, 800, 0.25, 0.3), noise(0.15, 0.3))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.1)

def arch_collapse():
    s = mix(sweep(400, 30, 0.4, 0.8), noise(0.4, 0.6), sine(50, 0.4, 0.4))
    return env_adsr(lowpass(s, 2000), 0.005, 0.08, 0.5, 0.18)

def arch_beam():
    s = mix(sweep(600, 1500, 0.2, 0.5), sine(1000, 0.2, 0.4), noise(0.1, 0.2))
    return env_adsr(s, 0.005, 0.05, 0.6, 0.06)

# === VOID SOVEREIGN BOSS ===

def void_sovereign_orb():
    s = mix(sine(300, 0.15, 0.5), sweep(200, 600, 0.15, 0.4), noise(0.08, 0.2))
    return env_adsr(s, 0.01, 0.03, 0.5, 0.05)

def void_sovereign_slam():
    s = mix(sweep(300, 30, 0.35, 0.8), noise(0.35, 0.5), sine(50, 0.35, 0.5))
    return env_adsr(lowpass(s, 1800), 0.002, 0.08, 0.5, 0.15)

def void_sovereign_teleport():
    s = mix(sweep(2000, 200, 0.15, 0.6), sweep(200, 1800, 0.15, 0.3))
    return env_adsr(s, 0.005, 0.03, 0.4, 0.05)

def void_sovereign_dim_lock():
    s = mix(square(200, 0.3, 0.5), sweep(400, 100, 0.3, 0.4), noise(0.15, 0.3))
    return env_adsr(s, 0.01, 0.08, 0.5, 0.1)

def void_sovereign_storm():
    s = mix(noise(0.5, 0.7), sweep(100, 500, 0.5, 0.4), sweep(500, 100, 0.5, 0.3))
    return env_adsr(lowpass(s, 3000), 0.02, 0.1, 0.6, 0.2)

def void_sovereign_laser():
    s = mix(sweep(1000, 2000, 0.2, 0.6), sine(1500, 0.2, 0.4), noise(0.1, 0.2))
    return env_adsr(s, 0.005, 0.03, 0.6, 0.06)

# === ENTROPY INCARNATE BOSS ===

def entropy_pulse():
    s = mix(sine(150, 0.2, 0.6), sweep(100, 300, 0.2, 0.4), noise(0.1, 0.3))
    return env_adsr(lowpass(s, 2000), 0.01, 0.05, 0.5, 0.08)

def entropy_tendril():
    s = mix(sweep(200, 800, 0.2, 0.5), saw(300, 0.2, 0.3), noise(0.1, 0.2))
    return env_adsr(s, 0.01, 0.05, 0.5, 0.08)

def entropy_missile():
    s = mix(sweep(500, 200, 0.12, 0.6), noise(0.08, 0.4), square(300, 0.08, 0.3))
    return env_linear(s, 0.002, 0.05)

def entropy_shatter():
    s = mix(noise(0.3, 0.8), sweep(500, 30, 0.3, 0.6), sine(60, 0.3, 0.4))
    return env_adsr(lowpass(s, 2500), 0.002, 0.05, 0.5, 0.15)

# === MISC ===

def lore_discover():
    s = concat(sine(400, 0.1, 0.3), sine(600, 0.1, 0.4), sine(500, 0.12, 0.4), sine(800, 0.15, 0.5))
    return env_linear(s, 0.005, 0.07)

def charge_ready():
    s = mix(sine(1000, 0.12, 0.5), sine(1500, 0.12, 0.3))
    return env_linear(s, 0.005, 0.05)

def heartbeat():
    # Lub-dub pattern
    lub = env_linear(mix(sine(55, 0.08, 0.8), sine(80, 0.08, 0.4)), 0.005, 0.03)
    gap = np.zeros(int(RATE * 0.08))
    dub = env_linear(mix(sine(45, 0.06, 0.6), sine(70, 0.06, 0.3)), 0.005, 0.02)
    return concat(lub, gap, dub)

def volume_preview():
    s = sine(800, 0.1, 0.5)
    return env_linear(s, 0.005, 0.04)

def shock_trap():
    s = mix(noise(0.12, 0.6), sweep(1000, 2500, 0.08, 0.4), sweep(2500, 800, 0.08, 0.3))
    return env_linear(s, 0.002, 0.05)

# === MAIN: Map SFX enum names to generators ===

SFX_MAP = {
    "PlayerJump": player_jump,
    "PlayerDash": player_dash,
    "PlayerLand": player_land,
    "MeleeSwing": melee_swing,
    "MeleeHit": melee_hit,
    "RangedShot": ranged_shot,
    "EnemyHit": enemy_hit,
    "EnemyDeath": enemy_death,
    "PlayerHurt": player_hurt,
    "PlayerDeath": player_death,
    "DimensionSwitch": dimension_switch,
    "RiftRepair": rift_repair,
    "RiftFail": rift_fail,
    "Pickup": pickup,
    "MenuSelect": menu_select,
    "MenuConfirm": menu_confirm,
    "LevelComplete": level_complete,
    "CollapseWarning": collapse_warning,
    "SuitEntropyCritical": entropy_critical,
    "SpikeDamage": spike_damage,
    "BossMultiShot": boss_multishot,
    "BossShieldBurst": boss_shield_burst,
    "BossTeleport": boss_teleport,
    "CrawlerDrop": crawler_drop,
    "SummonerSummon": summoner_summon,
    "SniperTelegraph": sniper_telegraph,
    "FireBurn": fire_burn,
    "LaserHit": laser_hit,
    "WyrmDive": wyrm_dive,
    "WyrmPoison": wyrm_poison,
    "WyrmBarrage": wyrm_barrage,
    "IceFreeze": ice_freeze,
    "ElectricChain": electric_chain,
    "Parry": parry,
    "ParryCounter": parry_counter,
    "ChargedAttackRelease": charged_attack,
    "CriticalHit": critical_hit,
    "ComboMilestone": combo_milestone,
    "AirJuggleLaunch": air_juggle,
    "EnemyStun": enemy_stun,
    "GroundSlam": ground_slam,
    "RiftShieldActivate": rift_shield_activate,
    "RiftShieldAbsorb": rift_shield_absorb,
    "RiftShieldReflect": rift_shield_reflect,
    "RiftShieldBurst": rift_shield_burst,
    "PhaseStrikeTeleport": phase_strike_teleport,
    "PhaseStrikeHit": phase_strike_hit,
    "BreakableWall": breakable_wall,
    "SecretRoomDiscover": secret_room,
    "ShrineActivate": shrine_activate,
    "ShrineBlessing": shrine_blessing,
    "ShrineCurse": shrine_curse,
    "MerchantGreet": merchant_greet,
    "AnomalySpawn": anomaly_spawn,
    "ArchTileSwap": arch_tile_swap,
    "ArchConstruct": arch_construct,
    "ArchRiftOpen": arch_rift_open,
    "ArchCollapse": arch_collapse,
    "ArchBeam": arch_beam,
    "VoidSovereignOrb": void_sovereign_orb,
    "VoidSovereignSlam": void_sovereign_slam,
    "VoidSovereignTeleport": void_sovereign_teleport,
    "VoidSovereignDimLock": void_sovereign_dim_lock,
    "VoidSovereignStorm": void_sovereign_storm,
    "VoidSovereignLaser": void_sovereign_laser,
    "EntropyPulse": entropy_pulse,
    "EntropyTendril": entropy_tendril,
    "EntropyMissile": entropy_missile,
    "EntropyShatter": entropy_shatter,
    "LoreDiscover": lore_discover,
    "ChargeReady": charge_ready,
    "Heartbeat": heartbeat,
    "VolumePreview": volume_preview,
    "ShockTrap": shock_trap,
}

if __name__ == "__main__":
    ensure_dir()
    print(f"Generating {len(SFX_MAP)} SFX to {OUT_DIR}...")
    for name, gen in SFX_MAP.items():
        to_wav(gen(), name)
        print(f"  {name}.wav")
    print(f"Done! {len(SFX_MAP)} files generated.")
