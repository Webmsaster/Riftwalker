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

    // Class Select
    m_en["class.title"]            = "C H O O S E  Y O U R  C L A S S";
    m_en["class.locked"]           = "LOCKED";
    m_en["class.nav_hint"]         = "A/D Switch  |  ENTER Select  |  ESC Back";
    m_en["class.hp"]               = "HP: %.0f";
    m_en["class.speed"]            = "Speed: %.0f";

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

    // Class Select
    m_de["class.title"]            = "W A E H L E  D E I N E  K L A S S E";
    m_de["class.locked"]           = "GESPERRT";
    m_de["class.nav_hint"]         = "A/D Wechseln  |  ENTER Auswaehlen  |  ESC Zurueck";
    m_de["class.hp"]               = "HP: %.0f";
    m_de["class.speed"]            = "Tempo: %.0f";

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
}
