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
    m_en["pause.floor"]            = "Floor: %d";
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
    m_de["pause.floor"]            = "Etage: %d";
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
}
