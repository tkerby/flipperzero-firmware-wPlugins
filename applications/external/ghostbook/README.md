# 👻 GhostBook

### Your contacts. Encrypted. Tap to share.

---

## The Problem

You meet someone at a con, a meetup, a party. They want your contact info.

You could:
- Hand over your phone (awkward)
- Spell out your Discord (they'll typo it)
- Write it on paper (they'll lose it)

**There has to be a better way.**

---

## The Solution

**Tap. Share. Done.**

GhostBook turns your Flipper Zero into an encrypted digital business card.

```
┌────────────────────────┐
│    >> GhostBook <<     │
│    ================    │
│    @yourhandle (o_o)   │
│    ----------------    │
│    Name: Ghost         │
│    Discord: ghost#1337 │
│                        │
│    [Hold to share]     │
└────────────────────────┘
```

---

## How It Works

**Share:** Your Flipper emulates an NFC tag containing your encrypted card. Hold it near another Flipper running GhostBook.

**Receive:** Scan for nearby cards. One tap, saved to contacts.

**Protect:** Wrong passcode too many times? Everything melts.

---

## Security That Bites Back

| Feature | What It Does |
|---------|--------------|
| 6-10 Button Passcode | 46K to 60M combinations |
| Auto-Wipe | 3/5/7/10 wrong attempts = gone |
| 256-bit Encryption | Military-grade protection |
| 10K Iterations | Brute force? Good luck. |
| No Cloud | Your data never leaves your device |

### The Wipe

Enter the wrong passcode too many times and watch your ghost melt:

```
!! I'M MELTING !!        !! MELTING... !!        !! GOODBYE !!

    (o_o)                    (x_-)                   . . .
    /| |\        →          ~~~~~        →
     | |
```

All data destroyed. No recovery. By design.

---

## What You Can Store

- **@Handle** — Your username
- **Name** — Real name (optional)
- **Email** — Contact email
- **Discord** — Your tag
- **Signal** — Phone number
- **Telegram** — Username
- **Notes** — Whatever you want
- **Flair** — ASCII art icon

---

## Quick Start

1. Install `ghostbook.fap` on your Flipper
2. Choose security level (passcode length)
3. Choose wipe threshold (attempts allowed)
4. Create your passcode
5. Fill in your profile
6. Start sharing

---

## For The Security-Minded

**On-device protection:**
- Passcode never stored (only hash)
- Wipe happens on the Flipper itself
- No way to extract passcode from files

**Offline attack resistance:**
- Unique 16-byte salt per device
- 10,000 hash iterations
- Random IV per encryption

**Threat model:**
- ✅ Casual snooping
- ✅ Lost/stolen device
- ✅ Nosy friends
- ⚠️ Determined attacker with your SD card
- ❌ Nation-state actors (use Signal)

---

## Why GhostBook?

| | GhostBook | Business Card | Phone |
|-|:---------:|:-------------:|:-----:|
| Encrypted | ✅ | ❌ | ❌ |
| Auto-wipe | ✅ | ❌ | ❌ |
| Tap to share | ✅ | ❌ | Maybe |
| Works offline | ✅ | ✅ | ❌ |
| Hacker cred | ✅✅✅ | ❌ | ❌ |

---

## Get It

**Download:** [Releases](https://github.com/digitard/ghostbook/releases)

**Build:**
```bash
git clone https://github.com/digitard/ghostbook
cd ghostbook && ufbt && ufbt launch
```

---

## Version 0.6.0

- ✅ NFC tap-to-share (NTAG215 emulation)
- ✅ NFC tap-to-receive
- ✅ Variable passcode (6-10 buttons)
- ✅ Configurable wipe threshold
- ✅ Melting ghost animation
- ✅ Encrypted storage
- ✅ vCard/CSV export

---

**Built by Digi** — [@digitard](https://github.com/digitard)

*Trust no one. Leave nothing.*

```
(o_o)
```
