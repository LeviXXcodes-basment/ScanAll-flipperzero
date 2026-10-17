 ScanAll — Flipper Zero NFC + RFID Reader

**ScanAll** is a lightweight, all-in-one scanner app for the Flipper Zero that
reads **both** of the Flipper's contactless frequencies from a single place:
**13.56 MHz NFC** cards and **125 kHz LF RFID** tags. It identifies what you're
holding, shows the useful details on screen, and lets you save the result to
your SD card so the stock apps can use it later.

Built as an external `.fap` with [ufbt](https://pypi.org/project/ufbt/).
Developed and tested on **Momentum firmware (MNTM-012)**.

> Made by **LeviXX**.

---

## Why this app?

The Flipper already ships with separate NFC and 125 kHz RFID apps, which is
great — but if you just picked up an unknown card or fob and you're not sure
which technology it uses, you end up bouncing between two apps to find out.
**ScanAll** puts both readers behind one menu, adds a short built-in guide that
explains how to tell NFC and RFID apart, and keeps the on-screen info clean and
to the point. It's meant to be the *first* app you open when you want to answer
"what is this thing, and can I save it?"

---

## Features

### NFC Scan (13.56 MHz)
- Reads and displays:
  - **Card type** (guessed from the SAK byte — e.g. Mifare Classic 1K/4K,
    Mifare Ultralight / NTAG, ISO14443-4 compatible, etc.)
  - **UID** (hex)
  - **ATQA** and **SAK**
- Press the **RIGHT arrow** on a successful read to open the **Options** menu:
  - **Save** — type a custom file name, or leave it empty to auto-name it from
    the date and UID
  - **Details** — a scrollable info page (protocol, type, UID, ATQA, SAK, UID
    length)
  - **Replace** — pick an existing `.nfc` file in the browser and overwrite its
    contents with the card you just read
  - **Save to new dir** — choose any folder with the file browser
- Files are saved to **`/ext/nfc`**, so they show up in the stock NFC app too.

### RFID Scan (125 kHz LF)
- Reads and displays:
  - **Protocol** (EM4100, HID, Indala, AWID, and the other LF protocols the
    firmware supports)
  - **Tag data** (brief rendered value)
- Press the **RIGHT arrow** on a successful read to open the **Options** menu:
  - **Details** — protocol, manufacturer, data size, and the full rendered data
  - **Save** — custom name or auto date-based name
  - **Replace** — overwrite an existing `.rfid` file
  - **Save to new dir** — choose any folder
- Files are saved to **`/ext/lfrfid`**.

### "NFC or RFID?" — built-in guide
A dedicated page that walks you through how to figure out which technology your
card or tag uses:
1. Try **NFC Scan** first. If it reads (shows a UID), it's a 13.56 MHz NFC card
   (Mifare, NTAG, bank cards, most phones, newer access cards).
2. If NFC finds nothing, try **RFID Scan**. If *that* reads, it's a 125 kHz LF
   tag (EM4100, HID, Indala — older building-access fobs, pet chips, many plain
   white cards).

Plus a quick rule of thumb: thin bank-style / phone tap → usually NFC; thick
fob / old office badge → often RFID. Some cards are dual, in which case both
scans find something.

### Quality-of-life
- **Three-state read feedback:** *Trying to read…* → *Reading…* → *Done
  reading!* (the RFID side shows all three; NFC shows trying → done, since the
  UID read is effectively instantaneous).
- **LED feedback:** blinks **blue** while scanning, turns **green** and buzzes
  on a successful read.
- **No accidental rescans:** going into an Options menu and back keeps the
  current read on screen — you only rescan when you explicitly start a new scan
  from the main menu.
- **Clean, English UI** with on-screen navigation hints (`< Back`,
  `Options >`).
- **Version shown** in the header and the About page.

---

## Screens / menu map

Main menu
├── NFC Scan ............ read a 13.56 MHz card → Options (Save/Details/Replace/Save to new dir)
├── RFID Scan (125 kHz) . read a 125 kHz LF tag → Options (Details/Save/Replace/Save to new dir)
├── NFC or RFID? ........ guide: which one is my card?
└── About ............... version + info


---

## Install

### Option 1 — build it yourself (recommended)
1. Install ufbt:

pip install --upgrade ufbt

2. In the project folder (the one containing `application.fam`):

ufbt

   This downloads the matching SDK and builds the app.
3. With the Flipper connected over USB:

ufbt launch

   This uploads and launches it. Alternatively, copy the built `.fap` from
   `dist/` to your SD card under `apps/NFC/` using qFlipper.

### Option 2 — prebuilt .fap
If a release `.fap` is attached under **Releases**, download it and drop it in
`apps/NFC/` on your Flipper's SD card.

> **Firmware note:** developed on Momentum (MNTM-012). On other firmware the NFC
> and LF RFID APIs may differ slightly; if a build fails, it's usually a small
> API-name mismatch to adjust.

---

## Where you'll find it on the Flipper

After installing: **Apps → NFC → ScanAll**.

---

## Limitations (read me)

- The NFC **Details** page shows **UID-level** info only. Reading the actual
  Mifare **block/sector data** (which is protected by keys and physically lives
  on the card) is **not** done here — use the stock NFC app for a full dump.
- ScanAll is a **read + save** utility. It does **not** write to cards and does
  **not** emulate them.
- LF sector-style data isn't a thing; for RFID the "Details" page is the
  equivalent deep view (full protocol data).

---

## Project layout

.
├── application.fam # app manifest (name, category, requirements, icon)
├── nfc_info_app.c # app source
├── images/
│ └── scanall_icon.png # 10x10 1-bit app icon (fob + card)
├── README.md
├── LICENSE # MIT
└── .gitignore # ignores ufbt/fbt build output


---

## Contributing

Issues and pull requests are welcome. If you hit a build error on a specific
firmware, please include the firmware name/version and the compiler output —
most fixes are one-line API-name adjustments.

---

## License

Released under the **MIT License** — see [`LICENSE`](LICENSE).

## Credits

Made by **LeviXX**.
Built on the Flipper Zero firmware's public NFC and LF RFID APIs.
