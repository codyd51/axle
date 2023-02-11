use crate::{Codepoint, Font, GlyphIndex};
use agx_definitions::{Point, PointF64, Polygon, Rect, Size};
use alloc::borrow::ToOwned;
use alloc::collections::BTreeMap;
use alloc::fmt::Debug;
use alloc::fmt::{Display, Formatter};
use alloc::vec;
use alloc::{
    rc::{Rc, Weak},
    string::String,
    vec::Vec,
};
use core::cmp::max;
use core::mem;
use core::ops::{Index, Range};
use itertools::Itertools;
use num_traits::PrimInt;

use crate::glyphs::parse_glyph;
use crate::metrics::{
    parse_horizontal_metrics, parse_vertical_metrics, GlyphMetrics, HheaTable, HheaTableRaw,
    LongHorMetric, LongHorMetricRaw, VerticalMetrics,
};
use crate::parse_utils::{
    fixed_word_to_i32, BigEndianValue, FromFontBufInPlace, TransmuteFontBufInPlace,
};
#[cfg(target_os = "axle")]
use axle_rt::println;
#[cfg(not(target_os = "axle"))]
use std::println;

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct OffsetSubtableRaw {
    scalar_type: BigEndianValue<u32>,
    num_tables: BigEndianValue<u16>,
    search_range: BigEndianValue<u16>,
    entry_selector: BigEndianValue<u16>,
    range_shift: BigEndianValue<u16>,
}

impl TransmuteFontBufInPlace for OffsetSubtableRaw {}

#[derive(Debug, Copy, Clone)]
struct OffsetSubtable {
    scalar_type: u32,
    num_tables: u16,
    search_range: u16,
    entry_selector: u16,
    range_shift: u16,
}

impl FromFontBufInPlace<OffsetSubtableRaw> for OffsetSubtable {
    fn from_in_place_buf(raw: &OffsetSubtableRaw) -> Self {
        Self {
            scalar_type: raw.scalar_type.into_value(),
            num_tables: raw.num_tables.into_value(),
            search_range: raw.search_range.into_value(),
            entry_selector: raw.entry_selector.into_value(),
            range_shift: raw.range_shift.into_value(),
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct TableRaw {
    tag: [u8; 4],
    checksum: BigEndianValue<u32>,
    offset: BigEndianValue<u32>,
    length: BigEndianValue<u32>,
}

impl TransmuteFontBufInPlace for TableRaw {}

#[derive(Debug, Copy, Clone)]
pub(crate) struct TableHeader<'a> {
    tag: &'a str,
    checksum: u32,
    pub(crate) offset: usize,
    length: usize,
}

impl<'a> TableHeader<'a> {
    fn new(raw: &'a TableRaw) -> Self {
        let tag_as_str = core::str::from_utf8(&raw.tag).unwrap();
        Self {
            tag: tag_as_str,
            checksum: raw.checksum.into_value(),
            offset: raw.offset.into_value() as _,
            length: raw.length.into_value() as _,
        }
    }
}

impl<'a> Display for TableHeader<'a> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "[{} {:08x} - {:08x}]",
            self.tag,
            self.offset,
            self.offset + self.length
        )
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct HeadTableRaw {
    version: BigEndianValue<u32>,
    font_revision: BigEndianValue<u32>,
    checksum_adjustment: BigEndianValue<u32>,
    magic: BigEndianValue<u32>,
    flags: BigEndianValue<u16>,
    units_per_em: BigEndianValue<u16>,
    date_created: BigEndianValue<u64>,
    date_modified: BigEndianValue<u64>,
    min_x: BigEndianValue<i16>,
    min_y: BigEndianValue<i16>,
    max_x: BigEndianValue<i16>,
    max_y: BigEndianValue<i16>,
    mac_style: BigEndianValue<u16>,
    lowest_rec_ppem: BigEndianValue<u16>,
    font_direction_hint: BigEndianValue<i16>,
    index_to_loc_format: BigEndianValue<i16>,
    glyph_data_format: BigEndianValue<i16>,
}

impl TransmuteFontBufInPlace for HeadTableRaw {}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum IndexToLocFormat {
    ShortOffsets,
    LongOffsets,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum FontDirectionality {
    MixedDirection,
    LeftToRight,
    LeftToRightAndNeutral,
    RightToLeft,
    RightToLeftAndNeutral,
}

#[derive(Debug, Copy, Clone)]
struct HeadTable {
    version: i32,
    font_revision: i32,
    checksum_adjustment: u32,
    magic: u32,
    flags: u16,
    units_per_em: u16,
    date_created: u64,
    date_modified: u64,
    glyph_bounding_box: Rect,
    mac_style: u16,
    lowest_recommended_pixel_per_em: u16,
    directionality: FontDirectionality,
    index_to_loc_format: IndexToLocFormat,
    glyph_data_format: i16,
}

impl HeadTable {
    // PT: Defined by the spec §Table 22
    const MAGIC: u32 = 0x5f0f3cf5;
}

impl FromFontBufInPlace<HeadTableRaw> for HeadTable {
    fn from_in_place_buf(raw: &HeadTableRaw) -> Self {
        let glyph_bounding_box_origin =
            Point::new(raw.min_x.into_value() as _, raw.min_y.into_value() as _);

        let flags = raw.flags.into_value();
        //assert_eq!(flags, 0b1001, "Other flags unhandled for now");

        let directionality = match raw.font_direction_hint.into_value() {
            0 => FontDirectionality::MixedDirection,
            1 => FontDirectionality::LeftToRight,
            2 => FontDirectionality::LeftToRightAndNeutral,
            -1 => FontDirectionality::RightToLeft,
            -2 => FontDirectionality::RightToLeftAndNeutral,
            _ => panic!("Invalid font direction hint"),
        };
        assert!(
            directionality == FontDirectionality::LeftToRightAndNeutral
                || directionality == FontDirectionality::LeftToRight,
            "Only left-to-right/-and-neutral is handled for now"
        );

        let index_to_loc_format = match raw.index_to_loc_format.into_value() {
            0 => IndexToLocFormat::ShortOffsets,
            1 => IndexToLocFormat::LongOffsets,
            _ => panic!("Invalid index_to_loc_format"),
        };

        let ret = Self {
            version: fixed_word_to_i32(raw.version.into_value()),
            font_revision: fixed_word_to_i32(raw.font_revision.into_value()),
            checksum_adjustment: raw.checksum_adjustment.into_value(),
            magic: raw.magic.into_value(),
            flags,
            units_per_em: raw.units_per_em.into_value(),
            date_created: raw.date_created.into_value(),
            date_modified: raw.date_modified.into_value(),
            glyph_bounding_box: Rect::from_parts(
                glyph_bounding_box_origin,
                Size::new(
                    isize::abs((raw.max_x.into_value() as isize) - glyph_bounding_box_origin.x),
                    isize::abs((raw.max_y.into_value() as isize) - glyph_bounding_box_origin.y),
                ),
            ),
            mac_style: raw.mac_style.into_value(),
            lowest_recommended_pixel_per_em: raw.lowest_rec_ppem.into_value(),
            directionality,
            index_to_loc_format,
            glyph_data_format: raw.glyph_data_format.into_value(),
        };
        assert_eq!(ret.magic, Self::MAGIC);
        ret
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct MaxProfileRaw {
    version: BigEndianValue<i32>,
    num_glyphs: BigEndianValue<u16>,
    max_points: BigEndianValue<u16>,
    max_contours: BigEndianValue<u16>,
    max_component_points: BigEndianValue<u16>,
    max_component_contours: BigEndianValue<u16>,
    max_zones: BigEndianValue<u16>,
    max_twilight_points: BigEndianValue<u16>,
    max_storage: BigEndianValue<u16>,
    max_function_defs: BigEndianValue<u16>,
    max_instruction_defs: BigEndianValue<u16>,
    max_stack_elements: BigEndianValue<u16>,
    max_size_of_instructions: BigEndianValue<u16>,
    max_component_elements: BigEndianValue<u16>,
    max_component_depth: BigEndianValue<u16>,
}

impl TransmuteFontBufInPlace for MaxProfileRaw {}

#[derive(Debug, Copy, Clone)]
struct MaxProfile {
    // PT: Only model the fields we use, for now
    num_glyphs: usize,
}

impl FromFontBufInPlace<MaxProfileRaw> for MaxProfile {
    fn from_in_place_buf(raw: &MaxProfileRaw) -> Self {
        Self {
            num_glyphs: raw.num_glyphs.into_value() as _,
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

pub struct FontParser<'a> {
    font_data: &'a [u8],
    head: Option<HeadTable>,
    pub(crate) table_headers: BTreeMap<&'a str, TableHeader<'a>>,
}

impl<'a> FontParser<'a> {
    pub fn new(font_data: &'a [u8]) -> Self {
        Self {
            font_data,
            head: None,
            table_headers: BTreeMap::new(),
        }
    }

    fn read<T: TransmuteFontBufInPlace>(&self, offset: usize) -> &'a T {
        unsafe {
            let ptr = self.font_data.as_ptr().offset(offset as isize);
            let reference: &'a T = &*{ ptr as *const T };
            reference
        }
    }

    pub(crate) fn read_with_cursor<T: TransmuteFontBufInPlace>(&self, cursor: &mut usize) -> &'a T {
        unsafe {
            let ptr = self.font_data.as_ptr().offset(*cursor as isize);
            let reference: &'a T = &*{ ptr as *const T };
            *cursor += mem::size_of::<T>();
            reference
        }
    }

    pub(crate) fn parse_table<A: TransmuteFontBufInPlace, T: FromFontBufInPlace<A>>(
        &self,
        tag: &str,
    ) -> T {
        let table_header = self.table_headers.get(tag).unwrap();
        let raw: &A = self.read(table_header.offset);
        T::from_in_place_buf(raw)
    }

    pub fn parse(&mut self) -> Font {
        let mut cursor = 0;
        let offset_subtable = OffsetSubtable::from_in_place_buf(self.read_with_cursor(&mut cursor));
        println!("Got offset subtable {offset_subtable:?}",);

        for i in 0..offset_subtable.num_tables {
            let table = TableHeader::new(self.read_with_cursor(&mut cursor));
            println!("Table #{i}: {table}");
            self.table_headers.insert(table.tag, table);
        }
        let head = self.parse_table("head");
        self.head = Some(head);
        println!("Found head: {:?}", head);
        let glyph_bounding_box = head.glyph_bounding_box;

        let max_profile: MaxProfile = self.parse_table("maxp");
        println!("Got max profile {max_profile:?}");

        let glyph_indexes_to_codepoints = self.parse_character_map();

        let mut all_glyphs = vec![];
        let mut codepoints_to_glyph_indexes = BTreeMap::new();
        for i in 0..max_profile.num_glyphs {
            let parsed_glyph = parse_glyph(self, i, &glyph_bounding_box);
            all_glyphs.push(parsed_glyph);

            match glyph_indexes_to_codepoints.get(&i) {
                None => (),
                Some(codepoint) => {
                    codepoints_to_glyph_indexes.insert(Codepoint(*codepoint), GlyphIndex(i));
                }
            }
        }

        let horizontal_glyph_metrics = parse_horizontal_metrics(self);
        let vertical_glyph_metrics = parse_vertical_metrics(self, max_profile.num_glyphs);
        for (i, glyph) in all_glyphs.iter_mut().enumerate() {
            if let Some(horizontal_metrics) = horizontal_glyph_metrics.get(i) {
                glyph
                    .render_metrics
                    .set_horizontal_metrics(horizontal_metrics.clone());
            }
            if vertical_glyph_metrics.is_some() {
                glyph
                    .render_metrics
                    .set_vertical_metrics(vertical_glyph_metrics.as_ref().unwrap()[i].clone());
            }
        }

        Font::new(
            // TODO(PT): Parse font names
            "abc",
            &self.head.unwrap().glyph_bounding_box,
            self.head.unwrap().units_per_em as _,
            all_glyphs,
            codepoints_to_glyph_indexes,
        )
    }

    fn parse_character_map(&self) -> BTreeMap<usize, usize> {
        let character_map_header = self.table_headers.get("cmap").unwrap();
        let character_map_structures_base = character_map_header.offset;
        let mut cursor = character_map_structures_base;
        let character_map_index =
            CharacterMapIndex::from_in_place_buf(self.read_with_cursor(&mut cursor));
        println!("got character map index {character_map_index:?}");
        let unicode_bmp_char_map_offset = (0..character_map_index.subtables_count)
            .find_map(|_| {
                let character_map_subtable =
                    CharacterMapSubtable::from_in_place_buf(self.read_with_cursor(&mut cursor));
                println!("Character map subtable {character_map_subtable:?}");
                let recognized_unicode_encodings = [
                    CharacterMapPlatformAndEncoding::Unicode(
                        CharacterMapUnicodeEncoding::Version2_0Bmp,
                    ),
                    CharacterMapPlatformAndEncoding::Unicode(
                        CharacterMapUnicodeEncoding::Version2_0Extended,
                    ),
                    CharacterMapPlatformAndEncoding::Unicode(
                        CharacterMapUnicodeEncoding::Version1_1,
                    ),
                ];
                if recognized_unicode_encodings
                    .contains(&character_map_subtable.platform_and_encoding)
                {
                    Some(character_map_subtable.offset)
                } else {
                    None
                }
            })
            .expect("Failed to find a recognized Unicode character map");
        println!("Unicode BMP character map offset: {unicode_bmp_char_map_offset}");

        // All table formats start with a common 4-word header
        let data_header_offset = character_map_structures_base + unicode_bmp_char_map_offset;
        let table_base_header =
            CharacterMapDataTableHeader::from_in_place_buf(self.read(data_header_offset));
        println!("Got table base header {table_base_header:?}");

        match table_base_header.format {
            4 => self.parse_character_map_table_format4(data_header_offset),
            12 => self.parse_character_map_table_format12(data_header_offset),
            _ => panic!("Unhandled table format {}", table_base_header.format),
        }
    }

    fn parse_character_map_table_format4(
        &self,
        data_header_offset: usize,
    ) -> BTreeMap<usize, usize> {
        let table_format4_header =
            CharacterMapDataFormat4Header::from_in_place_buf(self.read(data_header_offset));
        //println!("Got table format4 header {table_format4_header:?}");
        let seg_count = table_format4_header.seg_count_x2 / 2;

        let mut cursor = data_header_offset + mem::size_of::<CharacterMapDataFormat4HeaderRaw>();
        let mut segment_last_character_codes = vec![];
        for _ in 0..seg_count {
            let last_character_code = u16::from_in_place_buf(self.read_with_cursor(&mut cursor));
            segment_last_character_codes.push(last_character_code);
        }
        //println!("segment_last_character_codes {segment_last_character_codes:?}");

        // Skip over the `reserved_pad` field
        cursor += mem::size_of::<u16>();

        let mut segment_start_character_codes = vec![];
        for _ in 0..seg_count {
            let first_character_code = u16::from_in_place_buf(self.read_with_cursor(&mut cursor));
            segment_start_character_codes.push(first_character_code);
        }
        //println!("segment_start_character_codes {segment_start_character_codes:?}");

        let mut segment_id_deltas = vec![];
        for _ in 0..seg_count {
            let id_delta = u16::from_in_place_buf(self.read_with_cursor(&mut cursor));
            segment_id_deltas.push(id_delta);
        }
        //println!("segment_id_deltas {segment_id_deltas:?}");

        let mut segment_id_range_offsets = vec![];
        let segment_id_range_base = cursor;
        for _ in 0..seg_count {
            let segment_id_range_offset =
                u16::from_in_place_buf(self.read_with_cursor(&mut cursor));
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
            let segment_index =
                segment_ranges
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
                let glyph_index = u16::from_in_place_buf(self.read(glyph_index_addr));
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
        &self,
        data_header_offset: usize,
    ) -> BTreeMap<usize, usize> {
        let mut cursor = data_header_offset;
        let table_format12_header =
            CharacterMapDataFormat12Header::from_in_place_buf(self.read_with_cursor(&mut cursor));
        println!("Got table format12 header {table_format12_header:?}");
        let mut groups = vec![];
        for _ in 0..table_format12_header.groups_count {
            let group_header = CharacterMapDataFormat12GroupHeader::from_in_place_buf(
                self.read_with_cursor(&mut cursor),
            );
            println!("\tGot group header {group_header:?}");
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
                println!("Codepoint {codepoint} is unmapped");
                continue;
            }
            let group_containing_codepoint = group_containing_codepoint.unwrap();
            let glyph_index = group_containing_codepoint.start_glyph_code
                + (codepoint - group_containing_codepoint.char_code_range.start);
            glyph_indexes_to_codepoints.insert(glyph_index, codepoint);
        }

        glyph_indexes_to_codepoints
    }

    pub(crate) fn get_glyph_offset(&self, glyph_index: usize) -> usize {
        let locations_table = self.table_headers.get("loca").unwrap();
        match self.head.unwrap().index_to_loc_format {
            IndexToLocFormat::ShortOffsets => {
                let locations_entry_offset = glyph_index * mem::size_of::<u16>();
                let scaled_glyph_offset: &BigEndianValue<u16> =
                    self.read(locations_table.offset + locations_entry_offset);
                // §Table 33: The actual local offset divided by 2 is stored.
                scaled_glyph_offset.into_value() as usize * 2
            }
            IndexToLocFormat::LongOffsets => {
                let locations_entry_offset = glyph_index * mem::size_of::<u32>();
                let scaled_glyph_offset: &BigEndianValue<u32> =
                    self.read(locations_table.offset + locations_entry_offset);
                scaled_glyph_offset.into_value() as usize
            }
        }
    }
}
