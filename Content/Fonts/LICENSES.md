# Fonts

Loaded at runtime by `Source/DirectorsCut/UI/DCUIStyle.cpp` from `Content/Fonts/`, which is staged as non-UFS.
If a file is missing, the UI falls back to the engine's Roboto.

| File | Font | Role | Licence | Source |
|---|---|---|---|---|
| `Inter-Regular/Medium/SemiBold/Bold.ttf` | Inter 4.0 (Rasmus Andersson) | Every UI text | SIL Open Font License 1.1 | https://github.com/rsms/inter |
| `JetBrainsMono-Variable.ttf` | JetBrains Mono (`JetBrainsMono[wght].ttf`) | Timers, money, numbers | SIL Open Font License 1.1 | https://github.com/google/fonts/tree/main/ofl/jetbrainsmono |
| `Lucide.ttf` + `lucide_codepoints.json` | Lucide icon font, lucide-static 0.460.0 | UI icons (glyphs by name → codepoint) | ISC License (Feather portions MIT) | https://lucide.dev · https://www.npmjs.com/package/lucide-static |

All of these are free for commercial use and can be redistributed embedded in a game. The fonts are unmodified.
`lucide_codepoints.json` is derived from lucide-static's `font/info.json` and maps icon names to their codepoints.
