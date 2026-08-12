#ifndef GUARD_FONTS_H
#define GUARD_FONTS_H

#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontNormalLatinGlyphs[];
#else
extern const u16 gFontNormalLatinGlyphs[];
#endif
extern const u8 gFontNormalLatinGlyphWidths[];
#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontNormalJapaneseGlyphs[];
extern u16 gFontSmallLatinGlyphs[];
#else
extern const u16 gFontNormalJapaneseGlyphs[];
extern const u16 gFontSmallLatinGlyphs[];
#endif
extern const u8 gFontSmallLatinGlyphWidths[];
#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontSmallJapaneseGlyphs[];
extern u16 gFontShortLatinGlyphs[];
#else
extern const u16 gFontSmallJapaneseGlyphs[];
extern const u16 gFontShortLatinGlyphs[];
#endif
extern const u8 gFontShortLatinGlyphWidths[];
#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontShortJapaneseGlyphs[];
#else
extern const u16 gFontShortJapaneseGlyphs[];
#endif
extern const u8 gFontShortJapaneseGlyphWidths[];
#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontNarrowLatinGlyphs[];
#else
extern const u16 gFontNarrowLatinGlyphs[];
#endif
extern const u8 gFontNarrowLatinGlyphWidths[];
#ifdef DESKTOP_EXTERNAL_GAME_CONTENT
extern u16 gFontSmallNarrowLatinGlyphs[];
extern u16 gFontFRLGMaleJapaneseGlyphs[];
extern u16 gFontFRLGFemaleJapaneseGlyphs[];
#else
extern const u16 gFontSmallNarrowLatinGlyphs[];
extern const u16 gFontFRLGMaleJapaneseGlyphs[];
extern const u16 gFontFRLGFemaleJapaneseGlyphs[];
#endif
extern const u8 gFontSmallNarrowLatinGlyphWidths[];

#endif // GUARD_FONTS_H
