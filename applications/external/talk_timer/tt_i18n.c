#include "tt_i18n.h"

/* Aktuelle Sprache; die App läuft nur einmal, daher genügt eine Modulvariable */
static TtLang tt_lang_current = TtLangEn;

void tt_lang_set(TtLang lang) {
    if(lang < TtLangCount) tt_lang_current = lang;
}

const char* tt_tr(const char* en, const char* de) {
    return (tt_lang_current == TtLangDe) ? de : en;
}

const char* tt_lang_name(TtLang lang) {
    return (lang == TtLangDe) ? "Deutsch" : "English";
}
