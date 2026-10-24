# Publicare GitHub - titanview-tvw cu sursele grafice reale

1. Dezarhiveaza acest pachet local. Contine un singur folder Git: `titanview-tvw`.
2. Creeaza un repository gol **Public** in organizatia `softcad-ecosystem`, numit `titanview-tvw`, fara a genera README/licenta/gitignore.
3. Cloneaza-l intr-un folder nou cu GitHub Desktop (nu peste sursele extrase).
4. Copiaza **CONTINUTUL** folderului `titanview-tvw/` in repository-ul clonat; nu copia un eventual `.git` exterior.
5. Verifica existenta `graphics/libtitangfx/src/`, `graphics/libtitangfx/COPYING.LESSER`, `graphics/libtitangfx/libtitangfx.dll`, `.so` si `graphics/libtitangfx/path2/`.
6. Summary: `Initial public release - TitanView graphics LGPL` -> Commit to main -> Push origin.
7. Verifica `Actions` (Python checks, Linux graphics rebuild/smoke, Windows DLL smoke) si `Code` (vizibilitate Public).
8. **NU** incarca acest ZIP in Cloudflare Pages sau R2; el este strict pentru GitHub.

IMPORTANT: In prima documentatie GitHub aparea SHA-256 `7963af...`, dar ZIP-ul real primit are SHA-256 `b2d62c...`. Provenienta este documentata in `docs/GRAPHICS_VERIFICATION.md`. Nu declara identitate binara cu versiunea veche fara exemplarul vechi pentru comparatie.

Nu include compilatorul TVW 4.4.0; doar motorul grafic si instrumentele de dezvoltare. Licenta radacina Apache-2.0 NU inlocuieste LGPL a bibliotecilor.
