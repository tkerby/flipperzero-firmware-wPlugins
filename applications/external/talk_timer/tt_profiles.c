#include "tt_profiles.h"

#include <furi.h>
#include <furi_hal_usb_hid.h>

/*
 * Tastenkürzel-Tabelle. Quellen (alle abgerufen am 2026-09-17), nichts aus dem
 * Gedächtnis:
 *
 * [MS]  Microsoft Support, "Use keyboard shortcuts to deliver PowerPoint
 *       presentations", Abschnitte Windows und macOS
 *       https://support.microsoft.com/en-us/office/use-keyboard-shortcuts-to-deliver-powerpoint-presentations-1524ffce-bd2a-45f4-9a7f-f18b992b93a0
 *       Windows: Weiter = N, Enter, Page down, Right arrow, Down arrow, Spacebar;
 *                Zurück = P, Page up, Left arrow, Up arrow, Backspace;
 *                ab aktueller Folie = Shift+F5; schwarz = B oder Period (.);
 *                Ende = Esc
 *       macOS:   Weiter = N, Page down, Right arrow, Down arrow, Spacebar;
 *                Zurück = P, Page up, Left arrow, Up arrow, Delete;
 *                ab aktueller Folie = Cmd+Return; schwarz = B, Shift+B, Period (.);
 *                Ende = Esc, Hyphen (-), Cmd+Period
 *
 * [APL] Apple Support, "Keyboard shortcuts for Keynote on Mac" (Keynote 15.3),
 *       Abschnitt "Play a presentation and use the presenter mode"
 *       https://support.apple.com/guide/keynote/keyboard-shortcuts-tanfde4a3e6d/mac
 *       Play a presentation = Option-Command-P; Advance = Right Arrow or Down
 *       Arrow; Go to previous slide = Left Arrow or Up Arrow; Pause and show a
 *       black screen = B; Quit presentation mode = Esc or Q
 *
 * [GGL] Google Docs Editors Help, "Keyboard shortcuts for Google Slides",
 *       Abschnitte "PC shortcuts" und "Mac shortcuts"
 *       https://support.google.com/docs/answer/1696717
 *       PC:  Present slides = Ctrl+F5; Next = Right arrow; Previous = Left arrow;
 *            blank black slide = b oder . (zurück: beliebige Taste); Stop = Esc
 *       Mac: Present slides = Cmd+Enter; sonst wie PC
 *
 * Tastaturlayout: HID-Codes sind positionsbasiert. Verwendet werden nur Tasten,
 * die auf QWERTZ (DE) und QWERTY (US) an derselben Position dasselbe Zeichen
 * liefern: Pfeile, Bild auf/ab, F5, Return, Escape, Punkt, B und P. Y/Z und
 * Sonderzeichen-Positionen werden bewusst gemieden; deshalb "." statt "B", wo
 * der Hersteller beides anbietet. Keynote bietet für Schwarz nur B an.
 *
 * "Generisch/PDF" hat keine Herstellerquelle: Bild ab/auf ist die übliche
 * Belegung von Klicker-Hardware und funktioniert in gängigen PDF-Betrachtern;
 * Start und Schwarz gibt es dort nicht einheitlich, daher leer.
 *
 * Teststand Keynote (2026-09-17, Keynote 15.3.1, macOS, deutsche Tastatur, USB):
 * Start, Weiter, Zurück und Ende per Selbsttest gegen Keynotes AppleScript-Status
 * ("playing", "slide number") bestätigt; Schwarz (B) und die Bedienung am Gerät
 * vom Nutzer bestätigt. ACHTUNG: Opt+Cmd+P ist ein Umschalter,
 * bei laufender Präsentation beendet es sie (daher show_started in talk_timer.c).
 *
 * Teststand allgemein: Nur Keynote wird am Mac mit der echten App geprüft. Alle anderen
 * Profile sind nach Hersteller-Doku umgesetzt und UNGETESTET. Bluetooth wurde
 * ebenfalls mit Keynote am Mac geprüft (2026-09-17).
 */
static const TtProfile tt_profiles[TtProfileCount] = {
    [TtProfilePptWin] =
        {
            .name = "PowerPoint Win",
            .short_name = "PPT Win",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_RIGHT_ARROW, /* [MS] Right arrow key */
                    [TtActionPrev] = HID_KEYBOARD_LEFT_ARROW, /* [MS] Left arrow key */
                    [TtActionStart] = HID_KEYBOARD_F5 | KEY_MOD_LEFT_SHIFT, /* [MS] Shift+F5 */
                    [TtActionBlack] = HID_KEYBOARD_DOT, /* [MS] Period (.) */
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE, /* [MS] Esc */
                },
        },
    [TtProfilePptMac] =
        {
            .name = "PowerPoint Mac",
            .short_name = "PPT Mac",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_RIGHT_ARROW, /* [MS] Right arrow key */
                    [TtActionPrev] = HID_KEYBOARD_LEFT_ARROW, /* [MS] Left arrow key */
                    [TtActionStart] = HID_KEYBOARD_RETURN | KEY_MOD_LEFT_GUI, /* [MS] Cmd+Return */
                    [TtActionBlack] = HID_KEYBOARD_DOT, /* [MS] Period (.) */
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE, /* [MS] Esc */
                },
        },
    /* Einziges am echten Programm geprüftes Profil (siehe Teststand oben) */
    [TtProfileKeynote] =
        {
            .name = "Keynote",
            .short_name = "Keynote",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_RIGHT_ARROW, /* [APL] Right Arrow */
                    [TtActionPrev] = HID_KEYBOARD_LEFT_ARROW, /* [APL] Left Arrow */
                    /* [APL] Option-Command-P */
                    [TtActionStart] = HID_KEYBOARD_P | KEY_MOD_LEFT_ALT | KEY_MOD_LEFT_GUI,
                    [TtActionBlack] = HID_KEYBOARD_B, /* [APL] B */
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE, /* [APL] Esc */
                },
        },
    [TtProfileSlidesWin] =
        {
            .name = "Google Slides Win",
            .short_name = "Slides Win",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_RIGHT_ARROW, /* [GGL] Right arrow */
                    [TtActionPrev] = HID_KEYBOARD_LEFT_ARROW, /* [GGL] Left arrow */
                    [TtActionStart] = HID_KEYBOARD_F5 | KEY_MOD_LEFT_CTRL, /* [GGL] Ctrl+F5 */
                    [TtActionBlack] = HID_KEYBOARD_DOT, /* [GGL] . */
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE, /* [GGL] Esc */
                },
        },
    [TtProfileSlidesMac] =
        {
            .name = "Google Slides Mac",
            .short_name = "Slides Mac",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_RIGHT_ARROW, /* [GGL] Right arrow */
                    [TtActionPrev] = HID_KEYBOARD_LEFT_ARROW, /* [GGL] Left arrow */
                    [TtActionStart] = HID_KEYBOARD_RETURN | KEY_MOD_LEFT_GUI, /* [GGL] Cmd+Enter */
                    [TtActionBlack] = HID_KEYBOARD_DOT, /* [GGL] . */
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE, /* [GGL] Esc */
                },
        },
    [TtProfileGeneric] =
        {
            .name = "Generic / PDF",
            .short_name = "Generic",
            .keys =
                {
                    [TtActionNext] = HID_KEYBOARD_PAGE_DOWN, /* Klicker-Konvention */
                    [TtActionPrev] = HID_KEYBOARD_PAGE_UP, /* Klicker-Konvention */
                    [TtActionStart] = HID_KEYBOARD_NONE,
                    [TtActionBlack] = HID_KEYBOARD_NONE,
                    [TtActionEnd] = HID_KEYBOARD_ESCAPE,
                },
        },
};

const TtProfile* tt_profile_get(TtProfileId id) {
    furi_assert(id < TtProfileCount);
    return &tt_profiles[id];
}

uint16_t tt_profile_key(TtProfileId id, TtAction action) {
    furi_assert(id < TtProfileCount);
    furi_assert(action < TtActionCount);
    return tt_profiles[id].keys[action];
}
