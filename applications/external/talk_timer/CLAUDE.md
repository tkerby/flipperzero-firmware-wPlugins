# talk_timer – Flipper Zero App (Präsentations-Klicker mit Redezeit-Timer)

Native Flipper-Zero-App (.fap, C, ufbt): schaltet per USB- oder Bluetooth-HID
Folien weiter und erinnert per Vibration diskret an die Redezeit.

Referenzprojekt (nur lesen, NIE verändern):
`../zoom_remote` (GitHub: Mableton/zoom_remote).
Transport-Schicht, Settings, i18n und Menüaufbau von dort übernehmen.

## Feste Regeln

### Umgebung und Build
- Ziel: Flipper Zero mit Momentum Firmware **mntm-012**.
- ufbt baut gegen das Momentum-SDK:
  `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json`
- Nach jedem Arbeitsschritt mit `ufbt` bauen, Fehler selbst beheben, per
  `ufbt launch` deployen (qFlipper ist dabei geschlossen), dann sagen, was am
  Gerät zu testen ist.
- Solange die App im USB-HID-Modus ist, gibt es keinen seriellen Port:
  `ufbt launch` und qFlipper gehen dann nicht. Für Screenshots die App über
  Bluetooth verbinden, dann bleibt qFlipper per USB verbunden.
- Nur ein Mac vorhanden, kein Windows-Gerät. Windows NIE als getestet ausgeben.
- App-ID: `talk_timer`, Name "Talk Timer", 10x10-Icon, `fap_category="Tools"`.

### Funktion
- Startmenü: Verbindung (USB / Bluetooth), Präsentationsprogramm-Profil,
  Redezeit. Sprache Englisch (Standard) / Deutsch umschaltbar. Letzte Auswahl
  unter `/ext/apps_data/talk_timer/` merken.
- Profile: PowerPoint, Keynote, Google Slides, Generisch/PDF. Weiter/Zurück
  überall; Zusatzaktionen je Profil: Präsentation starten, schwarzer
  Bildschirm, Präsentation beenden.
- Timer: Redezeit in Minuten, Start/Pause/Reset. Warnungen einstellbar
  (Standard: 5 min vor Ende, 1 min vor Ende, Ende), jede mit eigenem, klar
  unterscheidbarem Vibrationsmuster. Kein Ton als Standard. Nach Ablauf zählt
  der Timer die Überziehung hoch und zeigt das deutlich an.
- Hauptscreen: große Restzeit, Fortschrittsbalken, Status (läuft / pausiert /
  überzogen), Verbindungs-Icon, Tastenleiste im Flipper-Stil.
- Option "Display anlassen", solange der Timer läuft.
- Schutz vor Fehlbedienung: App/Hauptscreen verlassen nur per langem Druck
  auf Zurück, bei gestartetem Timer zusätzlich mit Bestätigungsdialog.
- Presets für Redezeiten speicherbar.

### Tastenkürzel
- Quelle ausschließlich die offiziellen Hilfeseiten von Microsoft (PowerPoint
  Windows und Mac), Apple (Keynote) und Google (Slides). Nichts aus dem
  Gedächtnis. Quelle und Abrufdatum im Code nennen.
- Kürzel als Datentabelle in `tt_profiles.c`, nicht in der Logik verstreut.
- Deutsche Tastatur (QWERTZ): layoutunabhängige Tasten bevorzugen (Pfeile,
  Bild auf/ab, F5, Escape, Enter, Punkt). HID-Codes sind positionsbasiert;
  macOS-Apps werten Kürzel teils nach Position statt Zeichen aus. Buchstaben-
  Kürzel am Mac mit der echten App prüfen, nicht annehmen.

### Technik und bekannte Fallen
- Vorlage für USB-/Bluetooth-HID: HID-App im Momentum-Repo
  (`applications/system/hid_app`, Tag mntm-012). Beim Beenden USB-Modus und
  Bluetooth-Profil exakt wie dort zurücksetzen (USB-Konfig merken und
  wiederherstellen, BT-Keys-Pfad zurücksetzen, `bt_profile_restore_default`).
- `furi_hal_hid_set_state_callback` NICHT verwenden (läuft im USB-Interrupt,
  GUI-Events von dort lösen furi_check aus). Verbindungsstatus per
  Tick-Callback mit `furi_hal_hid_is_connected()` abfragen.
- `dialog_ex_reset()` löscht auch Callback und Kontext, danach neu setzen.
- Keine Firmware-Icons aus `assets_icons.h` (gibt es nur unter Momentum).
  Eigene 1-Bit-PNGs im Ordner `images/`.
- Nur APIs verwenden, die es auch im offiziellen SDK gibt.
- Timer mit FuriTimer oder Tick-Callback, Zeitmessung über `furi_get_tick()`,
  keine blockierenden Wartezeiten im GUI-Thread. Vibration über
  Notification-Sequenzen.
- Alles Allokierte beim Beenden freigeben. App schreibt nur in ihr eigenes
  Datenverzeichnis.
- Umlaute: Firmware-Schriften sind ASCII-only (u8g2 `_tr`). In Firmware-Modulen
  (Menüs, DialogEx, elements_button_*) deutsche Wörter OHNE Umlaute wählen
  ("Fertig" statt "Zurück"), KEINE Umschreibung wie "ue". Im eigenen
  Hauptscreen echte Umlaute über `tt_draw_utf8()` (Grundbuchstabe + Punkte).
  Deutsche Texte auf Rechtschreibung und Sinn prüfen.
- ViewDispatcher, Submenu/VariableItemList, eigene View für den Hauptscreen.
- Code-Kommentare auf Deutsch. Oberfläche zweisprachig über
  `tt_tr("English", "Deutsch")` (`tt_i18n.*`), Standard Englisch.

### Veröffentlichung (erst wenn die App am Gerät funktioniert)
- NICHTS öffentlich machen, posten oder einreichen ohne ausdrückliches OK.
- GitHub-Repo unter Mableton, zuerst privat, nach OK öffentlich. Lizenz
  GPL-3.0, englische README, Release mit zwei Builds: Momentum mntm-012 und
  offizielle Firmware (letzterer in abgeschotteter Umgebung mit eigenem
  `UFBT_HOME`; das Momentum-Setup unter `~/.ufbt` nicht anfassen).
- Catalog-Material in `.catalog/`: Beschreibung nur mit erlaubtem Markdown
  (H1/H2, fett, kursiv, Listen, Links), Changelog im Format `v1.0:`,
  Screenshots exakt 512x256 in den Farben (254,138,44) und (0,0,0).
  Dazu AGENTS.md und documentation/ im Catalog-Repo lesen.
- Manifest mit `python3 tools/bundle.py --nolint ...` validieren, bevor ein
  Pull Request entsteht.
- Pull Request im Catalog nur nach ausdrücklichem OK. KI-Offenlegung ehrlich
  als "Fully AI generated" mit Erklärung je Datei; Testangaben ehrlich (was
  am Gerät lief, was nur kompiliert wurde).

### Vorgehen
1. Plan mit Dateistruktur, Bildschirm-Skizze und Tastenbelegung zeigen, auf
   OK warten.
2. Schrittweise: Gerüst mit Menüs und Settings -> Timer mit Anzeige und
   Vibration -> USB-HID Folientasten -> Profile -> Bluetooth -> Feinschliff.
3. Nach jedem Schritt bauen, Fehler beheben, deployen, Testhinweise geben.

## Teststand (ehrlich halten)
- Am Gerät getestet wird nur **Keynote am Mac**. Alle anderen Profile
  (PowerPoint Win/Mac, Google Slides Win/Mac, Generisch/PDF) sind nach
  Hersteller-Doku umgesetzt und gelten als ungetestet (Kommentar "Teststand" in
  `tt_profiles.c`, so auch in README und Catalog-PR angeben). Keynote ist per
  USB und Bluetooth am Mac bestätigt (2026-09-17).

## Tastenbelegung Hauptscreen

| Taste          | kurz                               | lang                                  |
|----------------|------------------------------------|---------------------------------------|
| Rechts / Links | Folie weiter / zurück (beim Drücken, genau 1 Tastendruck, kein Repeat) | – |
| OK             | Timer Start / Pause; mit "Autostart" (Standard An) startet der erste Start auch die Präsentation | Reset (nur wenn Timer nicht läuft) |
| Hoch           | Schwarzer Bildschirm               | Präsentation starten (ab akt. Folie)  |
| Runter         | Anzeige Restzeit <-> abgelaufen    | Präsentation beenden (Esc)            |
| Zurück         | nur Hinweis "Hold Back to exit"    | Verlassen; bei gestartetem Timer Dialog |

Vibration: Warnung 1 = 1x lang, Warnung 2 = 2x lang, Ende = 3x sehr lang.

## Dateistruktur

```
application.fam        Manifest
talk_timer.png         10x10 App-Icon
talk_timer.h/.c        App-Struktur, ViewDispatcher, Navigation, Menüs, Einstieg
tt_timer.h/.c          Timerlogik (furi_get_tick-basiert, ohne GUI)
tt_alerts.h/.c         Warnschwellen-Tabellen + Vibrationssequenzen
tt_profiles.h/.c       Kürzel-Datentabelle mit Quellen und Abrufdatum
tt_transport.h/.c      USB-/BLE-HID, Init/Teardown wie HID-App
tt_settings.h/.c       Einstellungen + Presets (FlipperFormat)
tt_i18n.h/.c           Sprachumschaltung Englisch/Deutsch
views/tt_main_view.*   Hauptscreen
images/                eigene 1-Bit-Icons
.catalog/              Catalog-Material
```

## Bedienregeln (vom Nutzer verlangt, vor jedem Deploy akribisch prüfen)
- Richtung: Links = weniger/kleiner/aus, Rechts = mehr/größer/an. Alle
  Auswahltabellen AUFSTEIGEND sortieren (VariableItemList: Links = Index-1).
- Angezeigter Wert = aktiver Zustand (z.B. "Language: English", wenn die App
  englisch ist), nie das Umschaltziel.
- Werte in der VariableItemList kurz halten (Wertebereich nur ca. 36 px; die
  offizielle Firmware clippt nicht). Lange Auswahl -> eigenes Submenu (Profile).
- Pfeil links führt aus Untermenüs heraus (Presets und Profile immer; Einstellungen in der
  Zeile "< Back", weil Links in Wert-Zeilen "weniger" heißt). Umsetzung:
  ViewStack mit unsichtbarer View, die "Links kurz" abfängt.
- Presets speichern Redezeit UND Warnzeiten; Preset wählen setzt beides.

## Selbsttest am Mac (ohne Knopfdruck am Gerät)
- Für die Fehlersuche gab es vorübergehend einen Selbsttest (App sendet nach Öffnen
  des Hauptscreens automatisch Aktionen, schreibt `debug.log`). Vor der
  Veröffentlichung ENTFERNT (Catalog: keine Debug-Reste). Bei Bedarf neu bauen.
- Fernbedienen per CLI (`input send ok press|short|release`) über pyserial aus der
  ufbt-Toolchain; Keynote-Zustand per `osascript` (`playing`, `slide number`);
  Tastatureingang am Mac über `ioreg -c IOHIDSystem` (HIDIdleTime).
- Befund 2026-09-17: Keynote 15.3.1 Start/Weiter/Zurück/Ende OK. Opt+Cmd+P ist
  ein UMSCHALTER (beendet laufende Präsentation) -> `show_started` beachten.

## API-Fallen (offizielles SDK)
- `variable_item_list_set_header`, `variable_item_set_locked`,
  `variable_item_set_item_label`, `variable_item_list_get` gibt es NUR unter
  Momentum -> nicht verwenden. Abgleich gegen
  `targets/f7/api_symbols.csv` im offiziellen Firmware-Repo (Branch release).
- Menüs nicht direkt aus Modul-Callbacks umbauen (laufen teils unter dem
  Modell-Lock), sondern per Custom-Event im ViewDispatcher.
- DEADLOCK-FALLE: Der Enter-Callback der VariableItemList läuft INNERHALB von
  `with_view_model`. Dort nie USB/Bluetooth starten oder stoppen (bt_profile_start
  wartet auf den BT-Dienst, der auf die GUI, die GUI auf die Listen-Sperre ->
  App hängt). Hauptscreen deshalb nur über `TtCustomEventEnterMain` /
  `TtCustomEventLeaveMain` öffnen/verlassen. (Submenu-Callbacks laufen ohne
  Sperre, deshalb trat das in zoom_remote nie auf.)

## Stand
- 2026-09-17: VERÖFFENTLICHT (jeweils mit OK des Nutzers): Repo
  https://github.com/Mableton/talk_timer öffentlich, Release v1.0 mit
  `talk_timer_momentum-mntm-012.fap` und `talk_timer_official-1.4.3.fap`,
  Catalog-PR https://github.com/flipperdevices/flipper-application-catalog/pull/1241
  (Commit 346a420, mit `bundle.py --nolint` validiert). Hinweis: bundle.py braucht eine
  venv OHNE SCons (Python 3.11 der ufbt-Toolchain, `SSL_CERT_FILE` auf certifi setzen).
- 2026-09-17: USB und Bluetooth mit Keynote am Mac vom Nutzer bestätigt. Feinschliff
  (ungenutzter Code entfernt, `ufbt format`), Profil-Auswahlmenü statt abgeschnittener
  Werte, Screenshots umgerechnet (512x256, Catalog-Palette), Build gegen offizielle
  Firmware 1.4.3 in `~/.ufbt-official` fehlerfrei.
- 2026-09-17: Plan freigegeben (6 flache Profile, Exit-Schutz "lang + Dialog").
- Bluetooth: Deadlock beim Start behoben (2026-09-17), Start/Verlassen per CLI
  geprüft; Kopplung mit dem Mac muss der Nutzer testen.
- Schritte 2+3 (Timer, Anzeige, Vibration, Exit-Dialog, USB-HID-Tasten; BLE-Code
  aus zoom_remote schon enthalten, aber noch ungetestet) gebaut und deployt.
- Schritt 1 (Gerüst, Menüs, Settings, Presets, Sprache) gebaut und deployt;
  nach Nutzer-Feedback überarbeitet (Richtung, Sprache, Links=Zurück, Presets mit Warnzeiten).
