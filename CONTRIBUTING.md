# Contributing to ScanAll

Thanks for your interest in improving ScanAll! Contributions of all kinds are
welcome — bug reports, fixes, and features.

## Reporting a bug

Open an issue and include:

- Your **firmware name and version** (e.g. Momentum MNTM-012, official 1.x, …).
- What you did and what happened vs. what you expected.
- If it's a build error: the **full compiler output**. Most build failures are
  one-line API-name mismatches between firmware forks.
- If it's a crash: ideally the Flipper CLI log around the crash. Connect at
  `lab.flipper.net` → CLI, run `log debug`, reproduce, and copy the lines near
  the error.

## Building locally

```
pip install --upgrade ufbt
ufbt            # or: make
ufbt launch     # build + upload + run on a connected Flipper
```

The source is a single file, `scanall_app.c`, plus the manifest
`application.fam` and the icon under `images/`.

## Pull requests

- Keep changes focused and described in the PR body.
- Make sure `ufbt` builds cleanly before opening the PR — CI will also build it.
- Match the existing code style (4-space indent, snake_case, the firmware's
  `furi_*` / `nfc_*` / `lfrfid_*` APIs).

## Scope

ScanAll is a **read + save** utility for NFC and LF RFID. It intentionally does
**not** write to cards or emulate them, and it does not attempt full Mifare
sector dumps (use the stock NFC app for that). Feature PRs that stay within this
scope — better protocol display, nicer UI, more save options — are the most
likely to be merged.
