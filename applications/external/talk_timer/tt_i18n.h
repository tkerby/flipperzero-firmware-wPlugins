#pragma once

/*
 * Zweisprachige Oberfläche (Englisch als Standard, Deutsch umschaltbar).
 * Texte stehen direkt an der Verwendungsstelle: tt_tr("English", "Deutsch").
 * Anzeigetexte nur ASCII (keine Umlaute).
 */
typedef enum {
    TtLangEn,
    TtLangDe,
    TtLangCount,
} TtLang;

void tt_lang_set(TtLang lang);

/* Liefert den Text in der aktuell gewählten Sprache */
const char* tt_tr(const char* en, const char* de);

/* Eigenname der Sprache für das Einstellungsmenü ("English" / "Deutsch") */
const char* tt_lang_name(TtLang lang);
