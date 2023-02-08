use crate::{Codepoint, Font, GlyphIndex};
use agx_definitions::{Point, Polygon, Rect, Size};
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

use crate::metrics::{
    GlyphMetrics, HheaTable, HheaTableRaw, LongHorMetric, LongHorMetricRaw, VerticalMetrics,
};
use crate::parse_utils::{
    fixed_word_to_i32, BigEndianValue, FromFontBufInPlace, TransmuteFontBufInPlace,
};
use crate::parser::GlyphRenderInstructions::CompoundGlyph;
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
struct TableHeader<'a> {
    tag: &'a str,
    checksum: u32,
    offset: usize,
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
struct GlyphDescriptionRaw {
    contour_count: BigEndianValue<i16>,
    min_x: BigEndianValue<i16>,
    min_y: BigEndianValue<i16>,
    max_x: BigEndianValue<i16>,
    max_y: BigEndianValue<i16>,
}

impl TransmuteFontBufInPlace for GlyphDescriptionRaw {}

#[derive(Debug, Copy, Clone)]
struct GlyphDescription {
    contour_count: isize,
    bounding_box: Rect,
}

impl FromFontBufInPlace<GlyphDescriptionRaw> for GlyphDescription {
    fn from_in_place_buf(raw: &GlyphDescriptionRaw) -> Self {
        let contour_count = raw.contour_count.into_value() as isize;
        /*
        assert!(
            contour_count >= 1,
            "Other contour counts are unhandled for now"
        );
        */

        let bounding_box_origin =
            Point::new(raw.min_x.into_value() as _, raw.min_y.into_value() as _);
        let bounding_box = Rect::from_parts(
            bounding_box_origin,
            Size::new(
                isize::abs((raw.max_x.into_value() as isize) - bounding_box_origin.x),
                isize::abs((raw.max_y.into_value() as isize) - bounding_box_origin.y),
            ),
        );

        Self {
            contour_count,
            bounding_box,
        }
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

#[derive(Debug, Clone)]
pub struct PolygonsGlyphRenderInstructions {
    pub polygons: Vec<Polygon>,
}

impl PolygonsGlyphRenderInstructions {
    fn new(polygons: &Vec<Polygon>) -> Self {
        Self {
            polygons: polygons.to_owned(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct BlankGlyphRenderInstructions {}

impl BlankGlyphRenderInstructions {
    fn new() -> Self {
        Self {}
    }
}

#[derive(Debug, Clone)]
pub struct CompoundGlyphRenderInstructions {}

impl CompoundGlyphRenderInstructions {
    fn new() -> Self {
        Self {}
    }
}

#[derive(Debug, Clone)]
pub struct GlyphRenderMetrics {
    pub bounding_box: Rect,
    horizontal_metrics: Option<LongHorMetric>,
    vertical_metrics: Option<VerticalMetrics>,
}

impl GlyphRenderMetrics {
    fn new(bounding_box: &Rect) -> Self {
        Self {
            bounding_box: bounding_box.clone(),
            horizontal_metrics: None,
            vertical_metrics: None,
        }
    }

    fn set_horizontal_metrics(&mut self, metrics: LongHorMetric) {
        self.horizontal_metrics = Some(metrics)
    }

    fn set_vertical_metrics(&mut self, metrics: VerticalMetrics) {
        self.vertical_metrics = Some(metrics)
    }

    pub fn metrics(&self) -> GlyphMetrics {
        let h = self.horizontal_metrics.as_ref().unwrap();
        let v = self
            .vertical_metrics
            .as_ref()
            .unwrap_or(&VerticalMetrics {
                advance_height: self.bounding_box.height() as _,
                top_side_bearing: 0,
            })
            .clone();
        GlyphMetrics::new(
            h.advance_width,
            v.advance_height,
            h.left_side_bearing,
            v.top_side_bearing,
        )
    }
}

#[derive(Debug, Clone)]
pub enum GlyphRenderInstructions {
    PolygonsGlyph(PolygonsGlyphRenderInstructions),
    BlankGlyph(BlankGlyphRenderInstructions),
    CompoundGlyph(CompoundGlyphRenderInstructions),
}

#[derive(Debug, Clone)]
pub struct GlyphRenderDescription {
    render_metrics: GlyphRenderMetrics,
    pub render_instructions: GlyphRenderInstructions,
}

impl GlyphRenderDescription {
    fn polygons_glyph(glyph_bounding_box: &Rect, polygons: &Vec<Polygon>) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::PolygonsGlyph(
                PolygonsGlyphRenderInstructions::new(polygons),
            ),
        }
    }

    fn blank_glyph(glyph_bounding_box: &Rect) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::BlankGlyph(
                BlankGlyphRenderInstructions::new(),
            ),
        }
    }

    fn compound_glyph(glyph_bounding_box: &Rect) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::CompoundGlyph(
                CompoundGlyphRenderInstructions::new(),
            ),
        }
    }

    pub fn metrics(&self) -> GlyphMetrics {
        self.render_metrics.metrics()
    }
}

pub struct FontParser<'a> {
    font_data: &'a [u8],
    head: Option<HeadTable>,
    table_headers: BTreeMap<&'a str, TableHeader<'a>>,
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

    fn read_with_cursor<T: TransmuteFontBufInPlace>(&self, cursor: &mut usize) -> &'a T {
        unsafe {
            let ptr = self.font_data.as_ptr().offset(*cursor as isize);
            let reference: &'a T = &*{ ptr as *const T };
            *cursor += mem::size_of::<T>();
            reference
        }
    }

    fn parse_table<A: TransmuteFontBufInPlace, T: FromFontBufInPlace<A>>(&self, tag: &str) -> T {
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
            let parsed_glyph = self.parse_glyph(i, &glyph_bounding_box);
            all_glyphs.push(parsed_glyph);

            match glyph_indexes_to_codepoints.get(&i) {
                None => (),
                Some(codepoint) => {
                    codepoints_to_glyph_indexes.insert(Codepoint(*codepoint), GlyphIndex(i));
                }
            }
        }

        let horizontal_glyph_metrics = self.parse_horizontal_metrics();
        let vertical_glyph_metrics = self.parse_vertical_metrics(max_profile.num_glyphs);
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
            let id_delta = segment_id_deltas[segment_index];
            //assert_eq!(id_delta, 0, "Non-zero deltas are unhandled for now");
            if id_delta != 0 {
                println!("Non-zero deltas are unhandled for now");
                continue;
            }
            let id_range = segment_id_range_offsets[segment_index] as usize;
            //glyphIndexAddress = idRangeOffset[i] + 2 * (c - startCode[i]) + (Ptr) &idRangeOffset[i]
            let glyph_index_addr = id_range
                + (2 * (codepoint - segment_ranges[segment_index].start))
                + (segment_id_range_base + (segment_index * 2));

            let glyph_index = u16::from_in_place_buf(self.read(glyph_index_addr));
            println!("Codepoint({codepoint}) = GlyphIndex({glyph_index})");
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

    fn get_glyph_offset(&self, glyph_index: usize) -> usize {
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

    fn parse_glyph(&self, glyph_index: usize, glyph_bounding_box: &Rect) -> GlyphRenderDescription {
    fn parse_glyph(
        &self,
        glyph_index: usize,
        all_glyphs_bounding_box: &Rect,
    ) -> GlyphRenderDescription {
        let glyph_header = self.table_headers.get("glyf").unwrap();
        let (glyph_local_offset, glyph_data_length) = self.get_glyph_offset_and_length(glyph_index);
        let glyph_offset = glyph_header.offset + glyph_local_offset;
        // Empty glyph?
        if glyph_data_length == 0 {
            return GlyphRenderDescription::blank_glyph(&all_glyphs_bounding_box);
        }

        //println!("GlyphIndex({glyph_index}) offset = {glyph_offset}");
        let mut cursor = glyph_offset;
        let glyph_description =
            GlyphDescription::from_in_place_buf(self.read_with_cursor(&mut cursor));

        // Handle compound glyphs
        // Hack to make compound glyphs work
        if glyph_description.contour_count <= 0 {
            //println!("Compound glyph!");
            return GlyphRenderDescription::compound_glyph(&all_glyphs_bounding_box);
        }

        // This informs how many points there are in total
        let mut last_point_indexes = vec![];
        for _ in 0..glyph_description.contour_count {
            let contour_end = u16::from_in_place_buf(self.read_with_cursor(&mut cursor));
            last_point_indexes.push(contour_end as isize);
        }
        let max_point_idx = last_point_indexes.iter().max().unwrap();
        let point_count = max_point_idx + 1;

        // Skip instructions
        let instructions_len = self
            .read_with_cursor::<BigEndianValue<u16>>(&mut cursor)
            .into_value();
        //println!("\tSkipping instructions len: {instructions_len}");
        cursor += instructions_len as usize;

        let mut all_flags = vec![];
        let mut flag_count_to_parse = point_count as usize;
        while flag_count_to_parse > 0 {
            let flag_byte: u8 = *self.read_with_cursor(&mut cursor);
            let flags = get_flags_from_byte(flag_byte);
            if flags.contains(&GlyphOutlineFlag::Repeat) {
                let mut flags = flags.clone();
                flags.retain(|&f| f != GlyphOutlineFlag::Repeat);
                let repeat_count: u8 = *self.read_with_cursor(&mut cursor);
                flag_count_to_parse -= repeat_count as usize;
                // Add 1 to the repeat count to account for the initial set, plus repeats
                for _ in 0..repeat_count as usize + 1 {
                    all_flags.push(flags.clone());
                }
            } else {
                all_flags.push(flags);
            }
            flag_count_to_parse -= 1;
        }

        // Parse X coordinates
        let x_values =
            self.interpret_values_via_flags(&mut cursor, &all_flags, CoordinateComponentType::X);
        let y_values =
            self.interpret_values_via_flags(&mut cursor, &all_flags, CoordinateComponentType::Y);
        let points: Vec<Point> = x_values
            .iter()
            .zip(y_values.iter())
            // Flip the Y axis of every point to match our coordinate system
            .map(|(&x, &y)| Point::new(x, glyph_description.bounding_box.max_y() - y))
            .collect();

        // Split the total collection of points into polygons, using the last-point-indexes that
        // were given in the glyph metadata
        let mut polygons = vec![];
        let mut polygon_start_end_index_pairs = last_point_indexes.to_owned();
        // The first polygon starts at index 0
        // > The first point number of each contour (except the first) is one greater than the last point number of the preceding contour.
        polygon_start_end_index_pairs.insert(0, -1);
        for (&end_idx_of_previous_contour, &end_idx) in
            polygon_start_end_index_pairs.iter().tuple_windows()
        {
            let start_idx = end_idx_of_previous_contour + 1;
            polygons.push(Polygon::new(
                &points[start_idx as usize..(end_idx + 1) as usize],
            ))
        }

        /*
        println!(
            "Finished parsing glyph #{glyph_index} with polygon count {}",
            polygons.len()
        );
        */
        GlyphRenderDescription::polygons_glyph(&glyph_description.bounding_box, &polygons)
    }

    fn interpret_values_via_flags(
        &self,
        cursor: &mut usize,
        all_flags: &Vec<Vec<GlyphOutlineFlag>>,
        value_type: CoordinateComponentType,
    ) -> Vec<isize> {
        let mut values = vec![];
        let short_flag = match value_type {
            CoordinateComponentType::X => GlyphOutlineFlag::ShortX,
            CoordinateComponentType::Y => GlyphOutlineFlag::ShortY,
        };
        let same_flag = match value_type {
            CoordinateComponentType::X => GlyphOutlineFlag::SameX,
            CoordinateComponentType::Y => GlyphOutlineFlag::SameY,
        };
        let mut last_value = 0;
        for flag_set in all_flags.iter() {
            let value = if flag_set.contains(&short_flag) {
                // Value is u8
                let value_without_sign = *self.read_with_cursor::<u8>(cursor) as isize;
                // §Table 16:
                // > If the Short bit is set, Same describes the sign of the value,
                // with a value of 1 equalling positive and a zero value negative.
                if flag_set.contains(&same_flag) {
                    //println!("\tvalue_without_sign={value_without_sign}, SameValue");
                    last_value + value_without_sign
                } else {
                    //println!("\tvalue_without_sign={value_without_sign}, NotSameValue");
                    last_value - value_without_sign
                }
            } else {
                // Value is u16
                // §Table 16:
                // > If the Short bit is not set, and Same is set, then the current value
                // is the same as the previous value.
                // Otherwise, the current value is a signed 16-bit delta vector, and the
                // delta vector is the change in the value.
                if flag_set.contains(&same_flag) {
                    //println!("\tLong, Same");
                    last_value
                } else {
                    let value = self
                        .read_with_cursor::<BigEndianValue<i16>>(cursor)
                        .into_value() as isize;
                    //println!("\tLong, NotSame ({value})");
                    last_value + value
                }
            };
            last_value = value;
            //println!("\tGot value {value}");
            values.push(last_value);
        }
        values
    }

    fn parse_horizontal_metrics(&self) -> Vec<LongHorMetric> {
        let hhea: HheaTable = self.parse_table("hhea");
        println!("Got hhea {hhea:?}");
        let hmtx_offset = self.table_headers.get("hmtx").unwrap().offset;
        let mut cursor = hmtx_offset;
        let mut glyph_metrics = vec![];
        for _ in 0..hhea.long_hor_metrics_count {
            let glyph_metric = LongHorMetric::from_in_place_buf(self.read_with_cursor(&mut cursor));
            glyph_metrics.push(glyph_metric);
        }
        glyph_metrics
    }

    fn parse_vertical_metrics(&self, glyph_count: usize) -> Option<Vec<VerticalMetrics>> {
        let vmtx_offset = match self.table_headers.get("vmtx") {
            None => return None,
            Some(vmtx_header) => vmtx_header.offset,
        };
        let mut cursor = vmtx_offset;
        let mut glyph_metrics = vec![];
        for _ in 0..glyph_count {
            let glyph_metric =
                VerticalMetrics::from_in_place_buf(self.read_with_cursor(&mut cursor));
            glyph_metrics.push(glyph_metric);
        }
        Some(glyph_metrics)
    }
}

#[derive(Debug, Copy, Clone)]
enum CoordinateComponentType {
    X,
    Y,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum GlyphOutlineFlag {
    OnCurve,
    ShortX,
    ShortY,
    Repeat,
    SameX,
    SameY,
}

fn get_flags_from_byte(byte: u8) -> Vec<GlyphOutlineFlag> {
    // §Table 16
    let mut out = vec![];
    if byte & (1 << 0) != 0 {
        out.push(GlyphOutlineFlag::OnCurve);
    }
    if byte & (1 << 1) != 0 {
        out.push(GlyphOutlineFlag::ShortX);
    }
    if byte & (1 << 2) != 0 {
        out.push(GlyphOutlineFlag::ShortY);
    }
    if byte & (1 << 3) != 0 {
        out.push(GlyphOutlineFlag::Repeat);
    }
    if byte & (1 << 4) != 0 {
        out.push(GlyphOutlineFlag::SameX);
    }
    if byte & (1 << 5) != 0 {
        out.push(GlyphOutlineFlag::SameY);
    }
    out
}
