# FlipDeck → Flipper App Catalog: Schritt-für-Schritt

## Voraussetzungen

- GitHub Account
- App baut erfolgreich mit `ufbt build`
- Flipper Zero mit aktueller Release-Firmware

---

## Phase 1: Source Repo vorbereiten

### 1.1 GitHub Repo erstellen

```bash
# Neues Repo auf GitHub: github.com/DEIN_USERNAME/flipdeck
# Dann lokal:
cd flipdeck/flipper    # NUR den Flipper-Teil als Repo!
git init
git remote add origin git@github.com:DEIN_USERNAME/flipdeck.git
```

> **Wichtig:** Das App Catalog will ein Repo, das direkt `application.fam`
> im Root oder im App-Ordner hat. Am besten: das gesamte Projekt als Repo
> (mit `flipper/`, `host/`, etc.) und in `manifest.yml` den Pfad angeben.

### 1.2 Pflicht-Dateien prüfen

Das Repo braucht mindestens:

```
flipdeck/
├── flipper/
│   ├── application.fam     ✅ App-Manifest (appid, name, version, category)
│   ├── flipdeck.c          ✅ Source code
│   └── icon.png            ✅ 10×10px, 1-bit, PNG
├── screenshots/            ✅ qFlipper-Screenshots (nicht skaliert!)
│   ├── main.png
│   ├── media.png
│   └── custom.png
├── README.md               ✅ Beschreibung, Anleitung, Features
├── CHANGELOG.md            ✅ Versionshistorie
└── LICENSE                 ✅ Open Source Lizenz (MIT, GPL, etc.)
```

### 1.3 `application.fam` checken

Folgende Felder MÜSSEN gesetzt sein:

```python
App(
    appid="flipdeck",              # Unique! Lowercase, underscores OK
    name="FlipDeck",               # Anzeigename im Catalog
    apptype=FlipperAppType.EXTERNAL,
    entry_point="flipdeck_main",
    stack_size=4 * 1024,
    fap_icon="icon.png",           # 10×10px 1-bit PNG
    fap_category="USB",            # Catalog-Kategorie
    fap_description="USB HID Macro Pad with configurable pages and media keys",
    fap_author="DeinName",
    fap_version="3.0",             # MUSS bei jedem Update erhöht werden!
    fap_weburl="https://github.com/DEIN_USERNAME/flipdeck",
    requires=["gui", "storage", "notification"],
)
```

### 1.4 Screenshots erstellen

```
1. Flipper mit USB an Mac/PC anschließen
2. qFlipper öffnen → Screenshot-Button (📷 oben rechts)
3. Screenshots NICHT skalieren/bearbeiten!
4. Das erste Screenshot wird als Vorschaubild im Catalog verwendet
```

Empfohlene Screenshots:
- `main.png` – Media-Seite (Hauptansicht)
- `media.png` – In Benutzung (Button gedrückt)
- `custom.png` – Custom-Seite

### 1.5 CHANGELOG.md Format

```markdown
## 3.0
- Multi-page support with named headers
- F13-F24 custom key support
- Page navigation with swipe and dots
- Haptic and visual button feedback

## 2.0
- USB HID approach (replaced serial)
- Native media keys

## 1.0
- Initial release
```

### 1.6 Build verifizieren

```bash
cd flipper
ufbt update --channel=release    # Aktuellstes SDK
ufbt build                       # MUSS fehlerfrei durchlaufen!
```

### 1.7 Committen & pushen

```bash
git add -A
git commit -m "FlipDeck v3.0 - Multi-page USB HID Macro Pad"
git push origin main
```

**Commit-SHA notieren!** (brauchst du für `manifest.yml`)

```bash
git log -1 --format="%H"
# → z.B. a1b2c3d4e5f6...
```

---

## Phase 2: App Catalog Manifest erstellen

### 2.1 Flipper App Catalog Repo forken

```
1. Gehe zu: https://github.com/flipperdevices/flipper-application-catalog
2. Klick "Fork" (oben rechts)
3. Clone deinen Fork:
```

```bash
git clone git@github.com:DEIN_USERNAME/flipper-application-catalog.git
cd flipper-application-catalog
```

### 2.2 Branch erstellen

```bash
git checkout -b DEIN_USERNAME/flipdeck_3.0
```

### 2.3 manifest.yml erstellen

```bash
mkdir -p applications/USB/flipdeck
```

Erstelle `applications/USB/flipdeck/manifest.yml`:

```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/DEIN_USERNAME/flipdeck
    commit_sha: "DEIN_COMMIT_SHA_HIER"   # ← Aus Schritt 1.7!

short_description: "USB HID Macro Pad – media keys & custom shortcuts for Flipper Zero"

description: README.md

changelog: CHANGELOG.md

screenshots:
  - screenshots/main.png
  - screenshots/media.png
  - screenshots/custom.png
```

### 2.4 Manifest validieren

```bash
# Im flipper-application-catalog Repo:
python3 -m pip install -r requirements.txt
python3 -m tools.manifest_check applications/USB/flipdeck/manifest.yml
```

### 2.5 Optional: README im Catalog-Repo

Du kannst eine zusätzliche `README.md` in `applications/USB/flipdeck/` ablegen.
Diese wird angezeigt wenn jemand den Manifest-Link im Catalog anklickt.

---

## Phase 3: Pull Request erstellen

### 3.1 Committen & pushen

```bash
git add applications/USB/flipdeck/manifest.yml
git commit -m "Add FlipDeck v3.0 - USB HID Macro Pad"
git push origin DEIN_USERNAME/flipdeck_3.0
```

### 3.2 Pull Request öffnen

```
1. Gehe zu deinem Fork auf GitHub
2. Klick "Compare & pull request"
3. Base: flipperdevices/flipper-application-catalog : main
4. Fülle das PR-Template aus
5. Submit!
```

### 3.3 Review abwarten

- Automatische Checks laufen (Build, Manifest-Validierung)
- Flipper-Team reviewed innerhalb 1-2 Werktagen
- Bei Fehlern: GitHub-Notifications checken, Fixes committen
- Nach Merge: App erscheint in wenigen Minuten im Catalog!

---

## Phase 4: Updates veröffentlichen

Bei jeder neuen Version:

```bash
# 1. Im Source-Repo:
#    - Version in application.fam erhöhen (z.B. "3.1")
#    - CHANGELOG.md aktualisieren
#    - Committen & pushen
#    - Neue commit_sha notieren

# 2. Im Catalog-Fork:
#    - manifest.yml: commit_sha aktualisieren
#    - Neuen Branch + PR erstellen
```

---

## Checkliste vor dem PR

- [ ] `ufbt build` läuft fehlerfrei mit Release-Firmware
- [ ] `application.fam` hat unique `appid`, korrekte `fap_version`
- [ ] `icon.png` ist 10×10px, 1-bit Farbtiefe
- [ ] Screenshots sind original qFlipper-Resolution
- [ ] `README.md` erklärt Features und Benutzung
- [ ] `CHANGELOG.md` existiert mit Versionshistorie
- [ ] Lizenz-Datei vorhanden (MIT, GPL, etc.)
- [ ] `manifest.yml` hat korrekten `commit_sha`
- [ ] Manifest validiert (`manifest_check`)
- [ ] Kein Schadcode, keine illegalen Features
- [ ] Open Source Lizenz erlaubt Binary-Distribution
