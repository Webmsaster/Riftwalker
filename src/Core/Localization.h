#pragma once
#include <string>
#include <unordered_map>

enum class Lang { EN, DE };

class Localization {
public:
    static Localization& instance();
    void setLanguage(Lang lang);
    Lang getLanguage() const { return m_lang; }
    const char* get(const char* key) const;

private:
    Localization() { loadStrings(); }
    Lang m_lang = Lang::EN;
    std::unordered_map<std::string, std::string> m_en;
    std::unordered_map<std::string, std::string> m_de;
    void loadStrings();
};

// Shorthand macro for localized strings
#define LOC(key) Localization::instance().get(key)
