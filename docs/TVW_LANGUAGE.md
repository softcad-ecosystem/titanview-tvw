# TVW declarative language / limbajul declarativ TVW

**Evidence origin:** SoftCAD public documentation, October 2026:

- RO: <https://softcad.ro/referinta-tvw-limbaj.html>
- EN: <https://softcad.ro/en/tvw-language.html>
- Format identity: <https://softcad.ro/format-tvw.html>
- Interoperability: <https://softcad.ro/referinta-tcb-txb-tvw.html>

The TVW source language (`.tvw`) describes graphical interfaces declaratively. It is distinct from TitanScript (`.tss`), which provides automation scripting.

The **implemented production technology exists** (TitanPack, TitanArchive, TitanScann, TitanCAD). Separately, this package **does not include the current parser source or a parser-matched published grammar**, so this reference does not invent EBNF, keywords, a type system, syntax trees or positive executable `.tvw` samples.

A `.tvw` file has no verified public binary magic. The existing **read-only identity gate** checks whether a bounded source file is plausible UTF-8 text (up to 64 KiB), which is **not** a parser acceptance or a guarantee of safety. Real compilation/execution requires the canonical TVW toolchain; public validity claims must be driven by its own positive and negative fixtures.

## Language-reference handoff / Predare referință

Before labeling the language specification normative, copy directly from the canonical 4.4.0 parser/compiler reference and its test corpus, preserving current spelling and behavior. Publish independently runnable accepted/rejected `.tvw` fixtures and Windows/Linux parity logs. **This is a publication gate, not a new engine development task.**
