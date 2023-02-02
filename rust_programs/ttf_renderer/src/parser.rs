use agx_definitions::{Point, Rect, Size};
use num_traits::PrimInt;
use std::collections::BTreeMap;
use std::fmt::{Display, Formatter};
use std::mem;
use std::ops::Index;

trait TransmuteFontBufInPlace {}

fn fixed_word_to_i32(fixed: u32) -> i32 {
    fixed as i32 / (1 << 16)
}

#[derive(Debug, Copy, Clone)]
struct BigEndianValue<T: PrimInt>(T);
impl<T: PrimInt> TransmuteFontBufInPlace for BigEndianValue<T> {}

struct WrappedValue<T: PrimInt>(T);

impl<T: PrimInt> BigEndianValue<T> {
    fn into_value(self) -> T {
        let wrapped_value: WrappedValue<T> = self.into();
        wrapped_value.0
    }
}

impl<T: PrimInt> From<BigEndianValue<T>> for WrappedValue<T> {
    fn from(value: BigEndianValue<T>) -> WrappedValue<T> {
        WrappedValue {
            // TODO(PT): Only swap to LE if we're not running on BE
            0: value.0.swap_bytes() as T,
        }
    }
}

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
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
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
        assert_eq!(flags, 0b1001, "Other flags unhandled for now");

        let directionality = match raw.font_direction_hint.into_value() {
            0 => FontDirectionality::MixedDirection,
            1 => FontDirectionality::LeftToRight,
            2 => FontDirectionality::LeftToRightAndNeutral,
            -1 => FontDirectionality::RightToLeft,
            -2 => FontDirectionality::RightToLeftAndNeutral,
            _ => panic!("Invalid font direction hint"),
        };
        assert_eq!(
            directionality,
            FontDirectionality::LeftToRightAndNeutral,
            "Only left-to-right-and-neutral is handled for now"
        );

        let index_to_loc_format = match raw.index_to_loc_format.into_value() {
            0 => IndexToLocFormat::ShortOffsets,
            1 => IndexToLocFormat::LongOffsets,
            _ => panic!("Invalid index_to_loc_format"),
        };
        assert_eq!(
            index_to_loc_format,
            IndexToLocFormat::ShortOffsets,
            "Only short offsets are handled for now"
        );

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
        assert!(
            contour_count >= 1,
            "Other contour counts are unhandled for now"
        );

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

trait FromFontBufInPlace<T> {
    fn from_in_place_buf(raw: &T) -> Self;
}

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

    pub fn parse(&mut self) {
        let mut cursor = 0;
        let offset_subtable = OffsetSubtable::from_in_place_buf(self.read_with_cursor(&mut cursor));
        println!("Got offset subtable {offset_subtable:?}",);

        for i in 0..offset_subtable.num_tables {
            let table = TableHeader::new(self.read_with_cursor(&mut cursor));
            println!("Table #{i}: {table}");
            self.table_headers.insert(table.tag, table);
        }
        self.head = Some(self.parse_table("head"));
        println!("Found head: {:?}", self.head);

        let max_profile: MaxProfile = self.parse_table("maxp");
        println!("Got max profile {max_profile:?}");

        for i in 0..max_profile.num_glyphs {
            println!("\tGlyph offset of glyph #{i}: {}", self.get_glyph_offset(i));
        }

        /*
        let glyph_header = self.tables.get("glyf").unwrap();
        println!("Found glyph header: {glyph_header}");

        let glyph_description =
            GlyphDescription::new(self.read(glyph_header.offset + self.get_glyph_offset(2)));
        println!("Found glyph description {glyph_description:?}");
        let mut glyph_data_cursor = glyph_header.offset + mem::size_of::<GlyphDescription>();
        println!("Parsing glyph...");
        for i in 0..glyph_description.contour_count {
            let contour_end: &BigEndianValue<u16> = self.read_with_cursor(&mut glyph_data_cursor);
            println!("\tParsed contour #{i}: {}", contour_end.into_value());
        }

        // Skip instructions
        let instructions_len: &BigEndianValue<u16> = self.read_with_cursor(&mut glyph_data_cursor);
        println!(
            "\tSkipping instructions len: {}",
            instructions_len.into_value()
        );
        glyph_data_cursor += instructions_len.into_value() as usize;
        */
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
            _ => todo!(),
        }
    }
}
