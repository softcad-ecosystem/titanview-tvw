# TitanView / TVW - resurse pentru dezvoltatori si motorul grafic C99

[English](README.md) | [Site SoftCAD](https://softcad.ro) | [Documentatie TVW](https://softcad.ro/documentatie-tvw.html)

**TVW este o tehnologie reala, folosita deja in TitanPack, TitanArchive, TitanScann si TitanCAD**, conform titularului. Acest repository include acum **codul-sursa real al bibliotecii grafice libtitangfx** si al companionului `ts_gfx_path2`, impreuna cu bibliotecile Windows/Linux si documentatia pentru dezvoltatori.

## Ce contine

- `graphics/libtitangfx/src/`: motorul grafic C99 si antetele publice;
- `graphics/libtitangfx/libtitangfx.so` / `.dll`: Linux x64 / Windows x64;
- `graphics/libtitangfx/path2/`: motorul vectorial C99, cu sursa si binare;
- `graphics/libtitangfx/COPYING*`, `LICENSE.md`, `NOTICE.md`, `MANIFEST_SHA256.txt`: licente si amprente originale;
- exemple grafice, documentatie, verificari Python si automatizare GitHub Actions.

**Nu contine compilatorul si runtime-ul complet al limbajului TVW 4.4.0.** Acesta este un pachet grafic aprobat separat; motorul TVW complet ramane in distributia sa canonica. Nu sunt incluse sursele comerciale TitanCAD, TitanArchive, TitanPack, TitanScann, Shield sau motoarele proprietare CAD/cadastru/PDF.

## Verificari

```bash
cd graphics/libtitangfx
sha256sum -c MANIFEST_SHA256.txt
bash VERIFICARE.sh
cd ../..
python -m unittest discover -s tests -v
```

Pe Linux, sursele au fost recompilate si testele de randare native au trecut. Pe Windows, DLL-urile sunt PE x64, folosesc doar DLL-uri de sistem, dar **nu pretindem test fizic Windows in aceasta sesiune**. Verificarea automata Windows va rula dupa publicare.

## Licente

- **LGPL-3.0-or-later** pentru `graphics/libtitangfx` si `path2/` (sursa si binare).
- **CC0-1.0 OR Apache-2.0** pentru BLAKE3 vendorizat (avizele originale se pastreaza).
- Licenta Apache-2.0 din radacina se aplica **doar** documentatiei si instrumentelor originale adaugate de SoftCAD, nu bibliotecilor LGPL si nici runtime-ului TVW.

## Integritate / provenienta

Arhiva furnizata: `SoftCAD_libtitangfx_TVW_LGPL_2026-10-21.zip`. SHA-256 calculat: `b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047`.
Anterior fusese consemnat `7963af733d9232d6d4a75fac944293bdeb595bd49f1e1dc0740254a9d1874e94` pentru acelasi nume. Valoarea este **diferita**; noul ZIP are toate fisierele conforme cu manifestul propriu. Aceasta discrepanta este documentata, nu ascunsa.

Vezi [verificarea tehnica](docs/GRAPHICS_VERIFICATION.md) si [limitele publicarii](docs/PUBLICATION_SCOPE.md).
