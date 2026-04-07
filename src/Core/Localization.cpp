#include "Localization.h"

Localization& Localization::instance() {
    static Localization inst;
    return inst;
}

void Localization::setLanguage(Lang lang) {
    m_lang = lang;
}

const char* Localization::get(const char* key) const {
    auto& table = (m_lang == Lang::DE) ? m_de : m_en;
    auto it = table.find(key);
    if (it != table.end()) return it->second.c_str();
    // Fallback to English if key missing in current language
    if (m_lang != Lang::EN) {
        auto enIt = m_en.find(key);
        if (enIt != m_en.end()) return enIt->second.c_str();
    }
    // Last resort: return the key itself
    return key;
}

void Localization::loadStrings() {
    // ===== ENGLISH =====

    // Main Menu
    m_en["menu.new_run"]           = "New Run";
    m_en["menu.daily_run"]         = "Daily Run";
    m_en["menu.daily_leaderboard"] = "Daily Leaderboard";
    m_en["menu.challenges"]        = "Challenges";
    m_en["menu.upgrades"]          = "Upgrades";
    m_en["menu.bestiary"]          = "Bestiary";
    m_en["menu.achievements"]      = "Achievements";
    m_en["menu.lore"]              = "Lore";
    m_en["menu.run_history"]       = "Run History";
    m_en["menu.credits"]           = "Credits";
    m_en["menu.options"]           = "Options";
    m_en["menu.quit"]              = "Quit";
    m_en["menu.tutorial"]          = "Tutorial";
    m_en["menu.rift_shards"]       = "Rift Shards: %d";
    m_en["menu.ascension"]         = "Ascension %d";
    m_en["menu.rift_cores"]        = "Rift Cores: %d";
    m_en["menu.nav_hint"]          = "W/S Navigate  |  ENTER Select  |  ESC Quit";

    // Pause Menu
    m_en["pause.title"]            = "P A U S E D";
    m_en["pause.resume"]           = "Resume";
    m_en["pause.restart"]          = "Restart Run";
    m_en["pause.restart_confirm"]  = "Restart? Progress lost!";
    m_en["pause.daily_leaderboard"]= "Daily Leaderboard";
    m_en["pause.options"]          = "Options";
    m_en["pause.abandon"]          = "Abandon Run";
    m_en["pause.abandon_confirm"]  = "Confirm Abandon?";
    m_en["pause.quit_menu"]        = "Quit to Menu";
    m_en["pause.nav_hint"]         = "ESC Resume  |  W/S Navigate  |  ENTER Select";

    // Pause Stats
    m_en["pause.run_stats"]        = "RUN STATS";
    m_en["pause.floor"]            = "Floor: %d / Zone %d";
    m_en["pause.level"]            = "Level: %d  (%d/%d XP)";
    m_en["pause.difficulty"]       = "Difficulty: %s";
    m_en["pause.diff_easy"]        = "Easy";
    m_en["pause.diff_normal"]      = "Normal";
    m_en["pause.diff_hard"]        = "Hard";
    m_en["pause.boss_floor"]       = "** BOSS FLOOR **";
    m_en["pause.time"]             = "Time: %d:%02d";
    m_en["pause.kills"]            = "Kills: %d";
    m_en["pause.rifts_repaired"]   = "Rifts Repaired: %d";
    m_en["pause.shards"]           = "Shards: %d";
    m_en["pause.best_combo"]       = "Best Combo: %d";
    m_en["pause.equipment"]        = "EQUIPMENT";
    m_en["pause.class"]            = "Class: %s";
    m_en["pause.melee"]            = "Melee: %s";
    m_en["pause.ranged"]           = "Ranged: %s";
    m_en["pause.hp"]               = "HP: %.0f / %.0f";
    m_en["pause.relics"]           = "RELICS";

    // Options
    m_en["options.title"]          = "O P T I O N S";
    m_en["options.master_volume"]  = "Master Volume";
    m_en["options.sfx_volume"]     = "SFX Volume";
    m_en["options.music_volume"]   = "Music Volume";
    m_en["options.mute"]           = "Mute";
    m_en["options.fullscreen"]     = "Fullscreen";
    m_en["options.resolution"]     = "Resolution";
    m_en["options.screen_shake"]   = "Screen Shake";
    m_en["options.hud_opacity"]    = "HUD Opacity";
    m_en["options.rumble"]         = "Controller Rumble";
    m_en["options.colorblind"]     = "Color Blind Mode";
    m_en["options.hud_scale"]      = "HUD Scale";
    m_en["options.controls"]       = "Controls";
    m_en["options.reset"]          = "Reset Defaults";
    m_en["options.back"]           = "Back";
    m_en["options.language"]       = "Language";
    m_en["options.lang_en"]        = "English";
    m_en["options.lang_de"]        = "Deutsch";
    m_en["options.on"]             = "ON";
    m_en["options.off"]            = "OFF";
    m_en["options.crt_effect"]     = "CRT Effect";
    m_en["options.nav_hint"]       = "A/D Adjust  |  W/S Navigate  |  ENTER Toggle  |  ESC Back";

    // Game Over
    m_en["gameover.title"]         = "S U I T  C R A S H";
    m_en["gameover.death_hp"]      = "Hull integrity failure - HP depleted";
    m_en["gameover.death_entropy"] = "Suit entropy reached critical level";
    m_en["gameover.death_rift"]    = "Dimensional rift collapsed";
    m_en["gameover.death_time"]    = "Time expired - speedrun failed";
    m_en["gameover.floor_stats"]   = "Floor %d  |  %d Kills  |  %d:%02d";
    m_en["gameover.critical"]      = "CRITICAL";
    m_en["gameover.continue"]      = "Press any key to return";
    m_en["gameover.suit_failure"]  = "SUIT FAILURE";

    // Run Summary
    m_en["summary.title"]          = "R U N  E N D E D";
    m_en["summary.rooms_cleared"]  = "Rooms Cleared";
    m_en["summary.enemies_defeated"]= "Enemies Defeated";
    m_en["summary.rifts_repaired"] = "Rifts Repaired";
    m_en["summary.shards_earned"]  = "Shards Earned";
    m_en["summary.relics_collected"]= "Relics Collected";
    m_en["summary.best_combo"]     = "Best Combo";
    m_en["summary.dim_switches"]   = "Dim Switches";
    m_en["summary.damage_taken"]   = "Damage Taken";
    m_en["summary.aerial_kills"]   = "Aerial Kills";
    m_en["summary.dash_kills"]     = "Dash Kills";
    m_en["summary.total_runs"]     = "Total Runs";
    m_en["summary.grade"]          = "Grade: %s";
    m_en["summary.cause"]          = "Cause: %s";
    m_en["summary.balance"]        = "Balance Summary";
    m_en["summary.new_record"]     = "NEW RECORD!";
    m_en["summary.daily_score"]    = "DAILY SCORE";
    m_en["summary.new_best"]       = "NEW BEST!";
    m_en["summary.rank_today"]     = "Rank: #%d today";
    m_en["summary.leaderboard_hint"]= "Press L to view Leaderboard";
    m_en["summary.continue"]       = "Press ENTER to continue";

    // HUD
    m_en["hud.collapse"]           = "COLLAPSE: %d.%d";
    m_en["hud.exit"]               = "EXIT";
    m_en["hud.level_up"]           = "LEVEL UP!";
    m_en["hud.lore_discovered"]    = "LORE DISCOVERED";
    m_en["hud.chain_complete"]     = "CHAIN COMPLETE!";
    m_en["hud.rift_stabilized"]    = "RIFT STABILIZED - Warping to next dimension...";

    // Menu info panels
    m_en["menu.daily_date"]        = "Date: %s";
    m_en["menu.daily_seed"]        = "Seed: %d";
    m_en["menu.daily_mutator"]     = "Mutator: %s";
    m_en["menu.daily_best"]        = "Best: %d";
    m_en["menu.daily_no_best"]     = "Best: ---";
    m_en["menu.career_stats"]      = "CAREER STATS";
    m_en["menu.stats_runs"]        = "Runs: %d  |  Best Floor: %d";
    m_en["menu.stats_kills"]       = "Kills: %d  |  Rifts: %d";
    m_en["menu.stats_shards"]      = "Shards: %d";
    m_en["menu.stats_playtime"]    = "Playtime: %dh %dm";
    m_en["menu.stats_victories"]   = "Victories: %d";

    // HUD notifications
    m_en["hud.achievement"]        = "Achievement: %s";
    m_en["hud.reward"]             = "Reward: %s";
    m_en["hud.quest"]              = "Quest: %s (%d/%d)";
    m_en["hud.quest_complete"]     = "Quest Complete! +%d Shards";
    m_en["hud.repair_rift"]        = "Press F to repair rift";
    m_en["hud.score"]              = "Score: %d";
    m_en["hud.defeat_boss"]        = "Defeat the boss to unlock exit!";
    m_en["hud.repair_rifts"]       = "Repair all rifts to unlock exit!";
    m_en["hud.rift_dimb"]          = "This rift stabilizes in DIM-B. +%d%% shards, -%.0f entropy on repair, +%.2f entropy/s.";
    m_en["hud.rift_dima"]          = "This rift stabilizes in DIM-A. Safer route, no DIM-B pressure.";
    m_en["hud.wave_cleared"]       = "WAVE CLEARED";
    m_en["hud.shift_dimb"]         = "Shift to DIM-B: +%d%% shards, -%.0f entropy on repair";
    m_en["hud.shift_dima"]         = "Shift to DIM-A to stabilize this rift";
    m_en["hud.repair_dimb"]        = "Press F to repair volatile DIM-B rift (+%d%% shards, -%.0f entropy)";
    m_en["hud.repair_dima"]        = "Press F to repair stable DIM-A rift";
    m_en["relic.cursed"]           = "CURSED";
    m_en["menu.version"]           = "v0.1  -  Made with SDL2";
    m_en["pause.ngplus"]           = "New Game+ %d";
    m_en["pause.mastery"]          = "  [%s] %d kills";
    m_en["mastery.familiar"]       = "Familiar";
    m_en["mastery.proficient"]     = "Proficient";
    m_en["mastery.mastered"]       = "Mastered";

    // Class Select
    m_en["class.title"]            = "C H O O S E  Y O U R  C L A S S";
    m_en["class.locked"]           = "LOCKED";
    m_en["class.nav_hint"]         = "A/D Switch  |  ENTER Select  |  ESC Back";
    m_en["class.hp"]               = "HP: %.0f";
    m_en["class.speed"]            = "Speed: %.0f";

    // Class descriptions & passives
    m_en["class.voidwalker.desc"]      = "Master of dimensional rifts. Switching dimensions heals and recharges faster.";
    m_en["class.voidwalker.passive"]   = "Dimensional Affinity";
    m_en["class.voidwalker.pdesc"]     = "Dim-switch heals 3 HP, -30% switch cooldown, +30% DMG for 2.5s";
    m_en["class.voidwalker.ability"]   = "Phase Strike: +50% range";
    m_en["class.berserker.desc"]       = "Raging warrior. Grows stronger as health drops. Ground Slam devastates.";
    m_en["class.berserker.passive"]    = "Blood Rage";
    m_en["class.berserker.pdesc"]      = "Under 40% HP: +30% DMG, +20% attack speed";
    m_en["class.berserker.ability"]    = "Ground Slam: +40% radius, +0.4s stun";
    m_en["class.phantom.desc"]         = "Swift shadow. Phases through enemies during dash. Rift Shield enhanced.";
    m_en["class.phantom.passive"]      = "Shadow Step";
    m_en["class.phantom.pdesc"]        = "Dash 50% longer, phase through enemies, 0.5s invisible after";
    m_en["class.phantom.ability"]      = "Rift Shield: +1 max hit, speed boost on absorb";
    m_en["class.technomancer.desc"]    = "Tech engineer. Deploys turrets and shock traps. Constructs deal +20% DMG.";
    m_en["class.technomancer.passive"] = "Construct Mastery";
    m_en["class.technomancer.pdesc"]   = "Turrets/traps +20% DMG, last 50% longer, +10% ranged DMG";
    m_en["class.technomancer.ability"] = "Deploy Turret (Q) / Shock Trap (E)";

    // Class unlock requirements
    m_en["class.unlock.voidwalker"]    = "Always available";
    m_en["class.unlock.berserker"]     = "Kill 50 enemies total across all runs";
    m_en["class.unlock.phantom"]       = "Complete floor 3 in any run";
    m_en["class.unlock.technomancer"]  = "Repair 30 rifts total across all runs";

    // Upgrade names & descriptions (indexed by UpgradeID)
    // Shop buffs (RunBuffSystem)
    m_en["buff.0.name"]  = "HP Surge";           m_en["buff.0.desc"]  = "+30 Max HP";
    m_en["buff.1.name"]  = "Temporal Haste";     m_en["buff.1.desc"]  = "-25% Ability Cooldowns";
    m_en["buff.2.name"]  = "Entropy Damper";     m_en["buff.2.desc"]  = "-30% Entropy Gain";
    m_en["buff.3.name"]  = "Critical Edge";      m_en["buff.3.desc"]  = "+20% Crit Chance";
    m_en["buff.4.name"]  = "Shard Doubler";      m_en["buff.4.desc"]  = "2x Shard Gain";
    m_en["buff.5.name"]  = "Kill Rush";          m_en["buff.5.desc"]  = "Kill = Dash CD Reset";
    m_en["buff.6.name"]  = "Ember Blade";        m_en["buff.6.desc"]  = "Attacks Burn Enemies";
    m_en["buff.7.name"]  = "Frost Edge";         m_en["buff.7.desc"]  = "Attacks Slow Enemies";
    m_en["buff.8.name"]  = "Storm Striker";      m_en["buff.8.desc"]  = "Attacks Chain Lightning";
    m_en["buff.9.name"]  = "Vampire Blade";      m_en["buff.9.desc"]  = "5% Lifesteal on Hit";
    m_en["buff.10.name"] = "Soul Anchor";        m_en["buff.10.desc"] = "Revive Once at 50% HP";
    m_en["buff.11.name"] = "Phantom Step";       m_en["buff.11.desc"] = "Invincible After Dim-Switch";

    // Persistent upgrades
    m_en["upgrade.0.name"]  = "Reinforced Suit";     m_en["upgrade.0.desc"]  = "+20 Max HP";
    m_en["upgrade.1.name"]  = "Quantum Boots";       m_en["upgrade.1.desc"]  = "+10% Move Speed";
    m_en["upgrade.2.name"]  = "Phase Thrusters";     m_en["upgrade.2.desc"]  = "-15% Dash Cooldown";
    m_en["upgrade.3.name"]  = "Rift Blade";          m_en["upgrade.3.desc"]  = "+15% Melee Damage";
    m_en["upgrade.4.name"]  = "Void Projector";      m_en["upgrade.4.desc"]  = "+15% Ranged Damage";
    m_en["upgrade.5.name"]  = "Entropy Shield";      m_en["upgrade.5.desc"]  = "-20% Entropy Gain";
    m_en["upgrade.6.name"]  = "Auto-Stabilizer";     m_en["upgrade.6.desc"]  = "+Passive Entropy Decay";
    m_en["upgrade.7.name"]  = "Gravity Modulator";   m_en["upgrade.7.desc"]  = "+10% Jump Height";
    m_en["upgrade.8.name"]  = "Air Dash Module";     m_en["upgrade.8.desc"]  = "+1 Extra Jump";
    m_en["upgrade.9.name"]  = "Rift Capacitor";      m_en["upgrade.9.desc"]  = "-20% Switch Cooldown";
    m_en["upgrade.10.name"] = "Dimensional Plating"; m_en["upgrade.10.desc"] = "+10% Damage Reduction";
    m_en["upgrade.11.name"] = "Combo Amplifier";     m_en["upgrade.11.desc"] = "+Combo Damage Bonus";
    m_en["upgrade.12.name"] = "Grip Enhancer";       m_en["upgrade.12.desc"] = "Better Wall Slide Control";
    m_en["upgrade.13.name"] = "Rift Resonance";      m_en["upgrade.13.desc"] = "+10% Critical Chance";
    m_en["upgrade.14.name"] = "Shard Magnet";        m_en["upgrade.14.desc"] = "+Pickup Range";
    m_en["upgrade.15.name"] = "Temporal Flux";       m_en["upgrade.15.desc"] = "-15% Ability Cooldowns";
    m_en["upgrade.16.name"] = "Rift Amplifier";      m_en["upgrade.16.desc"] = "+20% Ability Damage";
    m_en["upgrade.17.name"] = "Barrier Core";        m_en["upgrade.17.desc"] = "+1 Shield Capacity";

    // Challenge modes
    m_en["challenge.1.name"] = "Speedrun";          m_en["challenge.1.desc"] = "10-minute timer. Beat the clock!";
    m_en["challenge.2.name"] = "Glass Cannon";       m_en["challenge.2.desc"] = "1 HP, 3x DMG, no healing";
    m_en["challenge.3.name"] = "Boss Rush";          m_en["challenge.3.desc"] = "Only boss fights";
    m_en["challenge.4.name"] = "Endless Rift";       m_en["challenge.4.desc"] = "Infinite levels, rising difficulty";
    m_en["challenge.5.name"] = "Dimension Lock";     m_en["challenge.5.desc"] = "No dimension switching";

    // Mutators
    m_en["mutator.1.name"] = "Big Head";             m_en["mutator.1.desc"] = "All hitboxes +50%";
    m_en["mutator.2.name"] = "Fragile World";        m_en["mutator.2.desc"] = "All tiles breakable";
    m_en["mutator.3.name"] = "Shard Storm";          m_en["mutator.3.desc"] = "3x shards, 2x enemies";
    m_en["mutator.4.name"] = "Dim Flux";             m_en["mutator.4.desc"] = "Auto-switch every 15s";
    m_en["mutator.5.name"] = "Bullet Hell";          m_en["mutator.5.desc"] = "All enemies shoot";
    m_en["mutator.6.name"] = "Low Gravity";          m_en["mutator.6.desc"] = "Half gravity, high jumps";

    // Difficulty Select
    m_en["difficulty.title"]       = "S E L E C T  D I F F I C U L T Y";
    m_en["difficulty.easy"]        = "E A S Y";
    m_en["difficulty.normal"]      = "N O R M A L";
    m_en["difficulty.hard"]        = "H A R D";
    m_en["difficulty.easy_desc"]   = "Fewer enemies, +50% shard drops";
    m_en["difficulty.normal_desc"] = "Standard experience";
    m_en["difficulty.hard_desc"]   = "More enemies, stronger bosses, -25% shards";
    m_en["difficulty.class"]       = "Class: %s";
    m_en["difficulty.nav_hint"]    = "W/S Navigate  |  ENTER Select  |  ESC Back";

    // Achievements
    m_en["achievements.title"]     = "A C H I E V E M E N T S";
    m_en["achievements.counter"]   = "%d / %d unlocked";
    m_en["achievements.nav_hint"]  = "[W/S] Scroll   [ESC] Back";

    // Bestiary
    m_en["bestiary.title"]         = "B E S T I A R Y";
    m_en["bestiary.discovered"]    = "Discovered: %d / %d";
    m_en["bestiary.kills"]         = "Kills: %d";
    m_en["bestiary.unencountered"] = "Unencountered";
    m_en["bestiary.lore"]          = "LORE:";
    m_en["bestiary.abilities"]     = "ABILITIES:";
    m_en["bestiary.weakness"]      = "WEAKNESS:";
    m_en["bestiary.effective"]     = "EFFECTIVE VS:";
    m_en["bestiary.unknown_boss"]  = "Unknown Boss";
    m_en["bestiary.unknown_enemy"] = "Unknown Enemy";
    m_en["bestiary.encounter"]     = "Encounter this enemy to reveal its data.";
    m_en["bestiary.defeat"]        = "Defeat it to permanently unlock its entry.";
    m_en["bestiary.nav_hint"]      = "W/S  Navigate     ESC  Back";
    m_en["bestiary.abilities_locked"] = "ABILITIES:   [ LOCKED ]";
    m_en["bestiary.weakness_locked"]  = "WEAKNESS:  [ LOCKED ]";
    m_en["element.fire"]           = "Fire";
    m_en["element.ice"]            = "Ice";
    m_en["element.electric"]       = "Electric";
    m_en["element.none"]           = "None";

    // Challenges
    m_en["challenges.title"]       = "C H A L L E N G E S";
    m_en["challenges.mutators"]    = "Mutators (max 2)";
    m_en["challenges.nav_hint"]    = "W/S Navigate  |  A/D Switch  |  ENTER Select/Toggle  |  ESC Back";

    // NG+ Select
    m_en["ngplus.title"]           = "N E W  G A M E +";
    m_en["ngplus.subtitle"]        = "Select challenge tier  (higher = harder + more shards)";
    m_en["ngplus.next_challenge"]  = "[NEXT CHALLENGE]";
    m_en["ngplus.nav_hint"]        = "W/S Navigate  |  ENTER Confirm  |  ESC Back";
    m_en["ngplus.tier.0"]          = "N O R M A L";
    m_en["ngplus.tier.1"]          = "N G + 1";
    m_en["ngplus.tier.2"]          = "N G + 2";
    m_en["ngplus.tier.3"]          = "N G + 3";
    m_en["ngplus.tier.4"]          = "N G + 4";
    m_en["ngplus.tier.5"]          = "N G + 5";
    m_en["ngplus.tier.6"]          = "N G + 6";
    m_en["ngplus.tier.7"]          = "N G + 7";
    m_en["ngplus.tier.8"]          = "N G + 8";
    m_en["ngplus.tier.9"]          = "N G + 9";
    m_en["ngplus.tier.10"]         = "N G + 10";
    m_en["ngplus.desc.0"]          = "Standard difficulty";
    m_en["ngplus.desc.1"]          = "+10% enemy HP, slower entropy decay";
    m_en["ngplus.desc.2"]          = "+15% enemy DMG, +1 relic choice at boss";
    m_en["ngplus.desc.3"]          = "+20% elite spawn, keep weapon upgrades";
    m_en["ngplus.desc.4"]          = "Boss +20% HP, extra shop slot";
    m_en["ngplus.desc.5"]          = "Harder patterns, start with common relic";
    m_en["ngplus.desc.6"]          = "+15% shard gain, traps +30%";
    m_en["ngplus.desc.7"]          = "Abilities: 50% CD start, boss extra phase";
    m_en["ngplus.desc.8"]          = "Start with 2 relics, elites get 2 modifiers";
    m_en["ngplus.desc.9"]          = "Shop -20% prices, enemies +30% speed";
    m_en["ngplus.desc.10"]         = "Start with legendary relic — CHAOS MODE";
    m_en["ngplus.reward.1"]        = "Reward: +20% shard bonus";
    m_en["ngplus.reward.2"]        = "Reward: +40% shard bonus";
    m_en["ngplus.reward.3"]        = "Reward: +60% shard bonus";
    m_en["ngplus.reward.4"]        = "Reward: +80% shard bonus";
    m_en["ngplus.reward.5"]        = "Reward: +100% shard bonus";
    m_en["ngplus.reward.6"]        = "Reward: +120% shard bonus";
    m_en["ngplus.reward.7"]        = "Reward: +150% shard bonus";
    m_en["ngplus.reward.8"]        = "Reward: +180% shard bonus";
    m_en["ngplus.reward.9"]        = "Reward: +200% shard bonus";
    m_en["ngplus.reward.10"]       = "Reward: +250% shard bonus  |  CHAOS MASTER title";

    // Keybindings
    m_en["keybindings.title"]      = "C O N T R O L S";
    m_en["keybindings.reset"]      = "Reset Defaults";
    m_en["keybindings.back"]       = "Back";
    m_en["keybindings.listen_hint"]= "Press a key to bind  |  ESC Cancel";
    m_en["keybindings.nav_hint"]   = "W/S Navigate  |  ENTER Rebind  |  ESC Back";

    // Daily Leaderboard
    m_en["daily.title"]            = "DAILY LEADERBOARD";
    m_en["daily.today_seed"]       = "Today: %s  |  Seed: %d";
    m_en["daily.best_today"]       = "Best Today: %d";
    m_en["daily.no_runs"]          = "No runs today yet";
    m_en["daily.score"]            = "Score";
    m_en["daily.class"]            = "Class";
    m_en["daily.floors"]           = "Floors";
    m_en["daily.kills"]            = "Kills";
    m_en["daily.rifts"]            = "Rifts";
    m_en["daily.combo"]            = "Combo";
    m_en["daily.time"]             = "Time";
    m_en["daily.result"]           = "Result";
    m_en["daily.victory"]          = "Victory";
    m_en["daily.fallen"]           = "Fallen";
    m_en["daily.no_runs_recorded"] = "No runs recorded for today. Play a Daily Run!";
    m_en["daily.previous_days"]    = "Previous Days";
    m_en["daily.no_history"]       = "No history yet";
    m_en["daily.back_hint"]        = "[ESC] Back";

    // Lore
    m_en["lore.title"]             = "~ CODEX ~";
    m_en["lore.fragments"]         = "%d / %d Fragments";
    m_en["lore.undiscovered"]      = "This fragment has not been discovered yet.";
    m_en["lore.nav_hint"]          = "W/S: Navigate   ESC: Back";

    // Tutorial hints (context-based, first run)
    m_en["tut.move"]               = "Move";
    m_en["tut.jump"]               = "Jump";
    m_en["tut.dash"]               = "Dash through danger (also in air!)";
    m_en["tut.melee"]              = "Melee Attack";
    m_en["tut.ranged"]             = "Ranged Attack (hold for charged shot!)";
    m_en["tut.dimension"]          = "Switch Dimension - paths differ between worlds!";
    m_en["tut.rift_repair"]        = "Repair this Rift! Solve the puzzle to reduce Entropy.";
    m_en["tut.entropy_warning"]    = "Entropy rising! Repair Rifts to lower it. At 100% your suit fails!";
    m_en["tut.conveyor"]           = "Conveyor Belt - pushes you along. Use it for speed!";
    m_en["tut.dim_platform"]       = "Glowing platform - only exists in one dimension! Switch with [E]";
    m_en["tut.wall_slide"]         = "Wall Slide! Jump off walls to reach higher areas.";
    m_en["tut.abilities"]          = "Use special abilities: Slam / Shield / Phase Strike";
    m_en["tut.relic_choice"]       = "Choose a Relic! Each gives a unique passive bonus for this run.";
    m_en["tut.combo"]              = "Combo x3! Chain hits without pause for bonus damage!";
    m_en["tut.exit"]               = "All Rifts repaired! Find the EXIT before the dimension collapses!";
    m_en["tut.weapon_switch"]      = "Switch weapons: [Q] Melee  [R] Ranged - try different styles!";
    m_en["tut.rift_dimb"]          = "This Rift only stabilizes in DIM-B. Shift with [E], but entropy rises faster there.";
    m_en["tut.rift_dima"]          = "This Rift only stabilizes in DIM-A. Shift with [E] to secure it safely.";
    m_en["tut.collapsing"]         = "Dimension collapsing! Reach the EXIT before time runs out!";

    // Tutorial state pages
    m_en["tut.nav_hint"]                = "A/D or Arrow Keys to navigate | ENTER to continue | ESC to skip";
    m_en["tut.page.welcome.title"]      = "Welcome to Riftwalker";
    m_en["tut.page.welcome.1"]          = "You are a Riftwalker — an explorer of fractured dimensions.";
    m_en["tut.page.welcome.2"]          = "Reality is tearing apart. Dimensional rifts threaten to consume everything.";
    m_en["tut.page.welcome.3"]          = "Your mission: descend through 30 floors, repair rifts, and stop the collapse.";
    m_en["tut.page.welcome.4"]          = "This is a roguelike: death is permanent, but knowledge persists.";
    m_en["tut.page.welcome.5"]          = "Each run makes you stronger. Learn, adapt, overcome.";

    m_en["tut.page.controls.title"]     = "Movement & Controls";
    m_en["tut.page.controls.move"]      = "Move with WASD or Arrow Keys";
    m_en["tut.page.controls.jump"]      = "Jump (also double-jump in air!)";
    m_en["tut.page.controls.dash"]      = "Dash through danger — invincible for a moment";
    m_en["tut.page.controls.wall"]      = "Slide on walls and wall-jump to reach higher areas.";

    m_en["tut.page.combat.title"]       = "Combat";
    m_en["tut.page.combat.melee"]       = "Melee Attack — fast, close range";
    m_en["tut.page.combat.ranged"]      = "Ranged Attack — hold for charged shots";
    m_en["tut.page.combat.parry"]       = "Time your melee right before an enemy hits — PARRY for bonus damage!";
    m_en["tut.page.combat.combo"]       = "Chain hits without pause to build combos for increasing damage.";
    m_en["tut.page.combat.weapons"]     = "Switch weapons to find your style:";

    m_en["tut.page.dim.title"]          = "Dimension Shifting";
    m_en["tut.page.dim.core"]           = "The core mechanic — switch between two parallel dimensions:";
    m_en["tut.page.dim.a_label"]        = "Dimension A";
    m_en["tut.page.dim.a_desc"]         = "Familiar terrain, weaker enemies";
    m_en["tut.page.dim.a_desc2"]        = "Lower entropy growth";
    m_en["tut.page.dim.b_label"]        = "Dimension B";
    m_en["tut.page.dim.b_desc"]         = "Harder enemies, better rewards";
    m_en["tut.page.dim.b_desc2"]        = "Faster entropy growth";
    m_en["tut.page.dim.entropy"]        = "Watch your Entropy meter! At 100%, your suit destabilizes.";
    m_en["tut.page.dim.rifts"]          = "Repair dimensional rifts to lower entropy:";

    m_en["tut.page.prog.title"]         = "Progression & Strategy";
    m_en["tut.page.prog.roguelike"]     = "Death ends the run — but upgrades carry over!";
    m_en["tut.page.prog.death"]         = "Spend Rift Shards on permanent upgrades between runs.";
    m_en["tut.page.prog.upgrades"]      = "Health, damage, dash charges, abilities — all upgradeable.";
    m_en["tut.page.prog.classes"]       = "4 Classes: Voidwalker, Berserker, Phantom, Technomancer";
    m_en["tut.page.prog.relics"]        = "Find Relics during runs for powerful passive bonuses.";
    m_en["tut.page.prog.floors"]        = "30 floors across 5 zones, each with unique hazards and enemies.";
    m_en["tut.page.prog.bosses"]        = "Defeat zone bosses to progress. Each has unique attack patterns.";

    m_en["tut.page.ready.title"]        = "Ready to Rift!";
    m_en["tut.page.ready.1"]            = "You know what you need. Time to walk the rift.";
    m_en["tut.page.ready.2"]            = "Choose your class, pick your difficulty, and dive in.";
    m_en["tut.page.ready.3"]            = "In-game hints will guide you through the first floor.";
    m_en["tut.page.ready.tip1"]         = "Tip: Explore both dimensions — some paths only exist in one!";
    m_en["tut.page.ready.tip2"]         = "Tip: Parrying is the fastest way to deal massive damage.";
    m_en["tut.page.ready.tip3"]         = "Tip: Check the Bestiary to learn enemy weaknesses.";
    m_en["tut.page.ready.start"]        = ">>> Press ENTER to choose your class <<<";


    // Death causes (run history, game over)
    m_en["death.hp"]               = "HP Depleted";
    m_en["death.entropy"]          = "Entropy Overload";
    m_en["death.rift"]             = "Rift Collapse";
    m_en["death.time"]             = "Time Expired";
    m_en["death.victory"]          = "Victory!";
    m_en["death.unknown"]          = "Unknown";

    // Upgrades
    m_en["upgrades.title"]         = "RIFTSUIT UPGRADES";
    m_en["upgrades.level"]         = "Lv %d/%d";
    m_en["upgrades.cost"]          = "Cost: %d";
    m_en["upgrades.maxed"]         = "MAXED";
    m_en["upgrades.nav_hint"]      = "W/S: Navigate    ENTER: Purchase    ESC: Back";

    // Splash
    m_en["splash.subtitle"]        = "A Dimension-Shifting Roguelike";
    m_en["splash.press_any"]       = "Press any key";

    // Menu subtitle
    m_en["menu.subtitle"]          = "Dimensional Mechanic";

    // Relic choice
    m_en["relic.choose"]           = "Choose a Relic";

    // Run History
    m_en["history.title"]          = "RUN HISTORY";
    m_en["history.stats"]          = "Total Runs: %d    Enemies Killed: %d    Best Floor: %d    Rifts: %d";
    m_en["history.avg_kills"]      = "Avg Kills/Run: %.1f";
    m_en["history.class"]          = "Class";
    m_en["history.floor"]          = "Floor";
    m_en["history.kills"]          = "Kills";
    m_en["history.rifts"]          = "Rifts";
    m_en["history.combo"]          = "Combo";
    m_en["history.time"]           = "Time";
    m_en["history.shards"]         = "Shards";
    m_en["history.diff"]           = "Diff";
    m_en["history.result"]         = "Result";
    m_en["history.empty"]          = "No runs recorded yet. Play some runs!";
    m_en["history.scroll"]         = "[W/S to scroll - %d/%d]";
    m_en["history.back"]           = "[ESC] Back";

    // Shop
    m_en["shop.title"]             = "- RIFT MARKET -";
    m_en["shop.skip"]              = "SKIP >>";
    m_en["shop.tutorial"]          = "Spend Rift Shards to upgrade your abilities for the next level!";
    m_en["shop.nav_hint"]          = "A/D or Arrows to Select  |  Enter to Buy  |  ESC to Skip";
    m_en["shop.common"]            = "COMMON";
    m_en["shop.rare"]              = "RARE";
    m_en["shop.legendary"]         = "LEGENDARY";
    m_en["shop.cost"]              = "%d Shards";
    m_en["shop.buy"]               = "[ENTER] Buy";
    m_en["shop.no_shards"]         = "Not Enough Shards";

    // Zone names & taglines (shown during zone transitions)
    m_en["zone.1.name"]            = "FRACTURED THRESHOLD";
    m_en["zone.1.tagline"]         = "Where reality first began to crack";
    m_en["zone.2.name"]            = "SHIFTING DEPTHS";
    m_en["zone.2.tagline"]         = "The dimensions bleed into each other";
    m_en["zone.3.name"]            = "RESONANT CORE";
    m_en["zone.3.tagline"]         = "Every step echoes across realities";
    m_en["zone.4.name"]            = "ENTROPY CASCADE";
    m_en["zone.4.tagline"]         = "Order dissolves. Chaos reigns.";
    m_en["zone.5.name"]            = "THE SOVEREIGN'S DOMAIN";
    m_en["zone.5.tagline"]         = "The heart of the Rift awaits";

    // Boss names (HUD + intro cards)
    m_en["boss.rift_guardian"]     = "RIFT GUARDIAN";
    m_en["boss.void_wyrm"]        = "VOID WYRM";
    m_en["boss.dim_architect"]     = "DIMENSIONAL ARCHITECT";
    m_en["boss.temporal_weaver"]   = "TEMPORAL WEAVER";
    m_en["boss.void_sovereign"]    = "VOID SOVEREIGN";
    m_en["boss.entropy_incarnate"] = "ENTROPY INCARNATE";

    // Boss intro subtitles
    m_en["boss.sub.rift_guardian"]     = "The rift remembers your face";
    m_en["boss.sub.rift_guardian_alt"] = "The dimension's immune response";
    m_en["boss.sub.void_wyrm"]        = "It slithers between dimensions";
    m_en["boss.sub.dim_architect"]     = "It rewrites reality itself";
    m_en["boss.sub.temporal_weaver"]   = "Past and future converge";
    m_en["boss.sub.entropy_incarnate"] = "Chaos given form and purpose";
    m_en["boss.sub.entropy_alt"]       = "The suit hungers for decay";
    m_en["boss.sub.void_sovereign"]    = "The last chain binding the Rift";

    // Run intro narrative
    m_en["intro.line1"]            = "The Rift opens...";
    m_en["intro.line1_ng"]         = "The Rift remembers you...";
    m_en["intro.line2"]            = "Your suit flickers. Entropy rising.";
    m_en["intro.line2_ng"]         = "Ascension Tier %d. The void grows stronger.";

    // Ending narratives (healer, 17 lines)
    m_en["ending.h.0"]  = "You stepped into the Rift seeking answers.";
    m_en["ending.h.1"]  = "Instead, you found only more questions.";
    m_en["ending.h.2"]  = "Each dimension was a fragment of a broken world,";
    m_en["ending.h.3"]  = "each enemy a shadow of what once was.";
    m_en["ending.h.4"]  = "The Void Sovereign was the last chain";
    m_en["ending.h.5"]  = "binding the Rift to this reality.";
    m_en["ending.h.6"]  = "With its defeat, the wound begins to heal.";
    m_en["ending.h.7"]  = "The dimensions slowly merge back together.";
    m_en["ending.h.8"]  = "But you know the truth now:";
    m_en["ending.h.9"]  = "The Rift was never the enemy.";
    m_en["ending.h.10"] = "It was a mirror.";
    m_en["ending.h.11"] = "And mirrors can never truly be shattered.";
    m_en["ending.h.12"] = "Only understood.";

    // Ending narratives (destroyer, 16 lines)
    m_en["ending.d.0"]  = "You came to the Rift with purpose.";
    m_en["ending.d.1"]  = "Not to understand. Not to heal.";
    m_en["ending.d.2"]  = "But to destroy.";
    m_en["ending.d.3"]  = "Every enemy fell before you,";
    m_en["ending.d.4"]  = "every dimension trembled at your approach.";
    m_en["ending.d.5"]  = "The Void Sovereign didn't stand a chance.";
    m_en["ending.d.6"]  = "You didn't just seal the Rift --";
    m_en["ending.d.7"]  = "you shattered it.";
    m_en["ending.d.8"]  = "The dimensions merge violently now.";
    m_en["ending.d.9"]  = "Not healing. Collapsing.";
    m_en["ending.d.10"] = "Perhaps destruction was always the answer.";
    m_en["ending.d.11"] = "Or perhaps the Rift needed a gentler hand.";
    m_en["ending.d.12"] = "You'll never know.";

    // Ending narratives (speedrunner, 12 lines)
    m_en["ending.s.0"]  = "Time bends around those who move through the Rift.";
    m_en["ending.s.1"]  = "You crossed thirty floors in mere minutes.";
    m_en["ending.s.2"]  = "The dimensions barely had time to notice you.";
    m_en["ending.s.3"]  = "The Void Sovereign barely had time to fight.";
    m_en["ending.s.4"]  = "You were a ghost.";
    m_en["ending.s.5"]  = "A ripple in the fabric of reality.";
    m_en["ending.s.6"]  = "The Rift sealed behind you";
    m_en["ending.s.7"]  = "before it even knew you were there.";
    m_en["ending.s.8"]  = "Speed is its own kind of mastery.";
    m_en["ending.s.9"]  = "But what did you miss along the way?";

    // Ending State
    m_en["ending.sealed"]          = "The Rift is sealed.";
    m_en["ending.shattered"]       = "The Rift is shattered.";
    m_en["ending.barely"]          = "The Rift barely noticed.";
    m_en["ending.skip"]            = "SPACE to skip";
    m_en["ending.continue"]        = "Press SPACE to continue";
    m_en["ending.run_complete"]    = "Run Complete";
    m_en["ending.run_destroyer"]   = "Run Complete - The Destroyer";
    m_en["ending.run_speedrunner"] = "Run Complete - The Speedrunner";
    m_en["ending.final_score"]     = "Final Score: %d";
    m_en["ending.enemies"]         = "Enemies Defeated: %d";
    m_en["ending.difficulty"]      = "Max Difficulty: %d";
    m_en["ending.relics"]          = "Relics Found: %d";
    m_en["ending.time"]            = "Time: %d:%02d";
    m_en["ending.thank_you"]       = "Thank You for Playing";
    m_en["ending.return"]          = "The Rift awaits your return... Ascend higher.";
    m_en["ending.back"]            = "Press any key to return to menu";

    // Credits
    m_en["credits.back"]           = "ESC: Back to Menu";
    m_en["credits.thanks"]         = "Thank you for playing.";
    m_en["credits.return"]         = "The Rift awaits your return...";
    m_en["credits.subtitle"]       = "A Roguelike Dimension-Shifting Platformer";
    m_en["credits.section.design"] = "--- Design & Programming ---";
    m_en["credits.design_line"]    = "Built with Claude Code + Human Collaboration";
    m_en["credits.section.engine"] = "--- Engine ---";
    m_en["credits.section.arch"]   = "--- Architecture ---";
    m_en["credits.arch.ecs"]       = "Entity Component System (ECS)";
    m_en["credits.arch.procgen"]   = "Procedural Level Generation";
    m_en["credits.arch.dualdim"]   = "Dual-Dimension World System";
    m_en["credits.section.gameplay"]= "--- Gameplay Systems ---";
    m_en["credits.gameplay.classes"]= "4 Playable Classes";
    m_en["credits.gameplay.weapons"]= "12 Weapons with Mastery Tiers";
    m_en["credits.gameplay.relics"] = "38 Relics with 22 Synergy Combos";
    m_en["credits.gameplay.bosses"] = "6 Bosses with Multi-Phase AI";
    m_en["credits.gameplay.enemies"]= "17 Enemy Types + 12 Elite Modifiers";
    m_en["credits.gameplay.entropy"]= "Suit Entropy Mechanic";
    m_en["credits.gameplay.ngplus"] = "New Game+ (10 Ascension Tiers)";
    m_en["credits.section.combat"] = "--- Combat ---";
    m_en["credits.combat.melee"]   = "Melee Combo System with Parry & Counter";
    m_en["credits.combat.charged"] = "Charged Attacks & Dash Combat";
    m_en["credits.combat.abilities"]= "3 Class Abilities per Class";
    m_en["credits.combat.elements"]= "Elemental Variants (Fire, Ice, Electric)";
    m_en["credits.section.world"]  = "--- World ---";
    m_en["credits.world.floors"]   = "30 Floors across 5 Themed Zones";
    m_en["credits.world.puzzles"]  = "Rift Puzzles & Secret Rooms";
    m_en["credits.world.npcs"]     = "NPCs with Questlines";
    m_en["credits.world.lore"]     = "20 Discoverable Lore Fragments";
    m_en["credits.world.events"]   = "Dynamic Level Events";
    m_en["credits.section.progression"]= "--- Progression ---";
    m_en["credits.prog.achievements"]= "42 Achievements";
    m_en["credits.prog.upgrades"]  = "Persistent Upgrade System";
    m_en["credits.prog.daily"]     = "Daily Challenge Runs";
    m_en["credits.prog.challenges"]= "5 Challenge Modes + 6 Mutators";
    m_en["credits.prog.bestiary"]  = "Bestiary with Kill Tracking";
    m_en["credits.section.audio"]  = "--- Audio ---";
    m_en["credits.audio.sfx"]      = "Procedural Sound Generation";
    m_en["credits.audio.music"]    = "Dynamic Music System";
    m_en["credits.section.visual"] = "--- Visual ---";
    m_en["credits.visual.render"]  = "Procedural Rendering with Sprite Support";
    m_en["credits.visual.parallax"]= "Parallax Background (6 Layers)";
    m_en["credits.visual.particles"]= "Particle Effects & Screen Shake";
    m_en["credits.visual.indicators"]= "Damage Indicators & Kill Feed";
    m_en["credits.section.testing"]= "--- Testing ---";
    m_en["credits.testing.bot"]    = "Automated Playtest Bot";
    m_en["credits.testing.visual"] = "Visual Regression Testing";
    m_en["credits.testing.balance"]= "Balance Snapshot System";
    m_en["credits.section.thanks"] = "--- Special Thanks ---";
    m_en["credits.thanks.void"]    = "The dimensional void, for being there";
    m_en["credits.thanks.coffee"]  = "Coffee, for making this possible";

    // Gameplay tips (shown during level transitions)
    m_en["tip.0"]  = "TIP: Dimension B has tougher enemies but better shard rewards.";
    m_en["tip.1"]  = "TIP: Parrying at the right moment triggers a powerful counter-attack.";
    m_en["tip.2"]  = "TIP: Wall-slide by holding toward a wall while airborne.";
    m_en["tip.3"]  = "TIP: Repairing rifts in Dimension B grants bonus shards and reduces entropy.";
    m_en["tip.4"]  = "TIP: Relics can combine into powerful synergies - check the pause menu.";
    m_en["tip.5"]  = "TIP: Crates sometimes drop health orbs and shards.";
    m_en["tip.6"]  = "TIP: Each class has a unique ability - use it often, it recharges quickly.";
    m_en["tip.7"]  = "TIP: Entropy increases over time. Repair rifts and switch dimensions to manage it.";
    m_en["tip.8"]  = "TIP: Elite enemies glow and have special modifiers. They drop better rewards.";
    m_en["tip.9"]  = "TIP: The minimap (M key) shows rift locations and the exit.";
    m_en["tip.10"] = "TIP: Dashing through enemies grants brief invincibility frames.";
    m_en["tip.11"] = "TIP: Bosses have 3 phases - their attacks change at 66% and 33% HP.";
    m_en["tip.12"] = "TIP: Weapon mastery increases with kills. Higher mastery unlocks bonuses.";
    m_en["tip.13"] = "TIP: Check the bestiary to learn enemy weaknesses and attack patterns.";
    m_en["tip.14"] = "TIP: Some relics are cursed - powerful effects with dangerous drawbacks.";
    m_en["tip.15"] = "TIP: Charged attacks deal massive damage. Hold the attack button to charge.";
    m_en["tip.16"] = "TIP: New Game+ adds new modifiers but rewards you with more shards.";
    m_en["tip.17"] = "TIP: Different weapon types are effective against different enemies.";
    m_en["tip.18"] = "TIP: Suit entropy affects your abilities. Keep it low for best performance.";
    m_en["tip.19"] = "TIP: Visit NPCs for quests, upgrades, and valuable lore.";
    m_en["tip.20"] = "TIP: Your persistent upgrades carry over between runs.";
    m_en["tip.21"] = "TIP: Jump on enemies from above for a stomp attack.";
    m_en["tip.22"] = "TIP: The Technomancer's turret and traps can control the battlefield.";
    m_en["tip.23"] = "TIP: Daily Runs use a shared seed - compete for the highest score.";
    m_en["tip.24"] = "TIP: Combo kills without taking damage build your kill streak multiplier.";
    m_en["tip.25"] = "TIP: The Berserker gains +30% damage and attack speed below 40% HP.";
    m_en["tip.26"] = "TIP: Secret rooms hide behind breakable walls. Look for cracks!";
    m_en["tip.27"] = "TIP: Parry timing is key — press attack just before an enemy hits you.";
    m_en["tip.28"] = "TIP: The shop appears between floors. Save shards for powerful upgrades!";
    m_en["tip.29"] = "TIP: Dimension B entropy rises faster but rifts grant bonus shards there.";
    m_en["tip.30"] = "TIP: The Phantom class can phase through enemies while dashing.";
    m_en["tip.31"] = "TIP: Watch the minimap — red dots are enemies, blue are rifts.";
    m_en["tip.32"] = "TIP: Elite enemies have colored auras showing their modifier type.";
    m_en["tip.33"] = "TIP: The Voidwalker heals 3 HP every dimension switch.";
    m_en["tip.34"] = "TIP: Challenge modes unlock after completing your first run.";
    m_en["tip.35"] = "TIP: Relic synergies activate when you carry matching relics — check the pause menu!";
    m_en["tip.36"] = "TIP: Right-click anywhere in menus to go back quickly.";
    m_en["tip.37"] = "TIP: Boss attacks change when they enrage below 33% HP — stay alert!";
    m_en["tip.38"] = "TIP: Health orbs from crates heal more when your HP is critically low.";
    m_en["tip.39"] = "TIP: Weapon mastery persists between runs — master your favorites!";
    m_en["tip.40"] = "TIP: NPCs offer quests, repairs, and rare items. Always check!";
    m_en["tip.41"] = "TIP: Cursed relics offer huge power boosts with dangerous drawbacks.";
    m_en["tip.42"] = "TIP: Dimension A is safer for rift repair, but Dimension B gives more shards.";
    m_en["tip.43"] = "TIP: Event chains span multiple floors — complete all stages for bonus rewards!";
    m_en["tip.44"] = "TIP: The Entropy Scythe reduces suit entropy on hit — great for long runs.";
    m_en["tip.45"] = "TIP: Hold the ranged attack button for a powerful charged shot!";
    m_en["tip.46"] = "TIP: Grappling Hook can pull enemies toward you or swing from walls.";
    m_en["tip.47"] = "TIP: Mini-bosses are stronger elites that appear mid-floor. Extra rewards!";
    m_en["tip.48"] = "TIP: Kill streaks without getting hit multiply your shard gains.";
    m_en["tip.49"] = "TIP: The Rift Shotgun fires 5 pellets — devastating at close range.";
    m_en["tip.50"] = "TIP: Visit the Bestiary to learn enemy weaknesses and effective weapons.";
    m_en["tip.51"] = "TIP: Rift repairs in Dimension B are riskier but reduce more entropy.";
    m_en["tip.52"] = "TIP: Phoenix Feather relic auto-revives you once per run — invaluable for bosses!";
    m_en["tip.53"] = "TIP: Each New Game+ tier unlocks unique modifiers AND bonus shard rewards.";
    m_en["tip.54"] = "TIP: The Void Beam pierces through all enemies in a line — great for corridors.";
    m_en["tip.55"] = "TIP: Dimensional Plating upgrade reduces all incoming damage by 10% per level.";
    m_en["tip.56"] = "TIP: The Fortune Teller NPC can reveal secret rooms on the map.";
    m_en["tip.57"] = "TIP: Stomping on enemies from above deals bonus damage based on fall height.";
    m_en["tip.58"] = "TIP: Elemental enemies take extra damage from their opposing element.";
    m_en["tip.59"] = "TIP: The Blacksmith NPC permanently enhances your weapons at higher story stages.";
    m_en["tip.60"] = "TIP: Entropy Anchor relic cuts entropy gain by 40% during boss fights.";
    m_en["tip.61"] = "TIP: Wall sliding lets you reach hidden areas above normal jump height.";
    m_en["tip.62"] = "TIP: Soul Siphon relic heals 3 HP per kill — essential for survival runs.";
    m_en["tip.63"] = "TIP: The Combo Amplifier upgrade increases damage dealt during combos.";
    m_en["tip.64"] = "TIP: Daily Runs use a unique seed — compete for the highest leaderboard score!";
    m_en["tip.65"] = "TIP: Time Dilator relic cuts ability cooldowns by 30% — combos never stop!";
    m_en["tip.66"] = "TIP: Reflector enemies cycle their shield every 4 seconds. Time your attacks!";
    m_en["tip.67"] = "TIP: The Rift Crossbow pierces multiple enemies — line up your shots!";
    m_en["tip.68"] = "TIP: Summoner enemies are fragile alone. Kill the summoner before the minions!";
    m_en["tip.69"] = "TIP: Check the pause menu for active relic synergies and current build stats.";
    m_en["tip.70"] = "TIP: Gravity Gauntlet pulls enemies toward you — great for crowd control!";
    m_en["tip.71"] = "TIP: Coyote time lets you jump briefly after walking off a ledge.";
    m_en["tip.72"] = "TIP: Attack buffering queues your next attack during cooldown for snappy combat.";
    m_en["tip.73"] = "TIP: Chain Whip has the longest melee range and pierces through enemies.";
    m_en["tip.74"] = "TIP: Press M to toggle the minimap on or off during gameplay.";
    m_en["tip.75"] = "TIP: Void Hunger relic gives +1% permanent damage per kill — scales insanely!";
    m_en["tip.76"] = "TIP: Chaos Orb relic grants random effects every 30 seconds — never boring!";
    m_en["tip.77"] = "TIP: The Phase Daggers crit every 5th hit — fast weapons mean more crits!";
    m_en["tip.78"] = "TIP: Entropy Sponge relic removes passive entropy but kills add 5% each.";
    m_en["tip.79"] = "TIP: F9 or F12 takes a screenshot at any time — share your best moments!";

    // Input action names (shown in Keybindings screen)
    m_en["action.move_left"]       = "Move Left";
    m_en["action.move_right"]      = "Move Right";
    m_en["action.look_up"]         = "Look Up";
    m_en["action.look_down"]       = "Look Down";
    m_en["action.jump"]            = "Jump";
    m_en["action.dash"]            = "Dash";
    m_en["action.melee"]           = "Melee Attack";
    m_en["action.ranged"]          = "Ranged Attack";
    m_en["action.dim_switch"]      = "Dimension Switch";
    m_en["action.interact"]        = "Interact";
    m_en["action.pause"]           = "Pause";
    m_en["action.slam"]            = "Ground Slam";
    m_en["action.shield"]          = "Rift Shield";
    m_en["action.phase"]           = "Phase Strike";

    // Buff pickups (in-game floating text)
    m_en["pickup.shield"]          = "SHIELD";
    m_en["pickup.speed"]           = "SPEED UP";
    m_en["pickup.damage"]          = "DMG UP";

    // NG+ rank titles (run summary screen)
    m_en["ngplus.rank.1"]          = "NG+1 CHALLENGER";
    m_en["ngplus.rank.2"]          = "NG+2 VETERAN";
    m_en["ngplus.rank.3"]          = "NG+3 HARDENED";
    m_en["ngplus.rank.4"]          = "NG+4 VOID TOUCHED";
    m_en["ngplus.rank.5"]          = "NG+5 RIFT SOVEREIGN";
    m_en["ngplus.rank.6"]          = "NG+6 ENTROPY WALKER";
    m_en["ngplus.rank.7"]          = "NG+7 DIMENSION LORD";
    m_en["ngplus.rank.8"]          = "NG+8 REALITY BREAKER";
    m_en["ngplus.rank.9"]          = "NG+9 VOID ASCENDANT";
    m_en["ngplus.rank.10"]         = "NG+10 CHAOS MASTER";

    // Run summary balance stats
    m_en["summary.balance_dmg"]    = "DMG %.2fx%s  |  ATK %.2fx%s";
    m_en["summary.balance_cap"]    = " (CAP)";
    m_en["summary.balance_cd"]     = "CD Floor %.0f%%  |  VoidRes %d  |  Zones %d";
    m_en["summary.balance_hunger"] = "VoidHunger %.0f%% final (%.0f%% peak)";
    m_en["summary.info"]           = "%s  |  %s / %s  |  Diff %d  |  %d:%02d";

    // Event chain / zone transition
    m_en["hud.chain_stage"]        = "CHAIN: Stage %d/%d";
    m_en["hud.zone"]               = "ZONE %d";
    m_en["hud.xp_bar"]            = "Lv.%d  %d/%d";
    m_en["hud.hp_display"]         = "HP %.0f/%.0f";
    m_en["hud.entropy_display"]    = "ENTROPY %.0f%%";
    m_en["hud.floor_boss"]         = "F%d / Zone %d  BOSS";
    m_en["hud.floor_miniboss"]     = "F%d / Zone %d  MINI-BOSS";
    m_en["hud.floor"]              = "F%d / Zone %d";
    m_en["hud.chain_reward"]       = "%s — +%d Shards";
    m_en["hud.weapon_melee"]       = "MELEE";
    m_en["hud.weapon_ranged"]      = "RANGED";

    // NPC names
    m_en["npc.scholar"]            = "Rift Scholar";
    m_en["npc.refugee"]            = "Dim. Refugee";
    m_en["npc.engineer"]           = "Lost Engineer";
    m_en["npc.echo"]               = "Echo of Self";
    m_en["npc.blacksmith"]         = "Blacksmith";
    m_en["npc.fortune"]            = "Fortune Teller";
    m_en["npc.merchant"]           = "Void Merchant";

    // NPC greetings (stage 0/1/2)
    m_en["npc.scholar.g0"]         = "Knowledge is power in the rift.";
    m_en["npc.scholar.g1"]         = "Welcome back! I've found something crucial...";
    m_en["npc.scholar.g2"]         = "You again! I've deciphered the final texts.";
    m_en["npc.refugee.g0"]         = "Let's make a deal...";
    m_en["npc.refugee.g1"]         = "You came back! I found supplies to share.";
    m_en["npc.refugee.g2"]         = "My friend! I can finally repay your kindness.";
    m_en["npc.engineer.g0"]        = "I can fix that for you.";
    m_en["npc.engineer.g1"]        = "I've been studying rift energy. Better results!";
    m_en["npc.engineer.g2"]        = "My masterwork is complete. This one's on me.";
    m_en["npc.echo.g0"]            = "Face yourself!";
    m_en["npc.echo.g1"]            = "Stronger now... Can you match me again?";
    m_en["npc.echo.g2"]            = "One final test. Prove your mastery.";
    m_en["npc.blacksmith.g0"]      = "Shards fuel my forge. Let me improve your gear.";
    m_en["npc.blacksmith.g1"]      = "Your weapons have seen battle. I can do more now.";
    m_en["npc.blacksmith.g2"]      = "For you, my finest work. Free of charge.";
    m_en["npc.fortune.g0"]         = "Cross my palm with shards, and I'll show you secrets.";
    m_en["npc.fortune.g1"]         = "The patterns grow clearer. I see more now.";
    m_en["npc.fortune.g2"]         = "The rifts have shown me everything. My final gift...";
    m_en["npc.merchant.g0"]        = "Dimensional artifacts, fresh from the void.";
    m_en["npc.merchant.g1"]        = "Ah, you return! I've acquired new inventory.";
    m_en["npc.merchant.g2"]        = "For a valued customer, my rarest stock.";

    // NPC dialog options (most visible action text)
    m_en["npc.opt.leave"]          = "[Leave]";
    m_en["npc.opt.fight"]          = "[Fight!]";
    m_en["npc.opt.not_yet"]        = "[Not yet...]";
    m_en["npc.opt.browse"]         = "[Browse wares]";
    m_en["npc.opt.read_fortune"]   = "[Read my fortune]";

    // NPC action options (Scholar)
    m_en["npc.scholar.s2.opt"]     = "[Learn the truth (+40 Shards, heal)]";
    m_en["npc.scholar.s1.opt1"]    = "[Listen (+25 Shards, -entropy)]";
    m_en["npc.scholar.s1.opt2"]    = "[Ask about the Sovereign]";
    m_en["npc.scholar.s0.opt1"]    = "[Listen to tip (+15 Shards)]";
    m_en["npc.scholar.s0.opt2"]    = "[Ask about enemies]";
    m_en["npc.scholar.quest"]      = "[Accept quest: Hunt 10 creatures]";
    // NPC action options (Refugee)
    m_en["npc.refugee.s2.opt"]     = "[Accept gift (free Relic)]";
    m_en["npc.refugee.s1.opt1"]    = "[Free healing (+30 HP)]";
    m_en["npc.refugee.s1.opt2"]    = "[Trade 15 HP for 60 Shards]";
    m_en["npc.refugee.s0.opt1"]    = "[Trade 20 HP for 50 Shards]";
    m_en["npc.refugee.s0.opt2"]    = "[Trade 30 HP for 80 Shards]";
    // NPC action options (Engineer)
    m_en["npc.engineer.s2.opt"]    = "[Permanent upgrade (+25% DMG)]";
    m_en["npc.engineer.s1.opt1"]   = "[Upgrade weapon (+40% DMG, 60s)]";
    m_en["npc.engineer.s1.opt2"]   = "[Tune attacks (+20% speed, 45s)]";
    m_en["npc.engineer.s0.opt"]    = "[Upgrade weapon (+30% DMG, 45s)]";
    m_en["npc.engineer.quest"]     = "[Accept quest: Repair 3 rifts]";
    // NPC action options (Blacksmith)
    m_en["npc.smith.s2.opt"]       = "[Accept masterwork (free +30% both)]";
    m_en["npc.smith.s1.opt1"]      = "[Sharpen melee +25% DMG (35 shards)]";
    m_en["npc.smith.s1.opt2"]      = "[Reinforce ranged +25% DMG (35 shards)]";
    m_en["npc.smith.s1.opt3"]      = "[Hone speed +15% ATK SPD (45 shards)]";
    m_en["npc.smith.s0.opt1"]      = "[Sharpen melee +20% DMG (40 shards)]";
    m_en["npc.smith.s0.opt2"]      = "[Reinforce ranged +20% DMG (40 shards)]";
    // NPC action options (Fortune Teller)
    m_en["npc.fortune.s2.opt1"]    = "[Reveal all secrets (free)]";
    m_en["npc.fortune.s2.opt2"]    = "[Boss foresight (+20% DMG vs boss)]";
    m_en["npc.fortune.s1.opt1"]    = "[Reveal hidden rooms (20 shards)]";
    m_en["npc.fortune.s1.opt2"]    = "[Reveal ambushes (15 shards)]";
    m_en["npc.fortune.s0.opt"]     = "[Reveal secrets (30 shards)]";
    // NPC action options (Void Merchant)
    m_en["npc.merch.s2.opt1"]      = "[Buy legendary relic (80 shards)]";
    m_en["npc.merch.s2.opt2"]      = "[Buy random relic (40 shards)]";
    m_en["npc.merch.s1.opt1"]      = "[Buy relic (60 shards)]";
    m_en["npc.merch.s1.opt2"]      = "[Buy random relic (35 shards)]";
    m_en["npc.merch.s0.opt"]       = "[Buy relic (50 shards)]";

    // NPC quest offers & progress
    m_en["npc.quest.scholar"]      = "I need data on the rift creatures. Defeat 10 enemies\nand I'll reward you with rift shards.";
    m_en["npc.quest.engineer"]     = "Help me calibrate the rift sensors. Repair 3 rifts\non this floor and I'll pay you — plus stabilize your suit.";
    m_en["npc.quest.fortune"]      = "The void hides many secrets. Find 2 secret rooms\non this floor, and I'll share a powerful vision.";
    m_en["npc.qprog.scholar"]      = "Keep hunting those creatures. I can sense more data flowing in...";
    m_en["npc.qprog.engineer"]     = "The sensors are picking up your repairs. Keep going!";
    m_en["npc.qprog.fortune"]      = "I sense you've found some hidden places... keep searching.";

    // NPC story lines (7 NPCs × 3 stages)
    m_en["npc.scholar.d0"]  = "The dimensional rifts grow unstable. Each repair\nweakens the Void Sovereign's grip on reality.";
    m_en["npc.scholar.d1"]  = "Ancient texts reveal the rifts aren't natural.\nSomeone — or something — tore the dimensions apart.";
    m_en["npc.scholar.d2"]  = "The truth: the Sovereign was once a guardian. The rifts\nare the dimensions healing themselves. You are the catalyst.";
    m_en["npc.refugee.d0"]  = "I'm trapped between dimensions. Trade me some health\nand I'll give you rift shards I've collected.";
    m_en["npc.refugee.d1"]  = "I found medical supplies between the dimensions.\nLet me patch you up — and I have a better deal too.";
    m_en["npc.refugee.d2"]  = "You've helped me survive this long. I gathered\nrift components — take this gift, no strings attached.";
    m_en["npc.engineer.d0"] = "Let me recalibrate your weapon with rift harmonics.\nShould boost damage output for a while.";
    m_en["npc.engineer.d1"] = "Rift energy amplifies weapons more than I thought!\nLonger effect, plus I can boost your attack speed.";
    m_en["npc.engineer.d2"] = "I cracked it! Rift-infused metal permanently bonds\nwith weapons. This upgrade lasts your whole run.";
    m_en["npc.echo.d0"]     = "A mirror match begins! Defeat your echo for a reward.";
    m_en["npc.echo.d1"]     = "Your echo is stronger now, adapted from last time.";
    m_en["npc.echo.d2"]     = "Your ultimate echo. Defeat it for the greatest reward.";
    m_en["npc.smith.d0"]    = "My forge burns with rift energy. I can sharpen your\nmelee or reinforce your ranged weapon — for a price.";
    m_en["npc.smith.d1"]    = "With more rift energy, I can now upgrade range\nand attack speed too. Better prices for a returning customer.";
    m_en["npc.smith.d2"]    = "I've perfected my rift-forging technique.\nAccept my masterwork — both weapons, permanently enhanced.";
    m_en["npc.fortune.d0"]  = "I sense hidden chambers nearby. For a small offering,\nI'll reveal their locations on your map.";
    m_en["npc.fortune.d1"]  = "The dimensional threads are easier to read now.\nI can show you hidden rooms and warn of ambushes ahead.";
    m_en["npc.fortune.d2"]  = "My visions have never been clearer. I can reveal every\nsecret on this floor — and grant you foresight against the boss.";
    m_en["npc.merchant.d0"] = "Each of these relics was pried from the void between\ndimensions. They carry great power — at a fair price.";
    m_en["npc.merchant.d1"] = "I've expanded my collection. These relics carry\nstronger dimensional resonance — worth every shard.";
    m_en["npc.merchant.d2"] = "My finest acquisition: a relic from the Sovereign's own\nvault. Normally priceless — but for you, a special rate.";

    // Random events
    m_en["event.merchant"]         = "Merchant";
    m_en["event.anomaly"]          = "Anomaly";
    m_en["event.rift_echo"]        = "Rift Echo";
    m_en["event.repair_station"]   = "Repair Station";
    m_en["event.gambling"]         = "Gambling Rift";
    m_en["event.shrine_power"]     = "Shrine of Power";
    m_en["event.shrine_vitality"]  = "Shrine of Vitality";
    m_en["event.shrine_speed"]     = "Shrine of Speed";
    m_en["event.shrine_entropy"]   = "Shrine of Entropy";
    m_en["event.shrine_shards"]    = "Shrine of Shards";

    // Event chains
    m_en["chain.merchant_quest"]   = "Merchant's Quest";
    m_en["chain.dim_tear"]         = "Dimensional Tear";
    m_en["chain.entropy_surge"]    = "Entropy Surge";
    m_en["chain.lost_cache"]       = "Lost Cache";
    m_en["npc.nav_hint"]           = "[W/S] Navigate  [Enter] Select  [Esc] Close";

    // Combat challenges
    m_en["cc.aerial.name"]         = "Aerial Kill";
    m_en["cc.aerial.desc"]         = "Kill an enemy while airborne";
    m_en["cc.multi.name"]          = "Triple Kill";
    m_en["cc.multi.desc"]          = "Kill 3 enemies within 4 seconds";
    m_en["cc.dash.name"]           = "Dash Slayer";
    m_en["cc.dash.desc"]           = "Kill an enemy with a dash attack";
    m_en["cc.combo.name"]          = "Combo Finisher";
    m_en["cc.combo.desc"]          = "Kill with a 3rd combo hit";
    m_en["cc.charged.name"]        = "Heavy Hitter";
    m_en["cc.charged.desc"]        = "Kill with a charged attack";
    m_en["cc.nodmg.name"]          = "Untouchable";
    m_en["cc.nodmg.desc"]          = "Clear wave without taking damage";

    // Dynamic events
    m_en["event.dim_storm"]        = "DIMENSION STORM";
    m_en["event.elite_invasion"]   = "ELITE INVASION";
    m_en["event.time_dilation"]    = "TIME DILATION";

    // Bestiary locked stat labels
    m_en["bestiary.boss_label"]    = "[ BOSS ]";
    m_en["bestiary.hp_locked"]     = "HP:  ???";
    m_en["bestiary.dmg_locked"]    = "DMG: ???";
    m_en["bestiary.spd_locked"]    = "SPD: ???";
    m_en["bestiary.elem_locked"]   = "ELEM: ???";

    // Combat floating text
    m_en["hud.parry"]              = "PARRY!";
    m_en["hud.crit"]               = "CRIT! %.0f";
    m_en["hud.level_display"]      = "Lv.%d";
    m_en["hud.challenge_complete"] = "CHALLENGE COMPLETE! +%d Shards";
    m_en["hud.rifts_counter"]      = "Rifts: %d / %d  [A:%d B:%d]";

    // Bestiary stat labels
    m_en["bestiary.stat_hp"]       = "HP:  %.0f";
    m_en["bestiary.stat_dmg"]      = "DMG: %.0f";
    m_en["bestiary.stat_spd"]      = "SPD: %.0f";
    m_en["bestiary.stat_spd_na"]   = "SPD: ---";
    m_en["bestiary.stat_elem"]     = "ELEM: %s";

    // ===== GERMAN =====

    // Main Menu
    m_de["menu.new_run"]           = "Neuer Lauf";
    m_de["menu.daily_run"]         = "Tageslauf";
    m_de["menu.daily_leaderboard"] = "Tagesrangliste";
    m_de["menu.challenges"]        = "Herausforderungen";
    m_de["menu.upgrades"]          = "Verbesserungen";
    m_de["menu.bestiary"]          = "Bestiarium";
    m_de["menu.achievements"]      = "Erfolge";
    m_de["menu.lore"]              = "Wissen";
    m_de["menu.run_history"]       = "Laufchronik";
    m_de["menu.credits"]           = "Mitwirkende";
    m_de["menu.options"]           = "Optionen";
    m_de["menu.quit"]              = "Beenden";
    m_de["menu.rift_shards"]       = "Risssplitter: %d";
    m_de["menu.ascension"]         = "Aufstieg %d";
    m_de["menu.rift_cores"]        = "Risskerne: %d";
    m_de["menu.nav_hint"]          = "W/S Navigieren  |  ENTER Auswaehlen  |  ESC Beenden";

    // Pause Menu
    m_de["pause.title"]            = "P A U S I E R T";
    m_de["pause.resume"]           = "Fortsetzen";
    m_de["pause.restart"]          = "Lauf neustarten";
    m_de["pause.restart_confirm"]  = "Neustarten? Fortschritt geht verloren!";
    m_de["pause.daily_leaderboard"]= "Tagesrangliste";
    m_de["pause.options"]          = "Optionen";
    m_de["pause.abandon"]          = "Lauf abbrechen";
    m_de["pause.abandon_confirm"]  = "Wirklich abbrechen?";
    m_de["pause.quit_menu"]        = "Zurueck zum Menue";
    m_de["pause.nav_hint"]         = "ESC Fortsetzen  |  W/S Navigieren  |  ENTER Auswaehlen";

    // Pause Stats
    m_de["pause.run_stats"]        = "LAUF-STATISTIK";
    m_de["pause.floor"]            = "Etage: %d / Zone %d";
    m_de["pause.level"]            = "Level: %d  (%d/%d XP)";
    m_de["pause.difficulty"]       = "Schwierigkeit: %s";
    m_de["pause.diff_easy"]        = "Leicht";
    m_de["pause.diff_normal"]      = "Normal";
    m_de["pause.diff_hard"]        = "Schwer";
    m_de["pause.boss_floor"]       = "** BOSS-ETAGE **";
    m_de["pause.time"]             = "Zeit: %d:%02d";
    m_de["pause.kills"]            = "Kills: %d";
    m_de["pause.rifts_repaired"]   = "Risse repariert: %d";
    m_de["pause.shards"]           = "Splitter: %d";
    m_de["pause.best_combo"]       = "Beste Kombo: %d";
    m_de["pause.equipment"]        = "AUSRUESTUNG";
    m_de["pause.class"]            = "Klasse: %s";
    m_de["pause.melee"]            = "Nahkampf: %s";
    m_de["pause.ranged"]           = "Fernkampf: %s";
    m_de["pause.hp"]               = "HP: %.0f / %.0f";
    m_de["pause.relics"]           = "RELIKTE";

    // Options
    m_de["options.title"]          = "O P T I O N E N";
    m_de["options.master_volume"]  = "Gesamtlautstaerke";
    m_de["options.sfx_volume"]     = "Effektlautstaerke";
    m_de["options.music_volume"]   = "Musiklautstaerke";
    m_de["options.mute"]           = "Stummschalten";
    m_de["options.fullscreen"]     = "Vollbild";
    m_de["options.resolution"]     = "Aufloesung";
    m_de["options.screen_shake"]   = "Bildschirmwackeln";
    m_de["options.hud_opacity"]    = "HUD-Transparenz";
    m_de["options.rumble"]         = "Controller-Vibration";
    m_de["options.colorblind"]     = "Farbenblind-Modus";
    m_de["options.hud_scale"]      = "HUD-Groesse";
    m_de["options.controls"]       = "Steuerung";
    m_de["options.reset"]          = "Standardwerte";
    m_de["options.back"]           = "Zurueck";
    m_de["options.language"]       = "Sprache";
    m_de["options.lang_en"]        = "English";
    m_de["options.lang_de"]        = "Deutsch";
    m_de["options.on"]             = "AN";
    m_de["options.off"]            = "AUS";
    m_de["options.crt_effect"]     = "CRT-Effekt";
    m_de["options.nav_hint"]       = "A/D Anpassen  |  W/S Navigieren  |  ENTER Umschalten  |  ESC Zurueck";

    // Game Over
    m_de["gameover.title"]         = "A N Z U G V E R S A G E N";
    m_de["gameover.death_hp"]      = "Huellenintegritaet verloren - HP aufgebraucht";
    m_de["gameover.death_entropy"] = "Anzug-Entropie hat kritisches Niveau erreicht";
    m_de["gameover.death_rift"]    = "Dimensionsriss kollabiert";
    m_de["gameover.death_time"]    = "Zeit abgelaufen - Speedrun gescheitert";
    m_de["gameover.floor_stats"]   = "Etage %d  |  %d Kills  |  %d:%02d";
    m_de["gameover.critical"]      = "KRITISCH";
    m_de["gameover.continue"]      = "Beliebige Taste zum Fortfahren";
    m_de["gameover.suit_failure"]  = "ANZUGVERSAGEN";

    // Run Summary
    m_de["summary.title"]          = "L A U F  B E E N D E T";
    m_de["summary.rooms_cleared"]  = "Raeume geschafft";
    m_de["summary.enemies_defeated"]= "Gegner besiegt";
    m_de["summary.rifts_repaired"] = "Risse repariert";
    m_de["summary.shards_earned"]  = "Splitter erhalten";
    m_de["summary.relics_collected"]= "Relikte gesammelt";
    m_de["summary.best_combo"]     = "Beste Kombo";
    m_de["summary.dim_switches"]   = "Dim.-Wechsel";
    m_de["summary.damage_taken"]   = "Schaden erhalten";
    m_de["summary.aerial_kills"]   = "Luft-Kills";
    m_de["summary.dash_kills"]     = "Dash-Kills";
    m_de["summary.total_runs"]     = "Laeufe gesamt";
    m_de["summary.grade"]          = "Note: %s";
    m_de["summary.cause"]          = "Ursache: %s";
    m_de["summary.balance"]        = "Balance-Zusammenfassung";
    m_de["summary.new_record"]     = "NEUER REKORD!";
    m_de["summary.daily_score"]    = "TAGESPUNKTZAHL";
    m_de["summary.new_best"]       = "NEUE BESTLEISTUNG!";
    m_de["summary.rank_today"]     = "Rang: #%d heute";
    m_de["summary.leaderboard_hint"]= "L fuer Rangliste";
    m_de["summary.continue"]       = "ENTER zum Fortfahren";

    // HUD
    m_de["hud.collapse"]           = "KOLLAPS: %d.%d";
    m_de["hud.exit"]               = "AUSGANG";
    m_de["hud.level_up"]           = "LEVEL UP!";
    m_de["hud.lore_discovered"]    = "WISSEN ENTDECKT";
    m_de["hud.chain_complete"]     = "KETTE ABGESCHLOSSEN!";
    m_de["hud.rift_stabilized"]    = "RISS STABILISIERT - Sprung zur naechsten Dimension...";

    // Menu info panels
    m_de["menu.daily_date"]        = "Datum: %s";
    m_de["menu.daily_seed"]        = "Seed: %d";
    m_de["menu.daily_mutator"]     = "Mutator: %s";
    m_de["menu.daily_best"]        = "Bestleistung: %d";
    m_de["menu.daily_no_best"]     = "Bestleistung: ---";
    m_de["menu.career_stats"]      = "KARRIERE-STATISTIK";
    m_de["menu.stats_runs"]        = "Laeufe: %d  |  Beste Etage: %d";
    m_de["menu.stats_kills"]       = "Kills: %d  |  Risse: %d";
    m_de["menu.stats_shards"]      = "Splitter: %d";
    m_de["menu.stats_playtime"]    = "Spielzeit: %dh %dm";
    m_de["menu.stats_victories"]   = "Siege: %d";

    // HUD notifications
    m_de["hud.achievement"]        = "Erfolg: %s";
    m_de["hud.reward"]             = "Belohnung: %s";
    m_de["hud.quest"]              = "Quest: %s (%d/%d)";
    m_de["hud.quest_complete"]     = "Quest abgeschlossen! +%d Splitter";
    m_de["hud.repair_rift"]        = "F druecken um Riss zu reparieren";
    m_de["hud.score"]              = "Punkte: %d";
    m_de["hud.defeat_boss"]        = "Besiege den Boss um den Ausgang freizuschalten!";
    m_de["hud.repair_rifts"]       = "Repariere alle Risse um den Ausgang freizuschalten!";
    m_de["hud.rift_dimb"]          = "Dieser Riss stabilisiert in DIM-B. +%d%% Splitter, -%.0f Entropie bei Reparatur, +%.2f Entropie/s.";
    m_de["hud.rift_dima"]          = "Dieser Riss stabilisiert in DIM-A. Sicherere Route, kein DIM-B-Druck.";
    m_de["hud.wave_cleared"]       = "WELLE GESCHAFFT";
    m_de["hud.shift_dimb"]         = "Wechsle zu DIM-B: +%d%% Splitter, -%.0f Entropie bei Reparatur";
    m_de["hud.shift_dima"]         = "Wechsle zu DIM-A um diesen Riss zu stabilisieren";
    m_de["hud.repair_dimb"]        = "F: Instabilen DIM-B-Riss reparieren (+%d%% Splitter, -%.0f Entropie)";
    m_de["hud.repair_dima"]        = "F: Stabilen DIM-A-Riss reparieren";
    m_de["relic.cursed"]           = "VERFLUCHT";
    m_de["menu.version"]           = "v0.1  -  Erstellt mit SDL2";
    m_de["pause.ngplus"]           = "New Game+ %d";
    m_de["pause.mastery"]          = "  [%s] %d Kills";
    m_de["mastery.familiar"]       = "Vertraut";
    m_de["mastery.proficient"]     = "Geuebt";
    m_de["mastery.mastered"]       = "Gemeistert";

    // Class Select
    m_de["class.title"]            = "W A E H L E  D E I N E  K L A S S E";
    m_de["class.locked"]           = "GESPERRT";
    m_de["class.nav_hint"]         = "A/D Wechseln  |  ENTER Auswaehlen  |  ESC Zurueck";
    m_de["class.hp"]               = "HP: %.0f";
    m_de["class.speed"]            = "Tempo: %.0f";

    // Class descriptions & passives
    m_de["class.voidwalker.desc"]      = "Meister der Dimensionsrisse. Dimensionswechsel heilt und laedt schneller.";
    m_de["class.voidwalker.passive"]   = "Dimensionsaffinitaet";
    m_de["class.voidwalker.pdesc"]     = "Dim.-Wechsel heilt 3 HP, -30% Wechsel-CD, +30% SCH fuer 2.5s";
    m_de["class.voidwalker.ability"]   = "Phasenschlag: +50% Reichweite";
    m_de["class.berserker.desc"]       = "Rasender Krieger. Wird staerker bei niedrigem HP. Bodenstampfer vernichtet.";
    m_de["class.berserker.passive"]    = "Blutrausch";
    m_de["class.berserker.pdesc"]      = "Unter 40% HP: +30% SCH, +20% Angriffsgeschwindigkeit";
    m_de["class.berserker.ability"]    = "Bodenstampfer: +40% Radius, +0.4s Betaeubung";
    m_de["class.phantom.desc"]         = "Flinker Schatten. Gleitet durch Gegner beim Dash. Rissschild verstaerkt.";
    m_de["class.phantom.passive"]      = "Schattenschritt";
    m_de["class.phantom.pdesc"]        = "Dash 50% laenger, durch Gegner hindurch, 0.5s unsichtbar danach";
    m_de["class.phantom.ability"]      = "Rissschild: +1 max Treffer, Tempo-Boost bei Absorption";
    m_de["class.technomancer.desc"]    = "Tech-Ingenieur. Stellt Geschuetze und Schockfallen auf. Konstrukte +20% SCH.";
    m_de["class.technomancer.passive"] = "Konstruktmeisterschaft";
    m_de["class.technomancer.pdesc"]   = "Geschuetze/Fallen +20% SCH, 50% laenger, +10% Fernkampf-SCH";
    m_de["class.technomancer.ability"] = "Geschuetz (Q) / Schockfalle (E)";

    // Class unlock requirements
    m_de["class.unlock.voidwalker"]    = "Immer verfuegbar";
    m_de["class.unlock.berserker"]     = "Toete insgesamt 50 Gegner in allen Laeufen";
    m_de["class.unlock.phantom"]       = "Erreiche Etage 3 in einem Lauf";
    m_de["class.unlock.technomancer"]  = "Repariere insgesamt 30 Risse in allen Laeufen";

    // Upgrade names & descriptions
    // Shop buffs
    m_de["buff.0.name"]  = "HP-Schub";           m_de["buff.0.desc"]  = "+30 Max HP";
    m_de["buff.1.name"]  = "Zeitliche Hast";     m_de["buff.1.desc"]  = "-25% Faehigkeits-Abklingzeit";
    m_de["buff.2.name"]  = "Entropiedaempfer";   m_de["buff.2.desc"]  = "-30% Entropiezunahme";
    m_de["buff.3.name"]  = "Kritische Kante";    m_de["buff.3.desc"]  = "+20% Krit. Trefferchance";
    m_de["buff.4.name"]  = "Splitterverdoppler"; m_de["buff.4.desc"]  = "2x Splitterzunahme";
    m_de["buff.5.name"]  = "Toetungsrausch";     m_de["buff.5.desc"]  = "Kill = Dash-CD Reset";
    m_de["buff.6.name"]  = "Glutklinge";         m_de["buff.6.desc"]  = "Angriffe verbrennen Gegner";
    m_de["buff.7.name"]  = "Frostkante";         m_de["buff.7.desc"]  = "Angriffe verlangsamen Gegner";
    m_de["buff.8.name"]  = "Sturmschlaeger";     m_de["buff.8.desc"]  = "Angriffe erzeugen Kettenblitze";
    m_de["buff.9.name"]  = "Vampirklinge";       m_de["buff.9.desc"]  = "5% Lebensraub pro Treffer";
    m_de["buff.10.name"] = "Seelenanker";        m_de["buff.10.desc"] = "Einmal bei 50% HP wiederbeleben";
    m_de["buff.11.name"] = "Phantomschritt";     m_de["buff.11.desc"] = "Unverwundbar nach Dim.-Wechsel";

    // Persistent upgrades
    m_de["upgrade.0.name"]  = "Verstaerkter Anzug";   m_de["upgrade.0.desc"]  = "+20 Max HP";
    m_de["upgrade.1.name"]  = "Quantenstiefel";       m_de["upgrade.1.desc"]  = "+10% Tempo";
    m_de["upgrade.2.name"]  = "Phasenantrieb";        m_de["upgrade.2.desc"]  = "-15% Dash-Abklingzeit";
    m_de["upgrade.3.name"]  = "Rissklinge";           m_de["upgrade.3.desc"]  = "+15% Nahkampfschaden";
    m_de["upgrade.4.name"]  = "Leerprojektor";        m_de["upgrade.4.desc"]  = "+15% Fernkampfschaden";
    m_de["upgrade.5.name"]  = "Entropieschild";       m_de["upgrade.5.desc"]  = "-20% Entropiezunahme";
    m_de["upgrade.6.name"]  = "Auto-Stabilisator";    m_de["upgrade.6.desc"]  = "+Passiver Entropieabbau";
    m_de["upgrade.7.name"]  = "Gravitationsmodulator"; m_de["upgrade.7.desc"] = "+10% Sprunghoehe";
    m_de["upgrade.8.name"]  = "Luftdash-Modul";       m_de["upgrade.8.desc"]  = "+1 Extrasprung";
    m_de["upgrade.9.name"]  = "Risskondensator";      m_de["upgrade.9.desc"]  = "-20% Wechsel-Abklingzeit";
    m_de["upgrade.10.name"] = "Dimensionspanzerung";  m_de["upgrade.10.desc"] = "+10% Schadensreduktion";
    m_de["upgrade.11.name"] = "Komboverstaerker";     m_de["upgrade.11.desc"] = "+Kombo-Schadensbonus";
    m_de["upgrade.12.name"] = "Griffverstaerker";     m_de["upgrade.12.desc"] = "Bessere Wandrutsch-Kontrolle";
    m_de["upgrade.13.name"] = "Rissresonanz";         m_de["upgrade.13.desc"] = "+10% Kritische Trefferchance";
    m_de["upgrade.14.name"] = "Splittermagnet";       m_de["upgrade.14.desc"] = "+Aufsammelreichweite";
    m_de["upgrade.15.name"] = "Zeitfluss";            m_de["upgrade.15.desc"] = "-15% Faehigkeits-Abklingzeit";
    m_de["upgrade.16.name"] = "Rissverstaerker";      m_de["upgrade.16.desc"] = "+20% Faehigkeitsschaden";
    m_de["upgrade.17.name"] = "Barrierekern";         m_de["upgrade.17.desc"] = "+1 Schildkapazitaet";

    // Challenge modes
    m_de["challenge.1.name"] = "Speedrun";           m_de["challenge.1.desc"] = "10-Minuten-Timer. Schlag die Uhr!";
    m_de["challenge.2.name"] = "Glaskanone";         m_de["challenge.2.desc"] = "1 HP, 3x SCH, keine Heilung";
    m_de["challenge.3.name"] = "Boss-Ansturm";       m_de["challenge.3.desc"] = "Nur Bosskämpfe";
    m_de["challenge.4.name"] = "Endloser Riss";      m_de["challenge.4.desc"] = "Unendliche Level, steigende Schwierigkeit";
    m_de["challenge.5.name"] = "Dimensionssperre";   m_de["challenge.5.desc"] = "Kein Dimensionswechsel";

    // Mutators
    m_de["mutator.1.name"] = "Grosskopf";            m_de["mutator.1.desc"] = "Alle Hitboxen +50%";
    m_de["mutator.2.name"] = "Zerbrechliche Welt";   m_de["mutator.2.desc"] = "Alle Kacheln zerstoerbar";
    m_de["mutator.3.name"] = "Splittersturm";        m_de["mutator.3.desc"] = "3x Splitter, 2x Gegner";
    m_de["mutator.4.name"] = "Dim-Fluss";            m_de["mutator.4.desc"] = "Auto-Wechsel alle 15s";
    m_de["mutator.5.name"] = "Kugelhagel";           m_de["mutator.5.desc"] = "Alle Gegner schiessen";
    m_de["mutator.6.name"] = "Niedrige Gravitation";  m_de["mutator.6.desc"] = "Halbe Schwerkraft, hohe Spruenge";

    // Achievement names & descriptions (DE only — EN falls back to struct data)
    m_de["ach.first_blood.name"]     = "Erstes Blut";        m_de["ach.first_blood.desc"]     = "Toete deinen ersten Gegner";
    m_de["ach.rift_walker.name"]     = "Risswandler";        m_de["ach.rift_walker.desc"]     = "Repariere deinen ersten Riss";
    m_de["ach.room_clearer.name"]    = "Raeumeraeumer";      m_de["ach.room_clearer.desc"]    = "Saeuber 10 Raeume in einem Lauf";
    m_de["ach.boss_slayer.name"]     = "Bosstöter";          m_de["ach.boss_slayer.desc"]     = "Besiege einen Boss";
    m_de["ach.wyrm_hunter.name"]     = "Wyrmjaeger";         m_de["ach.wyrm_hunter.desc"]     = "Besiege den Leere-Wyrm";
    m_de["ach.combo_king.name"]      = "Kombokoenig";        m_de["ach.combo_king.desc"]      = "Erreiche eine 10-Treffer-Kombo";
    m_de["ach.unstoppable.name"]     = "Unaufhaltsam";       m_de["ach.unstoppable.desc"]     = "Erreiche Schwierigkeit 5";
    m_de["ach.shard_hoarder.name"]   = "Splitterhorter";     m_de["ach.shard_hoarder.desc"]   = "Sammle insgesamt 1000 Splitter";
    m_de["ach.full_upgrade.name"]    = "Voll aufgeruestet";  m_de["ach.full_upgrade.desc"]    = "Maximiere ein Upgrade";
    m_de["ach.mini_boss_hunter.name"]= "Mini-Boss-Jaeger";   m_de["ach.mini_boss_hunter.desc"]= "Besiege einen Mini-Boss";
    m_de["ach.elemental_slayer.name"]= "Elementartoeter";    m_de["ach.elemental_slayer.desc"]= "Toete einen elementaren Gegner";
    m_de["ach.dash_master.name"]     = "Dash-Meister";       m_de["ach.dash_master.desc"]     = "Dashe 100 mal in einem Lauf";
    m_de["ach.dimension_hopper.name"]= "Dimensionsspringer"; m_de["ach.dimension_hopper.desc"]= "Wechsle insgesamt 50 mal die Dimension";
    m_de["ach.grade_s.name"]         = "S-Rang";             m_de["ach.grade_s.desc"]         = "Erreiche Note S in einem Lauf";
    m_de["ach.survivor.name"]        = "Ueberlebender";      m_de["ach.survivor.desc"]        = "Ueberlebe 20 Raeume in einem Lauf";
    m_de["ach.aerial_ace.name"]      = "Luftakrobat";        m_de["ach.aerial_ace.desc"]      = "Toete 5 Gegner in der Luft in einem Lauf";
    m_de["ach.flawless_floor.name"]  = "Makellos";           m_de["ach.flawless_floor.desc"]  = "Schliesse ein Level ohne Schaden ab";
    m_de["ach.resonance_master.name"]= "Resonanzmeister";    m_de["ach.resonance_master.desc"]= "Erreiche die hoechste Resonanzstufe";
    m_de["ach.combo_legend.name"]    = "Kombolegende";       m_de["ach.combo_legend.desc"]    = "Erreiche eine 15-Treffer-Kombo";
    m_de["ach.dash_slayer.name"]     = "Dash-Toeter";        m_de["ach.dash_slayer.desc"]     = "Toete 10 Gegner per Dash in einem Lauf";
    m_de["ach.charged_fury.name"]    = "Aufgeladene Wut";    m_de["ach.charged_fury.desc"]    = "Toete 3 Gegner mit Auflade-Angriffen in einem Lauf";
    m_de["ach.entropy_master.name"]  = "Entropiemeister";    m_de["ach.entropy_master.desc"]  = "Schliesse ein Level nur mit Entropiensense ab";
    m_de["ach.quest_helper.name"]    = "Hilfreicher Reisender"; m_de["ach.quest_helper.desc"] = "Schliesse 5 NPC-Quests ab";
    m_de["ach.speed_demon.name"]     = "Geschwindigkeitsdaemon"; m_de["ach.speed_demon.desc"] = "Beende das Spiel in unter 10 Minuten";
    m_de["ach.weapon_collector.name"]= "Waffenmeister";      m_de["ach.weapon_collector.desc"]= "Schalte alle 12 Waffen frei";
    m_de["ach.dimension_dancer.name"]= "Dimensionstänzer";   m_de["ach.dimension_dancer.desc"]= "Wechsle 50 mal die Dimension in einem Lauf";
    m_de["ach.void_mastery.name"]    = "Leeremeister";       m_de["ach.void_mastery.desc"]    = "Gewinne einen Lauf als Voidwalker";
    m_de["ach.berserk_mastery.name"] = "Berserkerlord";      m_de["ach.berserk_mastery.desc"] = "Gewinne einen Lauf als Berserker";
    m_de["ach.phantom_mastery.name"] = "Geisterprotokoll";   m_de["ach.phantom_mastery.desc"] = "Gewinne einen Lauf als Phantom";
    m_de["ach.tech_mastery.name"]    = "Chefingenieur";      m_de["ach.tech_mastery.desc"]    = "Gewinne einen Lauf als Technomancer";
    m_de["ach.all_classes.name"]     = "Alleskoenner";       m_de["ach.all_classes.desc"]     = "Gewinne einen Lauf mit jeder Klasse";
    m_de["ach.relic_collector.name"] = "Relikthorter";       m_de["ach.relic_collector.desc"] = "Halte 6 Relikte in einem Lauf";
    m_de["ach.synergy_hunter.name"]  = "Synergist";          m_de["ach.synergy_hunter.desc"]  = "Aktiviere 3 Relikt-Synergien in einem Lauf";
    m_de["ach.cursed_survivor.name"] = "Fluchtraeger";       m_de["ach.cursed_survivor.desc"] = "Gewinne einen Lauf mit 3 verfluchten Relikten";
    m_de["ach.parry_master.name"]    = "Parierkoenig";       m_de["ach.parry_master.desc"]    = "Pariere 20 Angriffe in einem Lauf";
    m_de["ach.no_damage_boss.name"]  = "Unberuehrbar";       m_de["ach.no_damage_boss.desc"]  = "Besiege einen Boss ohne Schaden zu nehmen";
    m_de["ach.kill_streak_50.name"]  = "Massaker";           m_de["ach.kill_streak_50.desc"]  = "Toete 50 Gegner in einem Level";
    m_de["ach.ranged_only.name"]     = "Scharfschuetze";     m_de["ach.ranged_only.desc"]     = "Schliesse 5 Etagen nur mit Fernkampf ab";
    m_de["ach.lore_hunter.name"]     = "Wissenhueter";       m_de["ach.lore_hunter.desc"]     = "Entdecke 12 Wissens-Fragmente";
    m_de["ach.ascension_1.name"]     = "New Game Plus";      m_de["ach.ascension_1.desc"]     = "Schliesse Aufstiegsstufe 1 ab";
    m_de["ach.ascension_5.name"]     = "Wahrer Risswandler"; m_de["ach.ascension_5.desc"]     = "Schliesse Aufstiegsstufe 5 ab";

    // Relic names & descriptions (DE only — EN falls back to RelicData struct)
    m_de["relic.1.name"]  = "Eisenherz";        m_de["relic.1.desc"]  = "+15 Max HP";
    m_de["relic.2.name"]  = "Flinke Stiefel";   m_de["relic.2.desc"]  = "+10% Tempo";
    m_de["relic.3.name"]  = "Splittermagnet";    m_de["relic.3.desc"]  = "+50% Aufsammelradius";
    m_de["relic.4.name"]  = "Dornenruestung";    m_de["relic.4.desc"]  = "8 SCH Reflexion";
    m_de["relic.5.name"]  = "Flinke Haende";     m_de["relic.5.desc"]  = "+15% Angriffsgeschwindigkeit";
    m_de["relic.6.name"]  = "Gluecksmuenze";     m_de["relic.6.desc"]  = "+10% Splitter-Drop";
    m_de["relic.7.name"]  = "Blutrausch";        m_de["relic.7.desc"]  = "+25% SCH unter 30% HP";
    m_de["relic.8.name"]  = "Echoschlag";        m_de["relic.8.desc"]  = "20% Doppeltreffer";
    m_de["relic.9.name"]  = "Phasenumhang";      m_de["relic.9.desc"]  = "1s unsichtbar nach Wechsel";
    m_de["relic.10.name"] = "Entropieanker";     m_de["relic.10.desc"] = "-40% Entropie (Boss)";
    m_de["relic.11.name"] = "Kettenblitz";       m_de["relic.11.desc"] = "Kill: 15 SCH Blitz";
    m_de["relic.12.name"] = "Seelensauger";      m_de["relic.12.desc"] = "Kills heilen 3 HP";
    m_de["relic.13.name"] = "Berserkerkern";     m_de["relic.13.desc"] = "+50% SCH, -30% HP";
    m_de["relic.14.name"] = "Zeitdehner";        m_de["relic.14.desc"] = "-30% Faehigkeits-CD";
    m_de["relic.15.name"] = "Dimensionsecho";    m_de["relic.15.desc"] = "Trifft beide Dimensionen";
    m_de["relic.16.name"] = "Phoenixfeder";      m_de["relic.16.desc"] = "1x Auto-Wiederbelebung";
    m_de["relic.17.name"] = "Leerehunger";       m_de["relic.17.desc"] = "Kill: +1% SCH (Lauf)";
    m_de["relic.18.name"] = "Chaoskugel";        m_de["relic.18.desc"] = "Zufallseffekt / 30s";
    m_de["relic.19.name"] = "Verfluchte Klinge"; m_de["relic.19.desc"] = "+40% Nahkampf, -20% Fernkampf";
    m_de["relic.20.name"] = "Glasherz";          m_de["relic.20.desc"] = "+50% Max HP, 1.6x Schaden";
    m_de["relic.21.name"] = "Zeitsteuer";        m_de["relic.21.desc"] = "-50% Faehigkeits-CD, kostet 5 HP";
    m_de["relic.22.name"] = "Entropieschwamm";   m_de["relic.22.desc"] = "Keine passive Entropie, Kills +5%";
    m_de["relic.23.name"] = "Leerepakt";         m_de["relic.23.desc"] = "Kill heilt 5 HP, max 60% HP";
    m_de["relic.24.name"] = "Chaosriss";         m_de["relic.24.desc"] = "10. Kill: Buff, 5. Treffer: Spitze";
    m_de["relic.25.name"] = "Rissleiter";        m_de["relic.25.desc"] = "Wechsel: +10% ANG-Tempo 3s (x3)";
    m_de["relic.26.name"] = "Dualitaetsstein";   m_de["relic.26.desc"] = "Dim A: +25% SCH, Dim B: +25% Ruestung";
    m_de["relic.27.name"] = "Dim.-Rueckstand";   m_de["relic.27.desc"] = "Wechsel: 15 SCH/s Zone 2s";
    m_de["relic.28.name"] = "Rissmantel";        m_de["relic.28.desc"] = "-50% Wechsel-CD, kostet 5% Max HP";
    m_de["relic.29.name"] = "Stabilitaetsmatrix"; m_de["relic.29.desc"] = "+3% SCH/s in Dim (max 30%)";
    m_de["relic.30.name"] = "Leereresonanz";     m_de["relic.30.desc"] = "Cross-Dim-Kill: 2x SCH";
    m_de["relic.31.name"] = "Blutpakt";          m_de["relic.31.desc"] = "+40% SCH, -1 HP pro Kill";
    m_de["relic.32.name"] = "Entropiesiphon";    m_de["relic.32.desc"] = "Kill: -8 Entropie, +50% Zunahme";
    m_de["relic.33.name"] = "Glaskanone";        m_de["relic.33.desc"] = "+60% SCH, Max HP halbiert";
    m_de["relic.34.name"] = "Vampirklinge";      m_de["relic.34.desc"] = "Kill: +3 HP, keine passive Heilung";
    m_de["relic.35.name"] = "Chaoskern";         m_de["relic.35.desc"] = "+25% Werte, Wechsel alle 20s";
    m_de["relic.36.name"] = "Berserkerfluch";    m_de["relic.36.desc"] = "+15% SCH pro fehlende 10% HP, kein Schild";
    m_de["relic.37.name"] = "Zeitverzerrung";    m_de["relic.37.desc"] = "+30% Tempo+ANG, 50% langsamerer Entropieabbau";
    m_de["relic.38.name"] = "Seelenegel";        m_de["relic.38.desc"] = "2x Splitter, -5 HP pro Level";

    // Difficulty Select
    m_de["difficulty.title"]       = "S C H W I E R I G K E I T";
    m_de["difficulty.easy"]        = "L E I C H T";
    m_de["difficulty.normal"]      = "N O R M A L";
    m_de["difficulty.hard"]        = "S C H W E R";
    m_de["difficulty.easy_desc"]   = "Weniger Gegner, +50% Splitter";
    m_de["difficulty.normal_desc"] = "Standarderfahrung";
    m_de["difficulty.hard_desc"]   = "Mehr Gegner, staerkere Bosse, -25% Splitter";
    m_de["difficulty.class"]       = "Klasse: %s";
    m_de["difficulty.nav_hint"]    = "W/S Navigieren  |  ENTER Auswaehlen  |  ESC Zurueck";

    // Achievements
    m_de["achievements.title"]     = "E R F O L G E";
    m_de["achievements.counter"]   = "%d / %d freigeschaltet";
    m_de["achievements.nav_hint"]  = "[W/S] Scrollen   [ESC] Zurueck";

    // Bestiary
    m_de["bestiary.title"]         = "B E S T I A R I U M";
    m_de["bestiary.discovered"]    = "Entdeckt: %d / %d";
    m_de["bestiary.kills"]         = "Kills: %d";
    m_de["bestiary.unencountered"] = "Nicht angetroffen";
    m_de["bestiary.lore"]          = "WISSEN:";
    m_de["bestiary.abilities"]     = "FAEHIGKEITEN:";
    m_de["bestiary.weakness"]      = "SCHWAECHE:";
    m_de["bestiary.effective"]     = "EFFEKTIV MIT:";
    m_de["bestiary.unknown_boss"]  = "Unbekannter Boss";
    m_de["bestiary.unknown_enemy"] = "Unbekannter Gegner";
    m_de["bestiary.encounter"]     = "Begegne diesem Gegner, um seine Daten freizuschalten.";
    m_de["bestiary.defeat"]        = "Besiege ihn, um seinen Eintrag dauerhaft freizuschalten.";
    m_de["bestiary.nav_hint"]      = "W/S  Navigieren     ESC  Zurueck";
    m_de["bestiary.abilities_locked"] = "FAEHIGKEITEN:   [ GESPERRT ]";
    m_de["bestiary.weakness_locked"]  = "SCHWAECHE:  [ GESPERRT ]";
    m_de["element.fire"]           = "Feuer";
    m_de["element.ice"]            = "Eis";
    m_de["element.electric"]       = "Blitz";
    m_de["element.none"]           = "Keine";

    // Challenges
    m_de["challenges.title"]       = "H E R A U S F O R D E R U N G E N";
    m_de["challenges.mutators"]    = "Mutatoren (max 2)";
    m_de["challenges.nav_hint"]    = "W/S Navigieren  |  A/D Wechseln  |  ENTER Auswaehlen  |  ESC Zurueck";

    // NG+ Select
    m_de["ngplus.title"]           = "N E W  G A M E +";
    m_de["ngplus.subtitle"]        = "Schwierigkeitsstufe waehlen (hoeher = schwerer + mehr Splitter)";
    m_de["ngplus.next_challenge"]  = "[NAECHSTE STUFE]";
    m_de["ngplus.nav_hint"]        = "W/S Navigieren  |  ENTER Bestaetigen  |  ESC Zurueck";
    m_de["ngplus.tier.0"]          = "N O R M A L";
    m_de["ngplus.tier.1"]          = "N G + 1";
    m_de["ngplus.tier.2"]          = "N G + 2";
    m_de["ngplus.tier.3"]          = "N G + 3";
    m_de["ngplus.tier.4"]          = "N G + 4";
    m_de["ngplus.tier.5"]          = "N G + 5";
    m_de["ngplus.tier.6"]          = "N G + 6";
    m_de["ngplus.tier.7"]          = "N G + 7";
    m_de["ngplus.tier.8"]          = "N G + 8";
    m_de["ngplus.tier.9"]          = "N G + 9";
    m_de["ngplus.tier.10"]         = "N G + 10";
    m_de["ngplus.desc.0"]          = "Standardschwierigkeit";
    m_de["ngplus.desc.1"]          = "+10% Gegner-HP, langsamerer Entropieverfall";
    m_de["ngplus.desc.2"]          = "+15% Gegner-DMG, +1 Relikt-Auswahl bei Boss";
    m_de["ngplus.desc.3"]          = "+20% Elite-Spawn, Waffenupgrades behalten";
    m_de["ngplus.desc.4"]          = "Boss +20% HP, extra Shop-Platz";
    m_de["ngplus.desc.5"]          = "Haertere Muster, Start mit gewoehnlichem Relikt";
    m_de["ngplus.desc.6"]          = "+15% Splitter-Ertrag, Fallen +30%";
    m_de["ngplus.desc.7"]          = "Faehigkeiten: 50% CD-Start, Boss-Extraphase";
    m_de["ngplus.desc.8"]          = "Start mit 2 Relikten, Eliten erhalten 2 Modifikatoren";
    m_de["ngplus.desc.9"]          = "Shop -20% Preise, Gegner +30% Tempo";
    m_de["ngplus.desc.10"]         = "Start mit legendaerem Relikt — CHAOS-MODUS";
    m_de["ngplus.reward.1"]        = "Belohnung: +20% Splitter-Bonus";
    m_de["ngplus.reward.2"]        = "Belohnung: +40% Splitter-Bonus";
    m_de["ngplus.reward.3"]        = "Belohnung: +60% Splitter-Bonus";
    m_de["ngplus.reward.4"]        = "Belohnung: +80% Splitter-Bonus";
    m_de["ngplus.reward.5"]        = "Belohnung: +100% Splitter-Bonus";
    m_de["ngplus.reward.6"]        = "Belohnung: +120% Splitter-Bonus";
    m_de["ngplus.reward.7"]        = "Belohnung: +150% Splitter-Bonus";
    m_de["ngplus.reward.8"]        = "Belohnung: +180% Splitter-Bonus";
    m_de["ngplus.reward.9"]        = "Belohnung: +200% Splitter-Bonus";
    m_de["ngplus.reward.10"]       = "Belohnung: +250% Splitter-Bonus  |  CHAOS-MEISTER-Titel";

    // Keybindings
    m_de["keybindings.title"]      = "S T E U E R U N G";
    m_de["keybindings.reset"]      = "Standardwerte";
    m_de["keybindings.back"]       = "Zurueck";
    m_de["keybindings.listen_hint"]= "Taste druecken zum Binden  |  ESC Abbrechen";
    m_de["keybindings.nav_hint"]   = "W/S Navigieren  |  ENTER Binden  |  ESC Zurueck";

    // Daily Leaderboard
    m_de["daily.title"]            = "TAGESRANGLISTE";
    m_de["daily.today_seed"]       = "Heute: %s  |  Seed: %d";
    m_de["daily.best_today"]       = "Bester Heute: %d";
    m_de["daily.no_runs"]          = "Noch keine Laeufe heute";
    m_de["daily.score"]            = "Punkte";
    m_de["daily.class"]            = "Klasse";
    m_de["daily.floors"]           = "Etagen";
    m_de["daily.kills"]            = "Kills";
    m_de["daily.rifts"]            = "Risse";
    m_de["daily.combo"]            = "Kombo";
    m_de["daily.time"]             = "Zeit";
    m_de["daily.result"]           = "Ergebnis";
    m_de["daily.victory"]          = "Sieg";
    m_de["daily.fallen"]           = "Gefallen";
    m_de["daily.no_runs_recorded"] = "Keine Laeufe fuer heute. Starte einen Tageslauf!";
    m_de["daily.previous_days"]    = "Vorherige Tage";
    m_de["daily.no_history"]       = "Noch keine Historie";
    m_de["daily.back_hint"]        = "[ESC] Zurueck";

    // Lore
    m_de["lore.title"]             = "~ KODEX ~";
    m_de["lore.fragments"]         = "%d / %d Fragmente";
    m_de["lore.undiscovered"]      = "Dieses Fragment wurde noch nicht entdeckt.";
    m_de["lore.nav_hint"]          = "W/S: Navigieren   ESC: Zurueck";

    // Tutorial hints
    m_de["tut.move"]               = "Bewegen";
    m_de["tut.jump"]               = "Springen";
    m_de["tut.dash"]               = "Durch Gefahr dashen (auch in der Luft!)";
    m_de["tut.melee"]              = "Nahkampf-Angriff";
    m_de["tut.ranged"]             = "Fernkampf (halten fuer aufgeladenen Schuss!)";
    m_de["tut.dimension"]          = "Dimension wechseln - Wege unterscheiden sich!";
    m_de["tut.rift_repair"]        = "Repariere diesen Riss! Loese das Raetsel um Entropie zu senken.";
    m_de["tut.entropy_warning"]    = "Entropie steigt! Repariere Risse. Bei 100% versagt der Anzug!";
    m_de["tut.conveyor"]           = "Foerderband - schiebt dich mit. Nutze es fuer Geschwindigkeit!";
    m_de["tut.dim_platform"]       = "Leuchtende Plattform - existiert nur in einer Dimension! Wechsle mit [E]";
    m_de["tut.wall_slide"]         = "Wandrutschen! Springe von Waenden fuer hoehere Bereiche.";
    m_de["tut.abilities"]          = "Nutze Spezialfaehigkeiten: Stampfer / Schild / Phasenschlag";
    m_de["tut.relic_choice"]       = "Waehle ein Relikt! Jedes gibt einen einzigartigen Bonus fuer diesen Lauf.";
    m_de["tut.combo"]              = "Kombo x3! Kettentreffern ohne Pause fuer Bonusschaden!";
    m_de["tut.exit"]               = "Alle Risse repariert! Finde den AUSGANG bevor die Dimension kollabiert!";
    m_de["tut.weapon_switch"]      = "Waffen wechseln: [Q] Nahkampf  [R] Fernkampf - probiere verschiedene Stile!";
    m_de["tut.rift_dimb"]          = "Dieser Riss stabilisiert nur in DIM-B. Wechsle mit [E], aber Entropie steigt schneller dort.";
    m_de["tut.rift_dima"]          = "Dieser Riss stabilisiert nur in DIM-A. Wechsle mit [E] um ihn sicher zu reparieren.";
    m_de["tut.collapsing"]         = "Dimension kollabiert! Erreiche den AUSGANG bevor die Zeit ablaeuft!";

    // Tutorial state pages
    m_de["tut.nav_hint"]                = "A/D oder Pfeiltasten zum Blaettern | ENTER weiter | ESC ueberspringen";
    m_de["tut.page.welcome.title"]      = "Willkommen bei Riftwalker";
    m_de["tut.page.welcome.1"]          = "Du bist ein Riftwalker — ein Erforscher zerbrochener Dimensionen.";
    m_de["tut.page.welcome.2"]          = "Die Realitaet bricht auseinander. Dimensionsrisse drohen alles zu verschlingen.";
    m_de["tut.page.welcome.3"]          = "Deine Mission: Durchquere 30 Etagen, repariere Risse, stoppe den Kollaps.";
    m_de["tut.page.welcome.4"]          = "Dies ist ein Roguelike: Tod ist endgueltig, aber Wissen bleibt.";
    m_de["tut.page.welcome.5"]          = "Jeder Durchlauf macht dich staerker. Lerne, passe dich an, ueberwinde.";

    m_de["tut.page.controls.title"]     = "Bewegung & Steuerung";
    m_de["tut.page.controls.move"]      = "Bewege dich mit WASD oder Pfeiltasten";
    m_de["tut.page.controls.jump"]      = "Springen (auch Doppelsprung in der Luft!)";
    m_de["tut.page.controls.dash"]      = "Ausweichen — fuer einen Moment unverwundbar";
    m_de["tut.page.controls.wall"]      = "Rutsche an Waenden und springe ab um hoeher zu kommen.";

    m_de["tut.page.combat.title"]       = "Kampf";
    m_de["tut.page.combat.melee"]       = "Nahkampfangriff — schnell, kurze Reichweite";
    m_de["tut.page.combat.ranged"]      = "Fernkampfangriff — halte fuer aufgeladene Schuesse";
    m_de["tut.page.combat.parry"]       = "Time deinen Nahkampf kurz vor einem Feindhit — PARIEREN fuer Bonusschaden!";
    m_de["tut.page.combat.combo"]       = "Verkette Treffer ohne Pause fuer steigende Kombo-Schadensboni.";
    m_de["tut.page.combat.weapons"]     = "Wechsle Waffen um deinen Stil zu finden:";

    m_de["tut.page.dim.title"]          = "Dimensionswechsel";
    m_de["tut.page.dim.core"]           = "Die Kernmechanik — wechsle zwischen zwei parallelen Dimensionen:";
    m_de["tut.page.dim.a_label"]        = "Dimension A";
    m_de["tut.page.dim.a_desc"]         = "Vertrautes Terrain, schwaecher Gegner";
    m_de["tut.page.dim.a_desc2"]        = "Langsameres Entropie-Wachstum";
    m_de["tut.page.dim.b_label"]        = "Dimension B";
    m_de["tut.page.dim.b_desc"]         = "Haertere Gegner, bessere Belohnungen";
    m_de["tut.page.dim.b_desc2"]        = "Schnelleres Entropie-Wachstum";
    m_de["tut.page.dim.entropy"]        = "Behalte dein Entropie-Meter im Auge! Bei 100% versagt dein Anzug.";
    m_de["tut.page.dim.rifts"]          = "Repariere dimensionale Risse um die Entropie zu senken:";

    m_de["tut.page.prog.title"]         = "Fortschritt & Strategie";
    m_de["tut.page.prog.roguelike"]     = "Tod beendet den Durchlauf — aber Upgrades bleiben!";
    m_de["tut.page.prog.death"]         = "Gib Rift-Scherben fuer permanente Verbesserungen aus.";
    m_de["tut.page.prog.upgrades"]      = "Gesundheit, Schaden, Ausweichladungen, Faehigkeiten — alles verbesserbar.";
    m_de["tut.page.prog.classes"]       = "4 Klassen: Voidwalker, Berserker, Phantom, Technomancer";
    m_de["tut.page.prog.relics"]        = "Finde Relikte waehrend Durchlaeufen fuer maechtige passive Boni.";
    m_de["tut.page.prog.floors"]        = "30 Etagen ueber 5 Zonen, jede mit eigenen Gefahren und Gegnern.";
    m_de["tut.page.prog.bosses"]        = "Besiege Zonenbosse um voranzukommen. Jeder hat eigene Angriffsmuster.";

    m_de["tut.page.ready.title"]        = "Bereit zum Riss!";
    m_de["tut.page.ready.1"]            = "Du weisst was noetig ist. Zeit den Riss zu durchschreiten.";
    m_de["tut.page.ready.2"]            = "Waehle deine Klasse, bestimme die Schwierigkeit und tauche ein.";
    m_de["tut.page.ready.3"]            = "Im Spiel fuehren dich Hinweise durch die erste Etage.";
    m_de["tut.page.ready.tip1"]         = "Tipp: Erkunde beide Dimensionen — manche Wege gibt es nur in einer!";
    m_de["tut.page.ready.tip2"]         = "Tipp: Parieren ist der schnellste Weg zu massivem Schaden.";
    m_de["tut.page.ready.tip3"]         = "Tipp: Pruefe das Bestiarium um Gegner-Schwaechen zu erfahren.";
    m_de["tut.page.ready.start"]        = ">>> Druecke ENTER um deine Klasse zu waehlen <<<";
    m_de["menu.tutorial"]               = "Tutorial";

    // Death causes
    m_de["death.hp"]               = "HP aufgebraucht";
    m_de["death.entropy"]          = "Entropie-Ueberladung";
    m_de["death.rift"]             = "Risskollaps";
    m_de["death.time"]             = "Zeit abgelaufen";
    m_de["death.victory"]          = "Sieg!";
    m_de["death.unknown"]          = "Unbekannt";

    // Upgrades
    m_de["upgrades.title"]         = "ANZUG-VERBESSERUNGEN";
    m_de["upgrades.level"]         = "Lv %d/%d";
    m_de["upgrades.cost"]          = "Kosten: %d";
    m_de["upgrades.maxed"]         = "MAXIMAL";
    m_de["upgrades.nav_hint"]      = "W/S: Navigieren    ENTER: Kaufen    ESC: Zurueck";

    // Splash
    m_de["splash.subtitle"]        = "Ein Dimensions-Roguelike";
    m_de["splash.press_any"]       = "Beliebige Taste druecken";

    // Menu subtitle
    m_de["menu.subtitle"]          = "Dimensionsmechanik";

    // Relic choice
    m_de["relic.choose"]           = "Waehle ein Relikt";

    // Run History
    m_de["history.title"]          = "LAUFCHRONIK";
    m_de["history.stats"]          = "Laeufe: %d    Gegner: %d    Beste Etage: %d    Risse: %d";
    m_de["history.avg_kills"]      = "Ø Kills/Lauf: %.1f";
    m_de["history.class"]          = "Klasse";
    m_de["history.floor"]          = "Etage";
    m_de["history.kills"]          = "Kills";
    m_de["history.rifts"]          = "Risse";
    m_de["history.combo"]          = "Kombo";
    m_de["history.time"]           = "Zeit";
    m_de["history.shards"]         = "Splitter";
    m_de["history.diff"]           = "Schw.";
    m_de["history.result"]         = "Ergebnis";
    m_de["history.empty"]          = "Noch keine Laeufe. Starte einen Lauf!";
    m_de["history.scroll"]         = "[W/S Scrollen - %d/%d]";
    m_de["history.back"]           = "[ESC] Zurueck";

    // Shop
    m_de["shop.title"]             = "- RISSMARKT -";
    m_de["shop.skip"]              = "WEITER >>";
    m_de["shop.tutorial"]          = "Gib Risssplitter aus, um deine Faehigkeiten fuer die naechste Etage zu verbessern!";
    m_de["shop.nav_hint"]          = "A/D Auswaehlen  |  Enter Kaufen  |  ESC Ueberspringen";
    m_de["shop.common"]            = "GEWOEHNLICH";
    m_de["shop.rare"]              = "SELTEN";
    m_de["shop.legendary"]         = "LEGENDAER";
    m_de["shop.cost"]              = "%d Splitter";
    m_de["shop.buy"]               = "[ENTER] Kaufen";
    m_de["shop.no_shards"]         = "Nicht genug Splitter";

    // Zone names & taglines
    m_de["zone.1.name"]            = "GEBROCHENE SCHWELLE";
    m_de["zone.1.tagline"]         = "Wo die Realitaet zuerst zu brechen begann";
    m_de["zone.2.name"]            = "VERSCHIEBENDE TIEFEN";
    m_de["zone.2.tagline"]         = "Die Dimensionen bluten ineinander";
    m_de["zone.3.name"]            = "RESONANTER KERN";
    m_de["zone.3.tagline"]         = "Jeder Schritt hallt durch alle Realitaeten";
    m_de["zone.4.name"]            = "ENTROPIE-KASKADE";
    m_de["zone.4.tagline"]         = "Ordnung loest sich auf. Chaos herrscht.";
    m_de["zone.5.name"]            = "DIE DOMAENE DES SOUVERAENS";
    m_de["zone.5.tagline"]         = "Das Herz des Risses wartet";

    // Boss names
    m_de["boss.rift_guardian"]     = "RISSWAECHTER";
    m_de["boss.void_wyrm"]        = "LEERENWURM";
    m_de["boss.dim_architect"]     = "DIMENSIONSARCHITEKT";
    m_de["boss.temporal_weaver"]   = "ZEITENWEBER";
    m_de["boss.void_sovereign"]    = "LEERENSOUVERAEN";
    m_de["boss.entropy_incarnate"] = "ENTROPIE INKARNIERT";

    // Boss intro subtitles
    m_de["boss.sub.rift_guardian"]     = "Der Riss erinnert sich an dein Gesicht";
    m_de["boss.sub.rift_guardian_alt"] = "Die Immunantwort der Dimension";
    m_de["boss.sub.void_wyrm"]        = "Er gleitet zwischen den Dimensionen";
    m_de["boss.sub.dim_architect"]     = "Er schreibt die Realitaet selbst um";
    m_de["boss.sub.temporal_weaver"]   = "Vergangenheit und Zukunft verschmelzen";
    m_de["boss.sub.entropy_incarnate"] = "Chaos in Form und mit Zweck";
    m_de["boss.sub.entropy_alt"]       = "Der Anzug hungert nach Verfall";
    m_de["boss.sub.void_sovereign"]    = "Die letzte Kette, die den Riss bindet";

    // Run intro narrative
    m_de["intro.line1"]            = "Der Riss oeffnet sich...";
    m_de["intro.line1_ng"]         = "Der Riss erinnert sich an dich...";
    m_de["intro.line2"]            = "Dein Anzug flackert. Entropie steigt.";
    m_de["intro.line2_ng"]         = "Aufstiegsstufe %d. Die Leere wird staerker.";

    // Ending narratives (healer)
    m_de["ending.h.0"]  = "Du tratest in den Riss auf der Suche nach Antworten.";
    m_de["ending.h.1"]  = "Stattdessen fandest du nur neue Fragen.";
    m_de["ending.h.2"]  = "Jede Dimension war ein Fragment einer zerbrochenen Welt,";
    m_de["ending.h.3"]  = "jeder Gegner ein Schatten dessen, was einst war.";
    m_de["ending.h.4"]  = "Der Leerensouveraen war die letzte Kette,";
    m_de["ending.h.5"]  = "die den Riss an diese Realitaet band.";
    m_de["ending.h.6"]  = "Mit seiner Niederlage beginnt die Wunde zu heilen.";
    m_de["ending.h.7"]  = "Die Dimensionen verschmelzen langsam wieder.";
    m_de["ending.h.8"]  = "Aber du kennst jetzt die Wahrheit:";
    m_de["ending.h.9"]  = "Der Riss war nie der Feind.";
    m_de["ending.h.10"] = "Er war ein Spiegel.";
    m_de["ending.h.11"] = "Und Spiegel koennen niemals wirklich zersplittern.";
    m_de["ending.h.12"] = "Nur verstanden werden.";

    // Ending narratives (destroyer)
    m_de["ending.d.0"]  = "Du kamst mit einem Ziel in den Riss.";
    m_de["ending.d.1"]  = "Nicht um zu verstehen. Nicht um zu heilen.";
    m_de["ending.d.2"]  = "Sondern um zu zerstoeren.";
    m_de["ending.d.3"]  = "Jeder Gegner fiel vor dir,";
    m_de["ending.d.4"]  = "jede Dimension erzitterte bei deiner Ankunft.";
    m_de["ending.d.5"]  = "Der Leerensouveraen hatte keine Chance.";
    m_de["ending.d.6"]  = "Du hast den Riss nicht nur versiegelt --";
    m_de["ending.d.7"]  = "du hast ihn zersplittert.";
    m_de["ending.d.8"]  = "Die Dimensionen verschmelzen jetzt gewaltsam.";
    m_de["ending.d.9"]  = "Keine Heilung. Zusammenbruch.";
    m_de["ending.d.10"] = "Vielleicht war Zerstoerung immer die Antwort.";
    m_de["ending.d.11"] = "Oder vielleicht brauchte der Riss eine sanftere Hand.";
    m_de["ending.d.12"] = "Du wirst es nie erfahren.";

    // Ending narratives (speedrunner)
    m_de["ending.s.0"]  = "Die Zeit biegt sich um jene, die durch den Riss eilen.";
    m_de["ending.s.1"]  = "Du durchquertest dreissig Etagen in wenigen Minuten.";
    m_de["ending.s.2"]  = "Die Dimensionen hatten kaum Zeit, dich zu bemerken.";
    m_de["ending.s.3"]  = "Der Leerensouveraen hatte kaum Zeit zu kaempfen.";
    m_de["ending.s.4"]  = "Du warst ein Geist.";
    m_de["ending.s.5"]  = "Eine Welle im Gewebe der Realitaet.";
    m_de["ending.s.6"]  = "Der Riss schloss sich hinter dir,";
    m_de["ending.s.7"]  = "bevor er wusste, dass du da warst.";
    m_de["ending.s.8"]  = "Geschwindigkeit ist eine eigene Art der Meisterschaft.";
    m_de["ending.s.9"]  = "Aber was hast du auf dem Weg verpasst?";

    // Ending State
    m_de["ending.sealed"]          = "Der Riss ist versiegelt.";
    m_de["ending.shattered"]       = "Der Riss ist zersplittert.";
    m_de["ending.barely"]          = "Der Riss hat es kaum bemerkt.";
    m_de["ending.skip"]            = "LEERTASTE zum Ueberspringen";
    m_de["ending.continue"]        = "LEERTASTE zum Fortfahren";
    m_de["ending.run_complete"]    = "Lauf abgeschlossen";
    m_de["ending.run_destroyer"]   = "Lauf abgeschlossen - Der Zerstoerer";
    m_de["ending.run_speedrunner"] = "Lauf abgeschlossen - Der Speedrunner";
    m_de["ending.final_score"]     = "Endpunktzahl: %d";
    m_de["ending.enemies"]         = "Gegner besiegt: %d";
    m_de["ending.difficulty"]      = "Max. Schwierigkeit: %d";
    m_de["ending.relics"]          = "Relikte gefunden: %d";
    m_de["ending.time"]            = "Zeit: %d:%02d";
    m_de["ending.thank_you"]       = "Danke fuers Spielen";
    m_de["ending.return"]          = "Der Riss erwartet deine Rueckkehr... Steige hoeher auf.";
    m_de["ending.back"]            = "Beliebige Taste zum Menue";

    // Credits
    m_de["credits.back"]           = "ESC: Zurueck zum Menue";
    m_de["credits.thanks"]         = "Danke fuers Spielen.";
    m_de["credits.return"]         = "Der Riss erwartet deine Rueckkehr...";
    m_de["credits.subtitle"]       = "Ein Roguelike-Plattformer mit Dimensionswechsel";
    m_de["credits.section.design"] = "--- Design & Programmierung ---";
    m_de["credits.design_line"]    = "Erstellt mit Claude Code + Mensch-Kollaboration";
    m_de["credits.section.engine"] = "--- Engine ---";
    m_de["credits.section.arch"]   = "--- Architektur ---";
    m_de["credits.arch.ecs"]       = "Entity Component System (ECS)";
    m_de["credits.arch.procgen"]   = "Prozedurale Levelgenerierung";
    m_de["credits.arch.dualdim"]   = "Dual-Dimensions-Weltsystem";
    m_de["credits.section.gameplay"]= "--- Spielsysteme ---";
    m_de["credits.gameplay.classes"]= "4 spielbare Klassen";
    m_de["credits.gameplay.weapons"]= "12 Waffen mit Meisterschaftsstufen";
    m_de["credits.gameplay.relics"] = "38 Relikte mit 22 Synergie-Kombos";
    m_de["credits.gameplay.bosses"] = "6 Bosse mit Mehrphasen-KI";
    m_de["credits.gameplay.enemies"]= "17 Gegnertypen + 12 Elite-Modifikatoren";
    m_de["credits.gameplay.entropy"]= "Anzug-Entropie-Mechanik";
    m_de["credits.gameplay.ngplus"] = "New Game+ (10 Aufstiegsstufen)";
    m_de["credits.section.combat"] = "--- Kampf ---";
    m_de["credits.combat.melee"]   = "Nahkampf-Kombosystem mit Parieren & Kontern";
    m_de["credits.combat.charged"] = "Aufladeangriffe & Dash-Kampf";
    m_de["credits.combat.abilities"]= "3 Klassenfaehigkeiten pro Klasse";
    m_de["credits.combat.elements"]= "Elementarvarianten (Feuer, Eis, Blitz)";
    m_de["credits.section.world"]  = "--- Welt ---";
    m_de["credits.world.floors"]   = "30 Etagen in 5 Themenzonen";
    m_de["credits.world.puzzles"]  = "Riss-Raetsel & Geheimraeume";
    m_de["credits.world.npcs"]     = "NPCs mit Questreihen";
    m_de["credits.world.lore"]     = "20 entdeckbare Wissens-Fragmente";
    m_de["credits.world.events"]   = "Dynamische Level-Ereignisse";
    m_de["credits.section.progression"]= "--- Fortschritt ---";
    m_de["credits.prog.achievements"]= "42 Erfolge";
    m_de["credits.prog.upgrades"]  = "Permanentes Upgrade-System";
    m_de["credits.prog.daily"]     = "Taegliche Herausforderungslaeufe";
    m_de["credits.prog.challenges"]= "5 Herausforderungsmodi + 6 Mutatoren";
    m_de["credits.prog.bestiary"]  = "Bestiarium mit Kill-Tracking";
    m_de["credits.section.audio"]  = "--- Audio ---";
    m_de["credits.audio.sfx"]      = "Prozedurale Klangerzegung";
    m_de["credits.audio.music"]    = "Dynamisches Musiksystem";
    m_de["credits.section.visual"] = "--- Grafik ---";
    m_de["credits.visual.render"]  = "Prozedurales Rendering mit Sprite-Unterstuetzung";
    m_de["credits.visual.parallax"]= "Parallax-Hintergrund (6 Ebenen)";
    m_de["credits.visual.particles"]= "Partikeleffekte & Bildschirmwackeln";
    m_de["credits.visual.indicators"]= "Schadensanzeigen & Kill-Feed";
    m_de["credits.section.testing"]= "--- Tests ---";
    m_de["credits.testing.bot"]    = "Automatisierter Spieltest-Bot";
    m_de["credits.testing.visual"] = "Visuelles Regressionstesting";
    m_de["credits.testing.balance"]= "Balance-Snapshot-System";
    m_de["credits.section.thanks"] = "--- Besonderer Dank ---";
    m_de["credits.thanks.void"]    = "Die dimensionale Leere, dafuer dass sie da ist";
    m_de["credits.thanks.coffee"]  = "Kaffee, der das alles moeglich gemacht hat";

    // Gameplay tips
    m_de["tip.0"]  = "TIPP: Dimension B hat staerkere Gegner aber bessere Splitter-Belohnungen.";
    m_de["tip.1"]  = "TIPP: Rechtzeitiges Parieren loest einen starken Gegenangriff aus.";
    m_de["tip.2"]  = "TIPP: An einer Wand rutschen: Halte zur Wand waehrend du in der Luft bist.";
    m_de["tip.3"]  = "TIPP: Risse in Dimension B reparieren gibt Bonus-Splitter und senkt Entropie.";
    m_de["tip.4"]  = "TIPP: Relikte koennen sich zu Synergien verbinden - pruefe das Pausenmenue.";
    m_de["tip.5"]  = "TIPP: Kisten enthalten manchmal Heilkugeln und Splitter.";
    m_de["tip.6"]  = "TIPP: Jede Klasse hat eine einzigartige Faehigkeit - nutze sie oft, sie laedt schnell.";
    m_de["tip.7"]  = "TIPP: Entropie steigt ueber Zeit. Repariere Risse und wechsle Dimensionen.";
    m_de["tip.8"]  = "TIPP: Elite-Gegner leuchten und haben Spezialmodifikatoren. Bessere Beute!";
    m_de["tip.9"]  = "TIPP: Die Minikarte (M-Taste) zeigt Riss-Positionen und den Ausgang.";
    m_de["tip.10"] = "TIPP: Durch Gegner dashen gewaehrt kurze Unverwundbarkeit.";
    m_de["tip.11"] = "TIPP: Bosse haben 3 Phasen - ihre Angriffe aendern sich bei 66% und 33% HP.";
    m_de["tip.12"] = "TIPP: Waffenmeisterschaft steigt mit Kills. Hoehere Stufen geben Boni.";
    m_de["tip.13"] = "TIPP: Im Bestiarium findest du Schwaechen und Angriffsmuster der Gegner.";
    m_de["tip.14"] = "TIPP: Manche Relikte sind verflucht - starke Effekte mit gefaehrlichen Nachteilen.";
    m_de["tip.15"] = "TIPP: Aufgeladene Angriffe richten massiven Schaden an. Halte die Angriffstaste.";
    m_de["tip.16"] = "TIPP: New Game+ fuegt neue Modifikatoren hinzu, belohnt aber mit mehr Splittern.";
    m_de["tip.17"] = "TIPP: Verschiedene Waffentypen sind gegen verschiedene Gegner effektiv.";
    m_de["tip.18"] = "TIPP: Anzug-Entropie beeinflusst deine Faehigkeiten. Halte sie niedrig.";
    m_de["tip.19"] = "TIPP: Besuche NPCs fuer Quests, Upgrades und wertvolles Wissen.";
    m_de["tip.20"] = "TIPP: Permanente Upgrades bleiben zwischen Laeufen erhalten.";
    m_de["tip.21"] = "TIPP: Springe von oben auf Gegner fuer einen Stampf-Angriff.";
    m_de["tip.22"] = "TIPP: Geschuetzturm und Fallen des Technomancers kontrollieren das Schlachtfeld.";
    m_de["tip.23"] = "TIPP: Tageslaeufe nutzen einen gemeinsamen Seed - kaempfe um die Hoechstpunktzahl.";
    m_de["tip.24"] = "TIPP: Kombo-Kills ohne Schaden erhoehen deinen Kill-Streak-Multiplikator.";
    m_de["tip.25"] = "TIPP: Der Berserker erhaelt +30% Schaden und Angriffsgeschwindigkeit unter 40% HP.";
    m_de["tip.26"] = "TIPP: Geheimraeume verbergen sich hinter zerbrechlichen Waenden. Achte auf Risse!";
    m_de["tip.27"] = "TIPP: Parieren ist entscheidend — greife kurz bevor ein Gegner dich trifft an.";
    m_de["tip.28"] = "TIPP: Der Shop erscheint zwischen den Etagen. Spare Splitter fuer starke Upgrades!";
    m_de["tip.29"] = "TIPP: In Dimension B steigt Entropie schneller, aber Risse geben dort Bonus-Splitter.";
    m_de["tip.30"] = "TIPP: Die Phantom-Klasse kann durch Gegner hindurchdashen.";
    m_de["tip.31"] = "TIPP: Beobachte die Minikarte — rote Punkte sind Gegner, blaue sind Risse.";
    m_de["tip.32"] = "TIPP: Elite-Gegner haben farbige Auren die ihren Modifikator-Typ zeigen.";
    m_de["tip.33"] = "TIPP: Der Voidwalker heilt 3 HP bei jedem Dimensionswechsel.";
    m_de["tip.34"] = "TIPP: Herausforderungsmodi werden nach dem ersten abgeschlossenen Lauf freigeschaltet.";
    m_de["tip.35"] = "TIPP: Relikt-Synergien aktivieren sich bei passenden Kombinationen — pruefe das Pausenmenue!";
    m_de["tip.36"] = "TIPP: Rechtsklick in Menues bringt dich schnell zurueck.";
    m_de["tip.37"] = "TIPP: Boss-Angriffe aendern sich unter 33% HP im Enrage-Modus — bleib wachsam!";
    m_de["tip.38"] = "TIPP: Heilkugeln aus Kisten heilen mehr wenn dein HP kritisch niedrig ist.";
    m_de["tip.39"] = "TIPP: Waffenmeisterschaft bleibt zwischen Laeufen erhalten — meistere deine Favoriten!";
    m_de["tip.40"] = "TIPP: NPCs bieten Quests, Reparaturen und seltene Gegenstaende. Immer vorbeischauen!";
    m_de["tip.41"] = "TIPP: Verfluchte Relikte bieten riesige Kraftboosts mit gefaehrlichen Nachteilen.";
    m_de["tip.42"] = "TIPP: Dimension A ist sicherer fuer Rissreparatur, aber Dimension B gibt mehr Splitter.";
    m_de["tip.43"] = "TIPP: Ereignisketten erstrecken sich ueber mehrere Etagen — schliesse alle Stufen fuer Bonusbelohnungen ab!";
    m_de["tip.44"] = "TIPP: Die Entropiesense reduziert Anzug-Entropie bei Treffern — ideal fuer lange Laeufe.";
    m_de["tip.45"] = "TIPP: Halte die Fernkampf-Taste fuer einen starken Aufladeschuss!";
    m_de["tip.46"] = "TIPP: Der Enterhaken kann Gegner heranziehen oder an Waenden schwingen.";
    m_de["tip.47"] = "TIPP: Mini-Bosse sind staerkere Elites mitten im Level. Extra Belohnungen!";
    m_de["tip.48"] = "TIPP: Kill-Serien ohne Treffer zu kassieren multiplizieren deine Splitter.";
    m_de["tip.49"] = "TIPP: Die Riss-Schrotflinte feuert 5 Kugeln — verheerend auf kurze Distanz.";
    m_de["tip.50"] = "TIPP: Im Bestiarium findest du Gegnerschwaechen und effektive Waffen.";
    m_de["tip.51"] = "TIPP: Rissreparaturen in Dimension B sind riskanter, reduzieren aber mehr Entropie.";
    m_de["tip.52"] = "TIPP: Das Phoenixfeder-Relikt belebt dich einmal pro Lauf wieder — unbezahlbar gegen Bosse!";
    m_de["tip.53"] = "TIPP: Jede New Game+ Stufe schaltet einzigartige Modifikatoren UND Bonus-Splitter frei.";
    m_de["tip.54"] = "TIPP: Der Leerestrahl durchbohrt alle Gegner in einer Linie — ideal fuer Korridore.";
    m_de["tip.55"] = "TIPP: Dimensionspanzerung reduziert allen eingehenden Schaden um 10% pro Stufe.";
    m_de["tip.56"] = "TIPP: Die Wahrsagerin kann Geheimraeume auf der Karte enthuellen.";
    m_de["tip.57"] = "TIPP: Von oben auf Gegner stampfen verursacht Bonusschaden basierend auf Fallhoehe.";
    m_de["tip.58"] = "TIPP: Elementargegner nehmen Extraschaden von ihrem gegensaetzlichen Element.";
    m_de["tip.59"] = "TIPP: Der Schmied verstaerkt deine Waffen permanent bei hoeheren Story-Stufen.";
    m_de["tip.60"] = "TIPP: Entropieanker-Relikt senkt Entropiezunahme um 40% bei Bosskaempfen.";
    m_de["tip.61"] = "TIPP: Wandrutschen ermoeglicht verborgene Bereiche ueber normaler Sprunghoehe.";
    m_de["tip.62"] = "TIPP: Seelensauger-Relikt heilt 3 HP pro Kill — unverzichtbar fuer Ueberlebenslaeufe.";
    m_de["tip.63"] = "TIPP: Der Komboverstaerker erhoeht den Schaden waehrend Kombos.";
    m_de["tip.64"] = "TIPP: Tageslaeufe nutzen einen einzigartigen Seed — kaempfe um den hoechsten Ranglistenplatz!";
    m_de["tip.65"] = "TIPP: Zeitdehner-Relikt senkt Faehigkeits-Abklingzeiten um 30% — Kombos hoeren nie auf!";
    m_de["tip.66"] = "TIPP: Spiegelwachen wechseln ihren Schild alle 4 Sekunden. Timing ist alles!";
    m_de["tip.67"] = "TIPP: Die Riss-Armbrust durchbohrt mehrere Gegner — richte deine Schuesse aus!";
    m_de["tip.68"] = "TIPP: Beschworer sind allein zerbrechlich. Toete den Beschworer vor den Dienern!";
    m_de["tip.69"] = "TIPP: Im Pausenmenue findest du aktive Relikt-Synergien und aktuelle Build-Werte.";
    m_de["tip.70"] = "TIPP: Gravitationshandschuh zieht Gegner zu dir — ideal fuer Massenkontrolle!";
    m_de["tip.71"] = "TIPP: Coyote-Time erlaubt kurzes Springen nachdem du von einer Kante gelaufen bist.";
    m_de["tip.72"] = "TIPP: Angriffs-Pufferung reiht deinen naechsten Angriff waehrend der Abklingzeit ein.";
    m_de["tip.73"] = "TIPP: Die Kettenpeitsche hat die groesste Nahkampf-Reichweite und durchbohrt Gegner.";
    m_de["tip.74"] = "TIPP: Druecke M um die Minikarte waehrend des Spiels ein- oder auszublenden.";
    m_de["tip.75"] = "TIPP: Leerehunger-Relikt gibt +1% permanenten Schaden pro Kill — skaliert enorm!";
    m_de["tip.76"] = "TIPP: Chaoskugel-Relikt gewaehrt alle 30 Sekunden Zufallseffekte — nie langweilig!";
    m_de["tip.77"] = "TIPP: Phasendolche kritten jeden 5. Treffer — schnelle Waffen bedeuten mehr Krits!";
    m_de["tip.78"] = "TIPP: Entropieschwamm entfernt passive Entropie, aber Kills fuegen je 5% hinzu.";
    m_de["tip.79"] = "TIPP: F9 oder F12 macht jederzeit einen Screenshot — teile deine besten Momente!";

    // Input action names
    m_de["action.move_left"]       = "Links bewegen";
    m_de["action.move_right"]      = "Rechts bewegen";
    m_de["action.look_up"]         = "Nach oben schauen";
    m_de["action.look_down"]       = "Nach unten schauen";
    m_de["action.jump"]            = "Springen";
    m_de["action.dash"]            = "Dash";
    m_de["action.melee"]           = "Nahkampf-Angriff";
    m_de["action.ranged"]          = "Fernkampf-Angriff";
    m_de["action.dim_switch"]      = "Dimensionswechsel";
    m_de["action.interact"]        = "Interagieren";
    m_de["action.pause"]           = "Pause";
    m_de["action.slam"]            = "Bodenstampfer";
    m_de["action.shield"]          = "Rissschild";
    m_de["action.phase"]           = "Phasenschlag";

    // Lore fragment titles + texts (DE only — EN falls back to LoreSystem struct)
    m_de["lore.0.title"]  = "Der Riss";
    m_de["lore.0.text"]   = "Zwischen den Welten liegt eine Wunde in der Realitaet selbst. Der Riss reisst durch Dimensionen wie ein Fluss aus Leere und verbindet Orte, die sich nie beruehren sollten. Wer ihn betritt, kehrt selten unveraendert zurueck.";
    m_de["lore.1.title"]  = "Echos des Ursprungs";
    m_de["lore.1.text"]   = "Die Dimensionen waren einst eins. Ein Kataklysmus spaltete die Realitaet in Schichten, jede ein verzerrtes Spiegelbild der anderen. Die Grenzen werden mit jedem Zyklus duenner, und die Kreaturen dazwischen werden kuehner.";
    m_de["lore.2.title"]  = "Die Walker-Plage";
    m_de["lore.2.text"]   = "Walker sind die haeufigsten Bewohner des Risses. Geistlose Huellengetrieben von unstillbarem Hunger wandern sie endlos durch die gebrochenen Korridore. Manche sagen, sie waren einst Entdecker wie du.";
    m_de["lore.3.title"]  = "Leere-Hunger";
    m_de["lore.3.text"]   = "Entropie ist der Atem des Risses. Wenn sie steigt, zerfransen die Raender der Realitaet. Farben verlaufen, Geraeusche verzerren sich, und die Grenze zwischen Dimensionen wird hauchdünn. Bei hoher Entropie zu ueberleben heisst am Rand des Vergessens zu tanzen.";
    m_de["lore.4.title"]  = "Die Aufgestiegenen";
    m_de["lore.4.text"]   = "Wer sich im Riss bewaehrt, erhaelt den Aufstieg — die Chance, mit groesserer Macht neu zu beginnen. Jeder Aufstieg entfernt Schwaeche, doch der Riss reagiert gleichermassen und wird immer feindseliger.";
    m_de["lore.5.title"]  = "Vergessenes Handwerk";
    m_de["lore.5.text"]   = "Die Haendler des Risses sind raetselhafte Gestalten. Sie existieren ausserhalb des normalen Raums, ihre Staende erscheinen an Kreuzungen zwischen Dimensionen. Niemand weiss, was sie mit den gesammelten Splittern tun.";
    m_de["lore.6.title"]  = "Echo des Waechters";
    m_de["lore.6.text"]   = "Der erste Waechter, den du besiegtest, war lediglich eine Schildwache — ein Konstrukt des Risses, das Eindringlinge testen sollte. Seine Zerstoerung sandte Wellen durch jede verbundene Dimension.";
    m_de["lore.7.title"]  = "Des Wyrms Klage";
    m_de["lore.7.text"]   = "Der Leere-Wyrm war uralt jenseits aller Vorstellung, eine Kreatur geboren im Raum zwischen den Raeumen. Sein Todesschrei hallte durch alle Dimensionen und weckte Dinge, die lange geschlummert hatten.";
    m_de["lore.8.title"]  = "Des Architekten Entwurf";
    m_de["lore.8.text"]   = "Der Dimensionsarchitekt versuchte, die Realitaet nach seinem eigenen Bauplan umzugestalten. Seine Tuerme und Mauern waren Versuche, den Riss zu flicken — oder vielleicht weiter aufzureissen. Sein wahrer Zweck starb mit ihm.";
    m_de["lore.9.title"]  = "Des Webers Faden";
    m_de["lore.9.text"]   = "Der Zeitweber existierte in allen Momenten gleichzeitig. Ihn zu besiegen war keine Frage der Staerke, sondern das Brechen der Schleife, die er um sich gewoben hatte. Die Zeit fliesst nun frei — zum Guten oder Schlechten.";
    m_de["lore.10.title"] = "Des Souverains Wahrheit";
    m_de["lore.10.text"]  = "Der Leere-Souverain war kein Herrscher, sondern ein Gefangener. An die tiefste Schicht des Risses gekettet, versuchte er aeonenlang auszubrechen. Durch seine Niederlage hast du vielleicht etwas viel Schlimmeres befreit — oder ihn endlich erloest.";
    m_de["lore.11.title"] = "Letzte Offenbarung";
    m_de["lore.11.text"]  = "Du bist der Riss. Jeder Dimensionswechsel, jeder getoetete Feind, jeder abgeschlossene Zyklus — du hast nicht die Wunde in der Realitaet erkundet. Du warst die Wunde, die lernte, sich selbst zu heilen. Der Riss endet dort, wo du begonnen hast.";
    m_de["lore.12.title"] = "Natur des Schwarms";
    m_de["lore.12.text"]  = "Riss-Schwaermer sind keine Individuen, sondern Fragmente eines einzigen Schwarmbewusstseins, das zersplitterte als sich die Dimensionen trennten. Jedes Bernsteininsekt traegt einen Splitter kollektiver Erinnerung.";
    m_de["lore.13.title"] = "Gravitationsanomalien";
    m_de["lore.13.text"]  = "Wo das dimensionale Gewebe am duennsten ist, bilden sich spontan Gravitationsbrunnen — lebende Singularitaeten die sich von raeumlicher Verzerrung ernaehren. Alte Rissgelehrte nannten sie den Hunger der Leere in Gestalt.";
    m_de["lore.14.title"] = "Die Kunst der Taeuschung";
    m_de["lore.14.text"]  = "Mimiks waren einst gewoehliche Materie, umgeformt durch laengere Exposition mit instabiler Rissenergie. Sie lernten ihre Formen zu vertrauten Gestalten zu falten — Kisten, Truhen, Schutt. Die perfekte Tarnung in einer Dimension wo nichts sicher ist.";
    m_de["lore.15.title"] = "Entropie als Fleisch";
    m_de["lore.15.text"]  = "Entropie Inkarniert ist keine Kreatur sondern ein Konzept in Gestalt — der unvermeidliche Waermetod aller Dimensionen verdichtet in einem einzigen Wesen. Es zu besiegen zerstoert nicht die Entropie. Es verzoegert lediglich das Ende.";
    m_de["lore.16.title"] = "Dimensionstheorie";
    m_de["lore.16.text"]  = "Die Wahrsagerinnen des Risses besitzen ein seltenes Geschenk: sie koennen alle dimensionalen Schichten gleichzeitig wahrnehmen. Was als Prophezeiung erscheint, ist einfach das Sehen dessen was bereits in benachbarten Realitaeten existiert.";
    m_de["lore.17.title"] = "Riss-Oekologie";
    m_de["lore.17.text"]  = "Der Riss ist nicht nur eine Wunde — er ist ein Oekosystem. Walker weiden an dimensionaler Energie. Flieger jagen Walker. Bosse regulieren die Population. Selbst die Splitter die du sammelst sind Naehrstoffe in dieser fremden Nahrungskette.";
    m_de["lore.18.title"] = "Leere-Handel";
    m_de["lore.18.text"]  = "Die Leere-Haendler reisen zwischen allen bekannten Rissen und handeln mit Relikten gefallener Reisender. Ihre Wirtschaft basiert auf dimensionalen Splittern — kristallisierte Realitaet als universelle Waehrung aller zerbrochenen Welten.";
    m_de["lore.19.title"] = "Vermaechtnis der Vier";

    // Bestiary enemy names (DE only — EN falls back to BestiaryEntry struct)
    m_de["enemy.0.name"]  = "Risslaeufer";           // Walker
    m_de["enemy.1.name"]  = "Rissflieger";           // Flyer
    m_de["enemy.2.name"]  = "Leeregeschuetz";        // Turret
    m_de["enemy.3.name"]  = "Rissrammer";            // Charger
    m_de["enemy.4.name"]  = "Phasenpirsch";          // Phaser
    m_de["enemy.5.name"]  = "Leersprenger";          // Exploder
    m_de["enemy.6.name"]  = "Rissschild";            // Shielder
    m_de["enemy.7.name"]  = "Leerekrabbler";         // Crawler
    m_de["enemy.8.name"]  = "Rissbeschworer";        // Summoner
    m_de["enemy.9.name"]  = "Leerescharfschuetze";   // Sniper
    m_de["enemy.10.name"] = "Phasenblinker";         // Teleporter
    m_de["enemy.11.name"] = "Spiegelwache";          // Reflector
    m_de["enemy.12.name"] = "Leereegel";             // Leech
    m_de["enemy.13.name"] = "Rissschwaermer";        // Swarmer
    m_de["enemy.14.name"] = "Gravitationsbrunnen";   // GravityWell
    m_de["enemy.15.name"] = "Mimik";                 // Mimic

    // Bestiary weakness texts (DE only)
    m_de["enemy.0.weak"]  = "Langsame Bewegung, vorhersagbare Patrouille";
    m_de["enemy.1.weak"]  = "Wenig HP, verwundbar beim Sturzflug";
    m_de["enemy.2.weak"]  = "Kann sich nicht bewegen, Nahkampf-Reichweite nutzen";
    m_de["enemy.3.weak"]  = "Anlauf vor Angriff, verwundbar beim Stopp";
    m_de["enemy.4.weak"]  = "Verwundbar direkt nach dem Phasenwechsel";
    m_de["enemy.5.weak"]  = "Sehr wenig HP, ein Nahkampftreffer toetet";
    m_de["enemy.6.weak"]  = "Ungeschuetzt von hinten, Ruestung 0.35";
    m_de["enemy.7.weak"]  = "Decke beobachten, wenig HP";
    m_de["enemy.8.weak"]  = "Zerbrechlich wenn allein, erst Diener toeten";
    m_de["enemy.9.weak"]  = "Lange Vorwarnzeit, wenig HP";
    m_de["enemy.10.weak"] = "Vorhersagbarer 3s-Teleport-Zyklus, zerbrechlich";
    m_de["enemy.11.weak"] = "Schild alle 4s fuer 1s unten, von hinten angreifen";
    m_de["enemy.12.weak"] = "Langsam, wenig direkter Schaden, schnell toeten bevor er heilt";
    m_de["enemy.13.weak"] = "Wenig HP, einzeln leicht zu toeten";
    m_de["enemy.14.weak"] = "Langsame Bewegung, aus Distanz bekaempfbar";
    m_de["enemy.15.weak"] = "Vorhersagbar nach Enthuellen, hoher Schaden aber langsam";

    // Bestiary abilities (DE)
    m_de["enemy.0.abil"]  = "Nahkampf, patrouilliert Plattformen";
    m_de["enemy.1.abil"]  = "Sturzflug, Luftpatrouille, 250px Sturzgeschw.";
    m_de["enemy.2.abil"]  = "Fernkampf alle 1.8s, 300px Erkennung";
    m_de["enemy.3.abil"]  = "Sturmangriff 350px/s, Rueckstoss 350";
    m_de["enemy.4.abil"]  = "Phasenwechsel alle 3s, Nahkampf bei Annaeherung";
    m_de["enemy.5.abil"]  = "80px Explosionsradius, Feuer-Element, schneller Ansturm";
    m_de["enemy.6.abil"]  = "Frontalschild, 35% Ruestung, langsam aber zaeh";
    m_de["enemy.7.abil"]  = "Deckenklammer, Fallhinterhalt bei 400px/s";
    m_de["enemy.8.abil"]  = "Beschwort 3 Diener alle 6s, Blitz-Affinitaet";
    m_de["enemy.9.abil"]  = "400px Reichweite, 0.8s Vorwarnung, weicht bei Annaeherung";
    m_de["enemy.10.abil"] = "Teleportiert sich alle 3s hinter den Spieler";
    m_de["enemy.11.abil"] = "Frontalschild (4s aktiv / 1s Pause), 0.2 Ruestung";
    m_de["enemy.12.abil"] = "Entzieht 2HP/0.5s nah, heilt sich gleichviel. Schneller unter 50% HP";
    m_de["enemy.13.abil"] = "Schnelle Zickzack-Bewegung, erscheint in Gruppen, springt Spieler an";
    m_de["enemy.14.abil"] = "Gravitationsfeld zieht Spieler in 120px, schwebend, pulsierend";
    m_de["enemy.15.abil"] = "Als Kiste getarnt, Hinterhalt-Ausfallschritt (18 SCH, 300 Rueckstoss)";

    // Bestiary effective weapons (DE)
    m_de["enemy.0.eff"]   = "Jede Waffe. Nahkampf-Kombo-Abschluss betaeubt.";
    m_de["enemy.1.eff"]   = "Fernkampfwaffen. Eis verlangsamt Sturzflug.";
    m_de["enemy.2.eff"]   = "Nahkampf in Reichweite. Durch Geschosse dashen.";
    m_de["enemy.3.eff"]   = "Seitlich ausweichen, nach Sturm kontern.";
    m_de["enemy.4.eff"]   = "Dimension abgleichen. Nach Phasenwechsel angreifen.";
    m_de["enemy.5.eff"]   = "Aus Distanz schiessen. Eis friert fest.";
    m_de["enemy.6.eff"]   = "Von hinten angreifen. Blitzketten durchdringen Schild.";
    m_de["enemy.7.eff"]   = "Fernkampfwaffen wirken gut. Feuer raeumt Decken.";
    m_de["enemy.8.eff"]   = "Schadensschub auf Beschworer. Diener ignorieren wenn moeglich.";
    m_de["enemy.9.eff"]   = "Zwischen Schuessen dashen. Distanz schnell schliessen.";
    m_de["enemy.10.eff"]  = "Nach Teleport angreifen. Fernkampfwaffen.";
    m_de["enemy.11.eff"]  = "Auf Schild-Pause warten. Von hinten angreifen.";
    m_de["enemy.12.eff"]  = "Schadensschub zum schnellen Toeten. Distanz halten.";
    m_de["enemy.13.eff"]  = "Flaechenangriffe oder breite Nahkampfschlaege. Nicht umzingeln lassen.";
    m_de["enemy.14.eff"]  = "Fernkampf ausserhalb des Zugradius. Bei Zug wegdashen.";
    m_de["enemy.15.eff"]  = "Vorsichtig an Kisten herangehen. Ausfallschritt parieren.";

    // Boss bestiary details (index 16 = Boss type, entries 0-5 via boss index offset)
    // Boss entries use type Boss (16) but are stored separately — we use a naming convention
    // Bestiary lore texts (DE — shorter summaries)
    m_de["enemy.0.lore"]  = "Risslaeufer sind rudimentaere Konstrukte aus kristallisierter Dimensionsenergie. Sie haben keine echte Intelligenz, nur einen territorialen Instinkt. Entdecker berichten, stundenlang von diesen stummen Waechtern verfolgt zu werden.";
    m_de["enemy.1.lore"]  = "Wenn Rissenergie aufwaerts durch dimensionale Risse stroemt, verdichtet sie sich manchmal zu Luftraeuber. Sie beanspruchen den vertikalen Raum und stuermen auf alles herab, was von unten eindringt.";
    m_de["enemy.2.lore"]  = "Leeregeschuetze sind uralte automatisierte Wachposten einer unbekannten Zivilisation. Ihre kristallinen Kerne feuern komprimierte Rissbolzen mit mechanischer Praezision. Sie ermueden nie, verfehlen nie.";
    m_de["enemy.3.lore"]  = "Rissrammer waren einst langsame dimensionale Weidegaenger. Aber Mutationen durch instabile Rissenergie verwandelten ihren Fluchtinstinkt in blinden Angriffswahn. Wenn sie dich sehen, rasen sie los.";
    m_de["enemy.4.lore"]  = "Phasenpirsche existieren gleichzeitig in beiden Dimensionen, aber nur fuer Bruchteile. Sie lauern im Zwischenraum und warten auf den perfekten Moment zum Zuschlagen.";
    m_de["enemy.5.lore"]  = "Leersprenger sind instabile Konzentrationen dimensionaler Energie, gefangen in einem zerbrechlichen biologischen Koerper. Beim Tod setzt die ganze Energie explosiv frei.";
    m_de["enemy.6.lore"]  = "Rissschilder besitzen kristalline Frontalbarrieren aus verdichteter Dimensionsenergie. Die Schilder regenerieren sich nach kurzer Zeit. Nur Angriffe von hinten oder elementare Ketten durchdringen sie.";
    m_de["enemy.7.lore"]  = "Leerekrabbler haften an Decken und Waenden mithilfe dimensionaler Adheasion. Sie lauern geduldig und lassen sich mit hoher Geschwindigkeit auf ahnungslose Beute fallen.";
    m_de["enemy.8.lore"]  = "Rissbeschworer kanalisieren dimensionale Energie um kurzlebige Diener-Konstrukte zu erschaffen. Allein sind sie zerbrechlich, aber mit ihrem Gefolge sind sie bedrohlich.";
    m_de["enemy.9.lore"]  = "Leerescharfschuetzen halten maximale Distanz und feuern praezise Energiebolzen mit deutlicher Vorwarnung. Ihre Strategie: Distanz halten und bei Annaeherung zurueckweichen.";
    m_de["enemy.10.lore"] = "Phasenblinker nutzen instabile Rissenergie um sich kurzzeitig durch den Dimensionszwischenraum zu teleportieren. Sie erscheinen bevorzugt hinter dem Ziel.";
    m_de["enemy.11.lore"] = "Spiegelwachen tragen rotierende Kristallschilder die Geschosse reflektieren. Der Schild wechselt in einem vorhersagbaren Zyklus zwischen aktiv und inaktiv.";
    m_de["enemy.12.lore"] = "Leereegel naehren sich von Lebensenergie die sie bei Kontakt aus ihren Opfern saugen. Die entzogene Energie heilt den Egel. Unter 50% HP werden sie aggressiver.";
    m_de["enemy.13.lore"] = "Rissschwaermer sind Fragmente eines einzigen Schwarmbewusstseins. Einzeln schwach, aber in Gruppen ueberwaoeltigend. Ihre Zickzack-Bewegung macht sie schwer zu treffen.";
    m_de["enemy.14.lore"] = "Gravitationsbrunnen sind lebende Singularitaeten die sich von raeumlicher Verzerrung ernaehren. Ihr Gravitationsfeld zieht alles in Reichweite unaufhaltsam an.";
    m_de["enemy.15.lore"] = "Mimiks waren einst gewoehliche Materie, umgeformt durch Rissenergie. Sie tarnen sich als Kisten und Truhen und greifen erst an, wenn die Beute nah genug ist.";

    // Boss lore texts (DE — shorter summaries)
    m_de["boss.0.lore"]   = "Der Risswaechter ist aelter als jede Zivilisation die den Riss jemals durchquerte. Ein geduldiger, methodischer Waechter der Eindringlinge in Phasen testet, mit zunehmender Aggressivitaet.";
    m_de["boss.1.lore"]   = "Der Leere-Wyrm bewohnt die Raeume zwischen Dimensionen wo kein Licht existiert. Sein schlangenartiger Koerper kann teilweise zwischen Realitaeten phasen und ist nur angreifbar wenn er voll materialisiert.";
    m_de["boss.2.lore"]   = "Der Dimensionsarchitekt ist das einzige bekannte Wesen das aktiv Strukturen aus roher Dimensionsenergie konstruiert. Seine Tuerme und Mauern koennten den Riss heilen — oder weiter aufreissen.";
    m_de["boss.3.lore"]   = "Der Zeitweber existiert an einem festen Punkt in der Zeit und nimmt Vergangenheit und Zukunft gleichzeitig wahr. Ihn zu besiegen erfordert das Brechen der Zeitschleife die er um sich gewoben hat.";
    m_de["boss.4.lore"]   = "Der Leere-Souverain herrscht aus der tiefsten Schicht der Dimensionsleere. Gefangen zwischen Dimensionen versucht er seit Aeonen auszubrechen. Seine Niederlage koennte etwas Schlimmeres befreit haben.";
    m_de["boss.5.lore"]   = "Entropie Inkarniert ist kein Wesen sondern ein Konzept in Gestalt — der unvermeidliche Waermetod aller Dimensionen verdichtet in einer Entitaet. Es zu besiegen verzoegert nur das Ende.";

    // Boss bestiary names (for Bestiary list/detail)
    m_de["boss.0.bname"] = "Risswaechter";
    m_de["boss.1.bname"] = "Leere-Wyrm";
    m_de["boss.2.bname"] = "Dimensionsarchitekt";
    m_de["boss.3.bname"] = "Zeitweber";
    m_de["boss.4.bname"] = "Leere-Souverain";
    m_de["boss.5.bname"] = "Entropie Inkarniert";

    m_de["boss.0.weak"]  = "Phase-2-Enrage laesst kurze Oeffnung";
    m_de["boss.0.abil"]  = "Schild-Schub, Mehrfachschuss, Phasen-Sprung, 3 Phasen";
    m_de["boss.0.eff"]   = "Angriffsmuster lernen. Beim Schild-Schub dashen.";
    m_de["boss.1.weak"]  = "Verwundbar nach Sturzflug-Landung";
    m_de["boss.1.abil"]  = "Giftwolken, Sturzflug, Kugelregen, 3 Phasen";
    m_de["boss.1.eff"]   = "Gruenen Boden meiden. Nach Sturzflug kontern.";
    m_de["boss.2.weak"]  = "Schwach beim Strahl-Aufbau, Konstrukte zerstoeren";
    m_de["boss.2.abil"]  = "Kacheltausch, Risszonen, Konstruktstrahl, Arena-Kollaps";
    m_de["boss.2.eff"]   = "Konstrukte schnell zerstoeren. Dim-Wechsel gegen Risszonen.";
    m_de["boss.3.weak"]  = "Verwundbar beim Zeitstopp-Anlauf";
    m_de["boss.3.abil"]  = "Zeitverlangsamung, Uhrzeiger-Feger, Zeitruecklauf, Zeitstopp";
    m_de["boss.3.eff"]   = "Durch Verlangsamungszonen dashen. Feger-Timing lernen.";
    m_de["boss.4.weak"]  = "Auf Phasenuebergaenge achten, Leere-Sturm meiden";
    m_de["boss.4.abil"]  = "Leere-Kugeln, Riss-Schlag, Dim-Sperre, Laser-Feger, Leere-Sturm";
    m_de["boss.4.eff"]   = "Mobil bleiben. Dim-Wechsel gegen Dim-Sperre.";
    m_de["boss.5.weak"]  = "Entropie-Schübe vorhersagbar, kurze Immunitaetsfenster";
    m_de["boss.5.abil"]  = "Entropie-Entzug, Entropie-Sturm, Dim-Sperre, Phasen-Korruption";
    m_de["boss.5.eff"]   = "Anzug-Entropie sorgfaeltig managen. DoTs sofort reinigen.";
    m_de["lore.19.text"]  = "Voidwalker, Berserker, Phantom, Technomancer — vier Wege durch den Riss, jeder geboren aus einer anderen Philosophie des Ueberlebens. Der Voidwalker umarmt das Dazwischen. Der Berserker trotzt ihm. Das Phantom gleitet hindurch. Der Technomancer schreibt es um.";

    // Buff pickups
    m_de["pickup.shield"]          = "SCHILD";
    m_de["pickup.speed"]           = "TEMPO HOCH";
    m_de["pickup.damage"]          = "SCHADEN HOCH";

    // NG+ rank titles (run summary)
    m_de["ngplus.rank.1"]          = "NG+1 HERAUSFORDERER";
    m_de["ngplus.rank.2"]          = "NG+2 VETERAN";
    m_de["ngplus.rank.3"]          = "NG+3 GEHAERTET";
    m_de["ngplus.rank.4"]          = "NG+4 LEERE-BERUEHRT";
    m_de["ngplus.rank.5"]          = "NG+5 RISS-HERRSCHER";
    m_de["ngplus.rank.6"]          = "NG+6 ENTROPIE-WANDLER";
    m_de["ngplus.rank.7"]          = "NG+7 DIMENSIONSLORD";
    m_de["ngplus.rank.8"]          = "NG+8 REALITAETSBRECHER";
    m_de["ngplus.rank.9"]          = "NG+9 LEERE-AUFSTEIGER";
    m_de["ngplus.rank.10"]         = "NG+10 CHAOSMEISTER";

    // Run summary balance stats
    m_de["summary.balance_dmg"]    = "SCH %.2fx%s  |  ANG %.2fx%s";
    m_de["summary.balance_cap"]    = " (MAX)";
    m_de["summary.balance_cd"]     = "CD Etage %.0f%%  |  LeerRes %d  |  Zonen %d";
    m_de["summary.balance_hunger"] = "LeerHunger %.0f%% final (%.0f%% Spitze)";
    m_de["summary.info"]           = "%s  |  %s / %s  |  Diff %d  |  %d:%02d";

    // Event chain / zone transition
    m_de["hud.chain_stage"]        = "KETTE: Stufe %d/%d";
    m_de["hud.zone"]               = "ZONE %d";
    m_de["hud.xp_bar"]            = "Lv.%d  %d/%d";
    m_de["hud.hp_display"]         = "HP %.0f/%.0f";
    m_de["hud.entropy_display"]    = "ENTROPIE %.0f%%";
    m_de["hud.floor_boss"]         = "E%d / Zone %d  BOSS";
    m_de["hud.floor_miniboss"]     = "E%d / Zone %d  MINI-BOSS";
    m_de["hud.floor"]              = "E%d / Zone %d";
    m_de["hud.chain_reward"]       = "%s — +%d Splitter";
    m_de["hud.weapon_melee"]       = "NAHKAMPF";
    m_de["hud.weapon_ranged"]      = "FERNKAMPF";

    // NPC names
    m_de["npc.scholar"]            = "Rissgelehrter";
    m_de["npc.refugee"]            = "Dim.-Fluechtling";
    m_de["npc.engineer"]           = "Verlorener Ingenieur";
    m_de["npc.echo"]               = "Echo deiner Selbst";
    m_de["npc.blacksmith"]         = "Schmied";
    m_de["npc.fortune"]            = "Wahrsagerin";
    m_de["npc.merchant"]           = "Leere-Haendler";

    // NPC greetings (stage 0/1/2)
    m_de["npc.scholar.g0"]         = "Wissen ist Macht im Riss.";
    m_de["npc.scholar.g1"]         = "Willkommen zurueck! Ich habe etwas Entscheidendes gefunden...";
    m_de["npc.scholar.g2"]         = "Du wieder! Ich habe die letzten Texte entschluesselt.";
    m_de["npc.refugee.g0"]         = "Lass uns handeln...";
    m_de["npc.refugee.g1"]         = "Du bist zurueck! Ich habe Vorraete zum Teilen gefunden.";
    m_de["npc.refugee.g2"]         = "Mein Freund! Ich kann endlich deine Guete erwidern.";
    m_de["npc.engineer.g0"]        = "Ich kann das fuer dich reparieren.";
    m_de["npc.engineer.g1"]        = "Ich habe Rissenergie studiert. Bessere Ergebnisse!";
    m_de["npc.engineer.g2"]        = "Mein Meisterwerk ist fertig. Das geht aufs Haus.";
    m_de["npc.echo.g0"]            = "Stelle dich selbst!";
    m_de["npc.echo.g1"]            = "Staerker jetzt... Kannst du es wieder mit mir aufnehmen?";
    m_de["npc.echo.g2"]            = "Ein letzter Test. Beweise deine Meisterschaft.";
    m_de["npc.blacksmith.g0"]      = "Splitter befeuern meine Schmiede. Ich verbessere deine Ausruestung.";
    m_de["npc.blacksmith.g1"]      = "Deine Waffen haben Schlachten gesehen. Ich kann mehr tun.";
    m_de["npc.blacksmith.g2"]      = "Fuer dich, mein bestes Werk. Kostenlos.";
    m_de["npc.fortune.g0"]         = "Kreuz meine Hand mit Splittern, und ich zeige dir Geheimnisse.";
    m_de["npc.fortune.g1"]         = "Die Muster werden klarer. Ich sehe mehr.";
    m_de["npc.fortune.g2"]         = "Die Risse haben mir alles gezeigt. Mein letztes Geschenk...";
    m_de["npc.merchant.g0"]        = "Dimensionale Artefakte, frisch aus der Leere.";
    m_de["npc.merchant.g1"]        = "Ah, du kehrst zurueck! Neuer Bestand eingetroffen.";
    m_de["npc.merchant.g2"]        = "Fuer einen geschaetzten Kunden, mein seltenster Bestand.";

    // NPC dialog options
    m_de["npc.opt.leave"]          = "[Gehen]";
    m_de["npc.opt.fight"]          = "[Kaempfen!]";
    m_de["npc.opt.not_yet"]        = "[Noch nicht...]";
    m_de["npc.opt.browse"]         = "[Waren ansehen]";
    m_de["npc.opt.read_fortune"]   = "[Zukunft lesen]";

    // NPC action options (Scholar)
    m_de["npc.scholar.s2.opt"]     = "[Wahrheit erfahren (+40 Splitter, Heilung)]";
    m_de["npc.scholar.s1.opt1"]    = "[Zuhoeren (+25 Splitter, -Entropie)]";
    m_de["npc.scholar.s1.opt2"]    = "[Nach dem Souveraen fragen]";
    m_de["npc.scholar.s0.opt1"]    = "[Tipp anhoeren (+15 Splitter)]";
    m_de["npc.scholar.s0.opt2"]    = "[Nach Gegnern fragen]";
    m_de["npc.scholar.quest"]      = "[Quest annehmen: 10 Kreaturen jagen]";
    // NPC action options (Refugee)
    m_de["npc.refugee.s2.opt"]     = "[Geschenk annehmen (gratis Relikt)]";
    m_de["npc.refugee.s1.opt1"]    = "[Gratis-Heilung (+30 HP)]";
    m_de["npc.refugee.s1.opt2"]    = "[15 HP tauschen fuer 60 Splitter]";
    m_de["npc.refugee.s0.opt1"]    = "[20 HP tauschen fuer 50 Splitter]";
    m_de["npc.refugee.s0.opt2"]    = "[30 HP tauschen fuer 80 Splitter]";
    // NPC action options (Engineer)
    m_de["npc.engineer.s2.opt"]    = "[Permanentes Upgrade (+25% SCH)]";
    m_de["npc.engineer.s1.opt1"]   = "[Waffe verbessern (+40% SCH, 60s)]";
    m_de["npc.engineer.s1.opt2"]   = "[Angriffe tunen (+20% Tempo, 45s)]";
    m_de["npc.engineer.s0.opt"]    = "[Waffe verbessern (+30% SCH, 45s)]";
    m_de["npc.engineer.quest"]     = "[Quest annehmen: 3 Risse reparieren]";
    // NPC action options (Blacksmith)
    m_de["npc.smith.s2.opt"]       = "[Meisterwerk annehmen (gratis +30% beide)]";
    m_de["npc.smith.s1.opt1"]      = "[Nahkampf schaerfen +25% SCH (35 Splitter)]";
    m_de["npc.smith.s1.opt2"]      = "[Fernkampf verstaerken +25% SCH (35 Splitter)]";
    m_de["npc.smith.s1.opt3"]      = "[Tempo schleifen +15% ANG (45 Splitter)]";
    m_de["npc.smith.s0.opt1"]      = "[Nahkampf schaerfen +20% SCH (40 Splitter)]";
    m_de["npc.smith.s0.opt2"]      = "[Fernkampf verstaerken +20% SCH (40 Splitter)]";
    // NPC action options (Fortune Teller)
    m_de["npc.fortune.s2.opt1"]    = "[Alle Geheimnisse enthuellen (gratis)]";
    m_de["npc.fortune.s2.opt2"]    = "[Boss-Vorhersage (+20% SCH vs Boss)]";
    m_de["npc.fortune.s1.opt1"]    = "[Geheimraeume zeigen (20 Splitter)]";
    m_de["npc.fortune.s1.opt2"]    = "[Hinterhalte zeigen (15 Splitter)]";
    m_de["npc.fortune.s0.opt"]     = "[Geheimnisse enthuellen (30 Splitter)]";
    // NPC action options (Void Merchant)
    m_de["npc.merch.s2.opt1"]      = "[Legendaeres Relikt kaufen (80 Splitter)]";
    m_de["npc.merch.s2.opt2"]      = "[Zufaelliges Relikt kaufen (40 Splitter)]";
    m_de["npc.merch.s1.opt1"]      = "[Relikt kaufen (60 Splitter)]";
    m_de["npc.merch.s1.opt2"]      = "[Zufaelliges Relikt kaufen (35 Splitter)]";
    m_de["npc.merch.s0.opt"]       = "[Relikt kaufen (50 Splitter)]";

    // NPC quest offers & progress
    m_de["npc.quest.scholar"]      = "Ich brauche Daten ueber die Risskreaturen. Besiege 10 Gegner\nund ich belohne dich mit Risssplittern.";
    m_de["npc.quest.engineer"]     = "Hilf mir die Risssensoren zu kalibrieren. Repariere 3 Risse\nauf dieser Etage und ich bezahle dich — plus Anzugstabilisierung.";
    m_de["npc.quest.fortune"]      = "Die Leere verbirgt viele Geheimnisse. Finde 2 Geheimraeume\nauf dieser Etage, und ich teile eine maechtige Vision.";
    m_de["npc.qprog.scholar"]      = "Jage weiter diese Kreaturen. Ich spuere mehr Daten einfliessen...";
    m_de["npc.qprog.engineer"]     = "Die Sensoren erfassen deine Reparaturen. Weiter so!";
    m_de["npc.qprog.fortune"]      = "Ich spuere, du hast verborgene Orte gefunden... suche weiter.";

    // NPC story lines (7 NPCs × 3 stages)
    m_de["npc.scholar.d0"]  = "Die Dimensionsrisse werden instabiler. Jede Reparatur\nschwaeecht den Griff des Leere-Souverains auf die Realitaet.";
    m_de["npc.scholar.d1"]  = "Alte Texte enthuellen: die Risse sind nicht natuerlich.\nJemand — oder etwas — hat die Dimensionen zerrissen.";
    m_de["npc.scholar.d2"]  = "Die Wahrheit: Der Souverain war einst ein Waechter. Die Risse\nsind die Dimensionen die sich selbst heilen. Du bist der Katalysator.";
    m_de["npc.refugee.d0"]  = "Ich bin zwischen den Dimensionen gefangen. Tausche etwas Gesundheit\nund ich gebe dir Risssplitter die ich gesammelt habe.";
    m_de["npc.refugee.d1"]  = "Ich fand medizinische Vorraete zwischen den Dimensionen.\nLass mich dich versorgen — und ich habe ein besseres Angebot.";
    m_de["npc.refugee.d2"]  = "Du hast mir so lange geholfen zu ueberleben. Ich sammelte\nRisskomponenten — nimm dieses Geschenk, ohne Hintergedanken.";
    m_de["npc.engineer.d0"] = "Lass mich deine Waffe mit Rissharmonien neu kalibrieren.\nSollte den Schadensausstoß fuer eine Weile steigern.";
    m_de["npc.engineer.d1"] = "Rissenergie verstaerkt Waffen mehr als ich dachte!\nLaengere Wirkung, plus ich kann dein Angriffstempo steigern.";
    m_de["npc.engineer.d2"] = "Ich hab es geschafft! Rissgestaertes Metall verbindet sich\npermanent mit Waffen. Dieses Upgrade haelt den ganzen Lauf.";
    m_de["npc.echo.d0"]     = "Ein Spiegelkampf beginnt! Besiege dein Echo fuer eine Belohnung.";
    m_de["npc.echo.d1"]     = "Dein Echo ist jetzt staerker, angepasst vom letzten Mal.";
    m_de["npc.echo.d2"]     = "Dein ultimatives Echo. Besiege es fuer die groesste Belohnung.";
    m_de["npc.smith.d0"]    = "Meine Schmiede brennt mit Rissenergie. Ich kann deinen\nNahkampf schaerfen oder Fernkampf verstaerken — fuer einen Preis.";
    m_de["npc.smith.d1"]    = "Mit mehr Rissenergie kann ich jetzt auch Reichweite\nund Angriffstempo verbessern. Bessere Preise fuer Stammkunden.";
    m_de["npc.smith.d2"]    = "Ich habe meine Riss-Schmiedetechnik perfektioniert.\nNimm mein Meisterwerk — beide Waffen, permanent verstaerkt.";
    m_de["npc.fortune.d0"]  = "Ich spuere verborgene Kammern in der Naehe. Fuer ein kleines\nOpfer enthulle ich ihre Positionen auf deiner Karte.";
    m_de["npc.fortune.d1"]  = "Die dimensionalen Faeden sind leichter zu lesen.\nIch kann verborgene Raeume und Hinterhalte voraussagen.";
    m_de["npc.fortune.d2"]  = "Meine Visionen waren nie klarer. Ich kann jedes\nGeheimnis auf dieser Etage enthuellen — und Voraussicht gegen den Boss.";
    m_de["npc.merchant.d0"] = "Jedes dieser Relikte wurde aus der Leere zwischen\nden Dimensionen gerissen. Grosse Macht — zu einem fairen Preis.";
    m_de["npc.merchant.d1"] = "Ich habe meine Sammlung erweitert. Diese Relikte tragen\nstaerkere dimensionale Resonanz — jeden Splitter wert.";
    m_de["npc.merchant.d2"] = "Mein bestes Stueck: ein Relikt aus dem Tresor des Souverains\nselbst. Normalerweise unbezahlbar — aber fuer dich, ein Sonderpreis.";

    // Random events
    m_de["event.merchant"]         = "Haendler";
    m_de["event.anomaly"]          = "Anomalie";
    m_de["event.rift_echo"]        = "Riss-Echo";
    m_de["event.repair_station"]   = "Reparaturstation";
    m_de["event.gambling"]         = "Gluecksriss";
    m_de["event.shrine_power"]     = "Schrein der Kraft";
    m_de["event.shrine_vitality"]  = "Schrein der Vitalitaet";
    m_de["event.shrine_speed"]     = "Schrein der Schnelligkeit";
    m_de["event.shrine_entropy"]   = "Schrein der Entropie";
    m_de["event.shrine_shards"]    = "Schrein der Splitter";

    // Event chains
    m_de["chain.merchant_quest"]   = "Haendlerquest";
    m_de["chain.dim_tear"]         = "Dimensionsriss";
    m_de["chain.entropy_surge"]    = "Entropie-Welle";
    m_de["chain.lost_cache"]       = "Verlorener Schatz";
    m_de["npc.nav_hint"]           = "[W/S] Navigieren  [Enter] Waehlen  [Esc] Schliessen";

    // Combat challenges
    m_de["cc.aerial.name"]         = "Luft-Kill";
    m_de["cc.aerial.desc"]         = "Toete einen Gegner in der Luft";
    m_de["cc.multi.name"]          = "Dreifach-Kill";
    m_de["cc.multi.desc"]          = "Toete 3 Gegner innerhalb von 4 Sekunden";
    m_de["cc.dash.name"]           = "Dash-Toeter";
    m_de["cc.dash.desc"]           = "Toete einen Gegner per Dash-Angriff";
    m_de["cc.combo.name"]          = "Kombo-Abschluss";
    m_de["cc.combo.desc"]          = "Toete mit dem 3. Kombo-Treffer";
    m_de["cc.charged.name"]        = "Schwerer Treffer";
    m_de["cc.charged.desc"]        = "Toete mit einem Auflade-Angriff";
    m_de["cc.nodmg.name"]          = "Unberuehrbar";
    m_de["cc.nodmg.desc"]          = "Saeuber Welle ohne Schaden zu nehmen";

    // Dynamic events
    m_de["event.dim_storm"]        = "DIMENSIONSSTURM";
    m_de["event.elite_invasion"]   = "ELITE-INVASION";
    m_de["event.time_dilation"]    = "ZEITDEHNUNG";

    // Bestiary locked stat labels
    m_de["bestiary.boss_label"]    = "[ BOSS ]";
    m_de["bestiary.hp_locked"]     = "HP:  ???";
    m_de["bestiary.dmg_locked"]    = "SCH: ???";
    m_de["bestiary.spd_locked"]    = "GES: ???";
    m_de["bestiary.elem_locked"]   = "ELEM: ???";

    // Combat floating text
    m_de["hud.parry"]              = "PARIERT!";
    m_de["hud.crit"]               = "KRIT! %.0f";
    m_de["hud.level_display"]      = "Lv.%d";
    m_de["hud.challenge_complete"] = "HERAUSFORDERUNG GESCHAFFT! +%d Splitter";
    m_de["hud.rifts_counter"]      = "Risse: %d / %d  [A:%d B:%d]";

    // Bestiary stat labels
    m_de["bestiary.stat_hp"]       = "HP:  %.0f";
    m_de["bestiary.stat_dmg"]      = "SCH: %.0f";
    m_de["bestiary.stat_spd"]      = "GES: %.0f";
    m_de["bestiary.stat_spd_na"]   = "GES: ---";
    m_de["bestiary.stat_elem"]     = "ELEM: %s";
}
