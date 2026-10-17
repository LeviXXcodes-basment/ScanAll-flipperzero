# GitHub feltöltés — lépésről lépésre (magyarul)

Ez a mappa egy kész, "profi" GitHub-repo. Tartalmaz CI auto-buildet,
README-t badge-ekkel, CONTRIBUTING-ot, Makefile-t, LICENSE-t és .gitignore-t.

## Fontos a feltöltés ELŐTT

- **License:** ebben a csomagban **MIT** van (LICENSE fájl + README). Amikor a
  repót létrehoztad, ne válassz másik license-t a GitHubon (vagy ha már
  választottál Apache/GPL-t, töröld azt a repo LICENSE fájlját, és töltsd fel
  helyette az itteni MIT LICENSE-t) — ne legyen kétféle license.
- **README:** ha a repo létrehozásakor bepipáltad az "Add README"-t, akkor van
  egy üres README a repóban. Töltsd fel/illeszd be helyette az itteni
  `README.md`-t (felülírja).
- **A README badge-ei** a `LeviXXcodes-basment/ScanAll-flipperzero` repo-útvonalra
  mutatnak. Ha a tiéd más (más user vagy más repo-név), nyisd meg a `README.md`-t
  és cseréld le ezt a szöveget a sajátodra (keresés-csere).

## A) Weben (git nélkül)

1. A repo főoldalán: **Add file → Upload files**.
2. Húzd be ennek a mappának a TARTALMÁT (ne a ScanAll mappát magát):
   - `application.fam`
   - `scanall_app.c`
   - `Makefile`
   - `README.md`, `CONTRIBUTING.md`, `LICENSE`, `.gitignore`
   - az `images/` mappát a `scanall_icon.png`-vel
   - a `.github/` mappát a `workflows/build.yml`-lel
3. A rejtett/al-mappás fájloknál (`.github/workflows/build.yml`) ha a böngészős
   feltöltő nem viszi a mappát: "Add file → Create new file", és a név mezőbe
   írd be teljes útvonallal: `.github/workflows/build.yml`, majd illeszd be a
   tartalmát.
4. **Commit changes**.

## B) Parancssorból (git CLI)

A gépeden, ebben a mappában:

```
git init
git add .
git commit -m "ScanAll: NFC + RFID reader, CI build, docs"
git branch -M main
git remote add origin https://github.com/FELHASZNALONEVED/ScanAll-flipperzero.git
git push -u origin main
```

## Az auto-build (CI) használata

- Minden push a `main`-re lefordítja a `.fap`-ot, és feltölti **build
  artifact**-ként (Actions fül → a futás → Artifacts).
- Ha **verzió-taget** pusholsz (pl. `v1.138`), a `.fap` felkerül a
  **Releases** oldalra is:
  ```
  git tag v1.138
  git push origin v1.138
  ```
- Az Actions fülön látod a build státuszát (a README-ben lévő zöld badge is ezt
  mutatja).
