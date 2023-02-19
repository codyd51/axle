use crate::{Codepoint, Font, GlyphIndex, GlyphRenderInstructions};
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

use crate::character_map::parse_character_map;
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
use core::ptr::slice_from_raw_parts;
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
        /*
        assert!(
            directionality == FontDirectionality::LeftToRightAndNeutral
                || directionality == FontDirectionality::LeftToRight,
            "Only left-to-right/-and-neutral is handled for now"
        );
        */

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

    pub(crate) fn read<T: TransmuteFontBufInPlace>(&self, offset: usize) -> &'a T {
        unsafe {
            let ptr = self.font_data.as_ptr().offset(offset as isize);
            let reference: &'a T = &*{ ptr as *const T };
            reference
        }
    }

    pub(crate) fn read_data_with_cursor<'buf, T: TransmuteFontBufInPlace + 'buf>(
        data: &'buf [u8],
        cursor: &mut usize,
    ) -> &'buf T {
        unsafe {
            let ptr = data.as_ptr().offset(*cursor as isize);
            let reference: &'buf T = &*{ ptr as *const T };
            *cursor += mem::size_of::<T>();
            reference
        }
    }

    pub(crate) fn read_bytes_from_data_with_cursor<'buf>(
        data: &'buf [u8],
        cursor: &mut usize,
        count: usize,
    ) -> &'buf [u8] {
        unsafe {
            let ptr = data.as_ptr().offset(*cursor as isize);
            let slice = slice_from_raw_parts(ptr, count);
            let reference: &'buf [u8] = &*{ slice as *const [u8] };
            *cursor += count;
            reference
        }
    }

    pub(crate) fn read_with_cursor<T: TransmuteFontBufInPlace>(&self, cursor: &mut usize) -> &'a T {
        Self::read_data_with_cursor(self.font_data, cursor)
    }

    pub(crate) fn read_bytes_with_cursor(&self, cursor: &mut usize, count: usize) -> &'a [u8] {
        Self::read_bytes_from_data_with_cursor(self.font_data, cursor, count)
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

        let glyph_indexes_to_codepoints = parse_character_map(self);

        let mut all_glyphs = vec![];
        let mut codepoints_to_glyph_indexes = BTreeMap::new();
        for i in 0..max_profile.num_glyphs {
            let parsed_glyph = parse_glyph(self, i, &glyph_bounding_box, head.units_per_em as _);
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
        for (i, glyph) in all_glyphs.iter().enumerate() {
            // Compound glyphs may inherit their metrics from one of their children
            if let GlyphRenderInstructions::CompoundGlyph(compound_glyph_instructions) =
                &glyph.render_instructions
            {
                if compound_glyph_instructions
                    .use_metrics_from_child_idx
                    .is_some()
                {
                    // Handled in the loop down below
                    continue;
                }
            }

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
        for (i, glyph) in all_glyphs.iter().enumerate() {
            // Compound glyphs may inherit their metrics from one of their children
            if let GlyphRenderInstructions::CompoundGlyph(compound_glyph_instructions) =
                &glyph.render_instructions
            {
                //println!("** Glyph {i} is compound");
                if let Some(child_idx_to_inherit_metrics_from) =
                    compound_glyph_instructions.use_metrics_from_child_idx
                {
                    //println!("** Glyph {i} uses metrics from child IDX {child_idx_to_inherit_metrics_from}");
                    // TODO(PT): We may need to do this after the first pass...
                    let child =
                        &compound_glyph_instructions.children[child_idx_to_inherit_metrics_from];
                    let child_glyph = all_glyphs.get(child.glyph_index).unwrap();
                    let child_metrics = &child_glyph.render_metrics;
                    /*
                    println!(
                        "\tChild metrics (glyph idx {}) {child_metrics:?}",
                        child.glyph_index
                    );
                    */
                    glyph.render_metrics.set_horizontal_metrics(
                        child_metrics
                            .horizontal_metrics
                            .borrow()
                            .as_ref()
                            .unwrap()
                            .clone(),
                    );
                    glyph.render_metrics.set_vertical_metrics(
                        child_metrics
                            .vertical_metrics
                            .borrow()
                            .as_ref()
                            .unwrap_or(&VerticalMetrics {
                                advance_height: glyph_bounding_box.height() as _,
                                top_side_bearing: 0,
                            })
                            .clone(),
                    );
                }
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
