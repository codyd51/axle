use crate::parse_utils::{BigEndianValue, FromFontBufInPlace, TransmuteFontBufInPlace};
use crate::parser::FontParser;
use crate::println;
use alloc::collections::BTreeMap;
use alloc::vec;
use alloc::vec::Vec;
use core::mem;
use core::ops::Range;
use itertools::Itertools;

/// §'cmap' Platforms
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum CharacterMapPlatformAndEncoding {
    Unicode(CharacterMapUnicodeEncoding),
    Macintosh,
    Microsoft,
}

impl CharacterMapPlatformAndEncoding {
    fn from_platform_and_specific_id(platform_id: u16, platform_specific_id: u16) -> Self {
        match platform_id {
            0 => {
                // Unicode
                Self::Unicode(match platform_specific_id {
                    0 => CharacterMapUnicodeEncoding::Version1_0,
                    1 => CharacterMapUnicodeEncoding::Version1_1,
                    3 => CharacterMapUnicodeEncoding::Version2_0Bmp,
                    4 => CharacterMapUnicodeEncoding::Version2_0Extended,
                    _ => panic!("Invalid Unicode platform_specific_id"),
                })
            }
            1 => Self::Macintosh,
            3 => Self::Microsoft,
            _ => panic!("Invalid character map platform_id"),
        }
    }
}

/// Unicode Platform-specific Encoding Identifiers
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum CharacterMapUnicodeEncoding {
    Version1_0,
    Version1_1,
    Version2_0Bmp,
    Version2_0Extended,
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapDataTableHeaderRaw {
    // Common header to all data table formats
    format: BigEndianValue<u16>,
    length: BigEndianValue<u16>,
}

impl TransmuteFontBufInPlace for CharacterMapDataTableHeaderRaw {}

#[derive(Debug, Copy, Clone)]
struct CharacterMapDataTableHeader {
    format: usize,
    length: usize,
}

impl FromFontBufInPlace<CharacterMapDataTableHeaderRaw> for CharacterMapDataTableHeader {
    fn from_in_place_buf(raw: &CharacterMapDataTableHeaderRaw) -> Self {
        Self {
            format: raw.format.into_value() as _,
            length: raw.length.into_value() as _,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapDataFormat4HeaderRaw {
    format: BigEndianValue<u16>,
    length: BigEndianValue<u16>,
    language: BigEndianValue<u16>,
    seg_count_x2: BigEndianValue<u16>,
    search_range: BigEndianValue<u16>,
    entry_selector: BigEndianValue<u16>,
    range_shift: BigEndianValue<u16>,
}

impl TransmuteFontBufInPlace for CharacterMapDataFormat4HeaderRaw {}

#[derive(Debug, Copy, Clone)]
struct CharacterMapDataFormat4Header {
    format: usize,
    length: usize,
    language: usize,
    seg_count_x2: usize,
    search_range: usize,
    entry_selector: usize,
    range_shift: usize,
}

impl FromFontBufInPlace<CharacterMapDataFormat4HeaderRaw> for CharacterMapDataFormat4Header {
    fn from_in_place_buf(raw: &CharacterMapDataFormat4HeaderRaw) -> Self {
        assert_eq!(raw.format.into_value(), 4);
        Self {
            format: raw.format.into_value() as _,
            length: raw.length.into_value() as _,
            language: raw.language.into_value() as _,
            seg_count_x2: raw.seg_count_x2.into_value() as _,
            search_range: raw.search_range.into_value() as _,
            entry_selector: raw.entry_selector.into_value() as _,
            range_shift: raw.range_shift.into_value() as _,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapDataFormat12HeaderRaw {
    format: BigEndianValue<u16>,
    reserved: BigEndianValue<u16>,
    length: BigEndianValue<u32>,
    language: BigEndianValue<u32>,
    groups_count: BigEndianValue<u32>,
}

impl TransmuteFontBufInPlace for CharacterMapDataFormat12HeaderRaw {}

#[derive(Debug, Copy, Clone)]
struct CharacterMapDataFormat12Header {
    format: usize,
    length: usize,
    language: usize,
    groups_count: usize,
}

impl FromFontBufInPlace<CharacterMapDataFormat12HeaderRaw> for CharacterMapDataFormat12Header {
    fn from_in_place_buf(raw: &CharacterMapDataFormat12HeaderRaw) -> Self {
        assert_eq!(raw.format.into_value(), 12);
        Self {
            format: raw.format.into_value() as _,
            length: raw.length.into_value() as _,
            language: raw.language.into_value() as _,
            groups_count: raw.groups_count.into_value() as _,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapDataFormat12GroupHeaderRaw {
    start_char_code: BigEndianValue<u32>,
    end_char_code: BigEndianValue<u32>,
    start_glyph_code: BigEndianValue<u32>,
}

impl TransmuteFontBufInPlace for CharacterMapDataFormat12GroupHeaderRaw {}

#[derive(Debug, Clone)]
struct CharacterMapDataFormat12GroupHeader {
    char_code_range: Range<usize>,
    start_glyph_code: usize,
}

impl FromFontBufInPlace<CharacterMapDataFormat12GroupHeaderRaw>
    for CharacterMapDataFormat12GroupHeader
{
    fn from_in_place_buf(raw: &CharacterMapDataFormat12GroupHeaderRaw) -> Self {
        Self {
            char_code_range: (raw.start_char_code.into_value() as usize
                ..raw.end_char_code.into_value() as usize + 1),
            start_glyph_code: raw.start_glyph_code.into_value() as usize,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapIndexRaw {
    version: BigEndianValue<u16>,
    subtables_count: BigEndianValue<u16>,
}

impl TransmuteFontBufInPlace for CharacterMapIndexRaw {}

#[derive(Debug, Copy, Clone)]
struct CharacterMapIndex {
    // PT: Only model the fields we use, for now
    subtables_count: usize,
}

impl FromFontBufInPlace<CharacterMapIndexRaw> for CharacterMapIndex {
    fn from_in_place_buf(raw: &CharacterMapIndexRaw) -> Self {
        Self {
            subtables_count: raw.subtables_count.into_value() as _,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct CharacterMapSubtableRaw {
    platform_id: BigEndianValue<u16>,
    platform_specific_id: BigEndianValue<u16>,
    offset: BigEndianValue<u32>,
}

impl TransmuteFontBufInPlace for CharacterMapSubtableRaw {}

#[derive(Debug, Copy, Clone)]
struct CharacterMapSubtable {
    platform_and_encoding: CharacterMapPlatformAndEncoding,
    offset: usize,
}

impl FromFontBufInPlace<CharacterMapSubtableRaw> for CharacterMapSubtable {
    fn from_in_place_buf(raw: &CharacterMapSubtableRaw) -> Self {
        Self {
            platform_and_encoding: CharacterMapPlatformAndEncoding::from_platform_and_specific_id(
                raw.platform_id.into_value(),
                raw.platform_specific_id.into_value(),
            ),
            offset: raw.offset.into_value() as _,
        }
    }
}

pub(crate) fn parse_character_map(parser: &FontParser) -> BTreeMap<usize, usize> {
    let character_map_header = parser.table_headers.get("cmap").unwrap();
    let character_map_structures_base = character_map_header.offset;
    let mut cursor = character_map_structures_base;
    let character_map_index =
        CharacterMapIndex::from_in_place_buf(parser.read_with_cursor(&mut cursor));
    //println!("got character map index {character_map_index:?}");
    let unicode_bmp_char_map_offset = (0..character_map_index.subtables_count)
        .find_map(|_| {
            let character_map_subtable =
                CharacterMapSubtable::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            //println!("Character map subtable {character_map_subtable:?}");
            let recognized_unicode_encodings = [
                CharacterMapPlatformAndEncoding::Unicode(
                    CharacterMapUnicodeEncoding::Version2_0Bmp,
                ),
                CharacterMapPlatformAndEncoding::Unicode(
                    CharacterMapUnicodeEncoding::Version2_0Extended,
                ),
                CharacterMapPlatformAndEncoding::Unicode(CharacterMapUnicodeEncoding::Version1_1),
                CharacterMapPlatformAndEncoding::Unicode(CharacterMapUnicodeEncoding::Version1_0),
            ];
            if recognized_unicode_encodings.contains(&character_map_subtable.platform_and_encoding)
            {
                Some(character_map_subtable.offset)
            } else {
                None
            }
        })
        .expect("Failed to find a recognized Unicode character map");
    //println!("Unicode BMP character map offset: {unicode_bmp_char_map_offset}");

    // All table formats start with a common 4-word header
    let data_header_offset = character_map_structures_base + unicode_bmp_char_map_offset;
    let table_base_header =
        CharacterMapDataTableHeader::from_in_place_buf(parser.read(data_header_offset));
    //println!("Got table base header {table_base_header:?}");

    match table_base_header.format {
        4 => parse_character_map_table_format4(parser, data_header_offset),
        12 => parse_character_map_table_format12(parser, data_header_offset),
        _ => panic!("Unhandled table format {}", table_base_header.format),
    }
}

fn parse_character_map_table_format4(
    parser: &FontParser,
    data_header_offset: usize,
) -> BTreeMap<usize, usize> {
    let table_format4_header =
        CharacterMapDataFormat4Header::from_in_place_buf(parser.read(data_header_offset));
    //println!("Got table format4 header {table_format4_header:?}");
    let seg_count = table_format4_header.seg_count_x2 / 2;

    let mut cursor = data_header_offset + mem::size_of::<CharacterMapDataFormat4HeaderRaw>();
    let mut segment_last_character_codes = vec![];
    for _ in 0..seg_count {
        let last_character_code = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        segment_last_character_codes.push(last_character_code);
    }
    //println!("segment_last_character_codes {segment_last_character_codes:?}");

    // Skip over the `reserved_pad` field
    cursor += mem::size_of::<u16>();

    let mut segment_start_character_codes = vec![];
    for _ in 0..seg_count {
        let first_character_code = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        segment_start_character_codes.push(first_character_code);
    }
    //println!("segment_start_character_codes {segment_start_character_codes:?}");

    let mut segment_id_deltas = vec![];
    for _ in 0..seg_count {
        let id_delta = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        segment_id_deltas.push(id_delta);
    }
    //println!("segment_id_deltas {segment_id_deltas:?}");

    let mut segment_id_range_offsets = vec![];
    let segment_id_range_base = cursor;
    for _ in 0..seg_count {
        let segment_id_range_offset = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        segment_id_range_offsets.push(segment_id_range_offset);
    }
    //println!("segment_id_range_offsets {segment_id_range_offsets:?}");

    let segment_ranges: Vec<Range<usize>> = segment_start_character_codes
        .iter()
        .zip(segment_last_character_codes.iter())
        .map(|(&start, &end)| (start as usize..end as usize + 1))
        .collect();
    //println!("Segment ranges: {segment_ranges:?}");

    // Map the first 256 Unicode codepoints (ASCII + extended ASCII)
    let mut glyph_indexes_to_codepoints = BTreeMap::new();
    for codepoint in 0..256 {
        let segment_index = segment_ranges
            .iter()
            .enumerate()
            .find_map(|(i, glyph_id_range)| {
                if glyph_id_range.contains(&codepoint) {
                    Some(i)
                } else {
                    None
                }
            });
        if segment_index.is_none() {
            //println!("No glyph for code point {codepoint}");
            continue;
        }
        let segment_index = segment_index.unwrap();
        //println!("Found character {character_to_map} in segment {segment_index}");
        let id_range = segment_id_range_offsets[segment_index] as usize;
        let id_delta = segment_id_deltas[segment_index];

        // > If the ID Range is 0, the ID Delta is added to the character code to get the glyph index
        let glyph_index = if id_range == 0 {
            // > NOTE: All id_delta arithmetic is modulo 65536.
            (id_delta.overflowing_add((codepoint as u16))).0
        } else {
            //glyphIndexAddress = idRangeOffset[i] + 2 * (c - startCode[i]) + (Ptr) &idRangeOffset[i]
            let glyph_index_addr = id_range
                + (2 * (codepoint - segment_ranges[segment_index].start))
                + (segment_id_range_base + (segment_index * 2));
            let glyph_index = u16::from_in_place_buf(parser.read(glyph_index_addr));
            // > If the glyph index isn't 0 (that is, if it's not the missing glyph),
            // the value is added to idDelta to get the actual glyph ID to use.
            match glyph_index {
                0 => glyph_index,
                // > NOTE: All id_delta arithmetic is modulo 65536.
                _ => (id_delta.overflowing_add(glyph_index)).0,
            }
        };
        //println!("Codepoint({codepoint}) = GlyphIndex({glyph_index})");
        glyph_indexes_to_codepoints.insert(glyph_index as usize, codepoint as usize);
    }

    glyph_indexes_to_codepoints
}

fn parse_character_map_table_format12(
    parser: &FontParser,
    data_header_offset: usize,
) -> BTreeMap<usize, usize> {
    let mut cursor = data_header_offset;
    let table_format12_header =
        CharacterMapDataFormat12Header::from_in_place_buf(parser.read_with_cursor(&mut cursor));
    //println!("Got table format12 header {table_format12_header:?}");
    let mut groups = vec![];
    for _ in 0..table_format12_header.groups_count {
        let group_header = CharacterMapDataFormat12GroupHeader::from_in_place_buf(
            parser.read_with_cursor(&mut cursor),
        );
        //println!("\tGot group header {group_header:?}");
        groups.push(group_header);
    }
    // Map the first 256 Unicode codepoints (ASCII + extended ASCII)
    let mut glyph_indexes_to_codepoints = BTreeMap::new();
    for codepoint in 0..256 {
        let group_containing_codepoint = groups
            .iter()
            .find(|&group| group.char_code_range.contains(&codepoint));
        if group_containing_codepoint.is_none() {
            // Codepoint unmapped
            //println!("Codepoint {codepoint} is unmapped");
            continue;
        }
        let group_containing_codepoint = group_containing_codepoint.unwrap();
        let glyph_index = group_containing_codepoint.start_glyph_code
            + (codepoint - group_containing_codepoint.char_code_range.start);
        glyph_indexes_to_codepoints.insert(glyph_index, codepoint);
    }

    glyph_indexes_to_codepoints
}
