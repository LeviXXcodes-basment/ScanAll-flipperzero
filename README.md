<!--
  NOTE: the badge/links below use the repo path  LeviXXcodes-basment/ScanAll-flipperzero
  If your repo path is different, do a find-and-replace on that string.
-->

# ScanAll

### All-in-one NFC & RFID reader for the Flipper Zero

Read &bull; Identify &bull; Save

[![Build FAP](https://github.com/LeviXXcodes-basment/ScanAll-flipperzero/actions/workflows/build.yml/badge.svg)](https://github.com/LeviXXcodes-basment/ScanAll-flipperzero/actions/workflows/build.yml)
![Firmware](https://img.shields.io/badge/firmware-Momentum%20MNTM--012-orange)
![Category](https://img.shields.io/badge/category-NFC-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Made by LeviXX](https://img.shields.io/badge/made%20by-LeviXX-black)

---

**ScanAll** reads **both** of the Flipper Zero's contactless frequencies from a
single place — **13.56 MHz NFC** cards and **125 kHz LF RFID** tags. It tells
you what you're holding, shows the useful details on screen, and saves the
result to your SD card so the stock apps can use it later.

Built as an external `.fap` with [ufbt](https://pypi.org/project/ufbt/).
Developed and tested on **Momentum firmware (MNTM-012)**.

---

## ✨ Why this app?

The Flipper ships with separate NFC and 125 kHz RFID apps. That's great — until
you pick up an unknown card or fob and don't know which technology it uses, and
end up hopping between two apps to find out. **ScanAll** puts both readers
behind one menu, adds a short built-in guide on how to tell NFC and RFID apart,
and keeps the on-screen info clean. It's meant to be the *first* app you open to
answer "what is this thing, and can I save it?"

---

## 🔧 Features

### NFC Scan (13.56 MHz)
- Reads **card type** (from SAK), **UID**, **ATQA**, **SAK**
- Options (RIGHT arrow on a hit):
  - **Save** — custom name, or empty = date/UID based name
  - **Details** — scrollable info page (protocol, type, UID, ATQA, SAK, UID length)
  - **Replace** — overwrite an existing `.nfc` file
  - **Save to new dir** — pick the folder with the file browser
- Saves to **`/ext/nfc`** → visible in the stock NFC app

### RFID Scan (125 kHz LF)
- Reads the **protocol** (EM4100, HID, Indala, AWID, …) and the **tag data**
- Options (RIGHT arrow on a hit):
  - **Details** — protocol, manufacturer, data size, full rendered data
  - **Save** — custom name, or empty = date based name
  - **Replace** — overwrite an existing `.rfid` file
  - **Save to new dir** — pick the folder
- Saves to **`/ext/lfrfid`**

### Extras
- **"NFC or RFID?"** — a built-in guide on how to tell which one your card is
- **Three-state read feedback:** *Trying to read…* → *Reading…* → *Done reading!*
- **LED feedback:** blue while scanning, green + buzz on success
- **No accidental rescans:** opening Options and going back keeps the current read
- Clean English UI with on-screen hints (`< Back`, `Options >`)

---

## 🗺️ Menu map

```
Main menu
├── NFC Scan ............ read a 13.56 MHz card → Options (Save / Details / Replace / Save to new dir)
├── RFID Scan (125 kHz) . read a 125 kHz LF tag → Options (Details / Save / Replace / Save to new dir)
├── NFC or RFID? ........ guide: which one is my card?
└── About ............... version + info
```

---

## 📦 Install

### Option 1 — download a prebuilt `.fap`
Grab the latest `.fap` from the [**Releases**](../../releases) page and copy it
to your Flipper's SD card under `apps/NFC/` (e.g. with qFlipper). Every tagged
version is built automatically by CI.

### Option 2 — build it yourself
```bash
pip install --upgrade ufbt
ufbt            # or: make
ufbt launch     # build + upload + run on a connected Flipper
```
Or copy the built file from `dist/` to `apps/NFC/` on the SD card.

> **Firmware note:** developed on Momentum (MNTM-012). On other firmware the NFC
> and LF RFID APIs may differ slightly; a failed build is usually a one-line
> API-name adjustment.

---

## 📍 On the Flipper

After installing: **Apps → NFC → ScanAll**.

---

## ⚠️ Limitations

- The NFC **Details** page shows **UID-level** info only. Reading the actual
  Mifare **block/sector data** (key-protected, physically on the card) is **not**
  done here — use the stock NFC app for a full dump.
- ScanAll is a **read + save** utility: no writing to cards, no emulation.

---

## 🗂️ Project layout

```
.
├── .github/workflows/build.yml   # CI: builds the .fap, uploads artifact, releases on tag
├── application.fam               # app manifest (name, category, requirements, icon)
├── scanall_app.c                 # app source
├── images/
│   └── scanall_icon.png          # 10x10 1-bit app icon (fob + card)
├── Makefile                      # ufbt convenience wrapper
├── CONTRIBUTING.md
├── README.md
├── LICENSE                       # MIT
└── .gitignore
```

---

## 🤝 Contributing

Issues and PRs welcome — see [CONTRIBUTING.md](CONTRIBUTING.md). For build
errors, include your firmware version and the compiler output.

## 📄 License

[MIT](LICENSE).

## 👤 Credits

Made by **LeviXX**. Built on the Flipper Zero firmware's public NFC and LF RFID APIs.

   -Use with own risk
