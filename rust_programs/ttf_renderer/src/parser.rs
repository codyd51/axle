use agx_definitions::{Point, Rect, Size};
use std::collections::BTreeMap;
use std::fmt::{Display, Formatter};
use std::mem;

trait TransmuteFontBufInPlace {}

fn fixed_word_to_i32(fixed: u32) -> i32 {
    fixed as i32 / (1 << 16)
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct OffsetSubtableRaw {
    scalar_type: u32,
    num_tables: u16,
    search_range: u16,
    entry_selector: u16,
    range_shift: u16,
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

impl OffsetSubtable {
    fn new(raw: &OffsetSubtableRaw) -> Self {
        // Swaps BE to LE
        // TODO(PT): Only swap to LE if we're not running on BE
        Self {
            scalar_type: raw.scalar_type.swap_bytes(),
            num_tables: raw.num_tables.swap_bytes(),
            search_range: raw.search_range.swap_bytes(),
            entry_selector: raw.entry_selector.swap_bytes(),
            range_shift: raw.range_shift.swap_bytes(),
        }
    }
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct TableRaw {
    tag: [u8; 4],
    checksum: u32,
    offset: u32,
    length: u32,
}

impl TransmuteFontBufInPlace for TableRaw {}

#[derive(Debug, Copy, Clone)]
struct Table<'a> {
    tag: &'a str,
    checksum: u32,
    offset: u32,
    length: u32,
}

impl<'a> Table<'a> {
    fn new(raw: &'a TableRaw) -> Self {
        let tag_as_str = core::str::from_utf8(&raw.tag).unwrap();
        Self {
            tag: tag_as_str,
            checksum: raw.checksum.swap_bytes(),
            offset: raw.offset.swap_bytes(),
            length: raw.length.swap_bytes(),
        }
    }
}

impl<'a> Display for Table<'a> {
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
    version: u32,
    font_revision: u32,
    checksum_adjustment: u32,
    magic: u32,
    flags: u16,
    units_per_em: u16,
    date_created: u64,
    date_modified: u64,
    min_x: i16,
    min_y: i16,
    max_x: i16,
    max_y: i16,
    mac_style: u16,
    lowest_rec_ppem: u16,
    font_direction_hint: i16,
    index_to_loc_format: i16,
    glyph_data_format: i16,
}

impl TransmuteFontBufInPlace for HeadTableRaw {}

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
    lowest_rec_ppem: u16,
    font_direction_hint: i16,
    index_to_loc_format: i16,
    glyph_data_format: i16,
}

impl HeadTable {
    // PT: Defined by the spec §Table 22
    const MAGIC: u32 = 0x5f0f3cf5;
    fn new(raw: &HeadTableRaw) -> Self {
        let glyph_bounding_box_origin =
            Point::new(raw.min_x.swap_bytes() as _, raw.min_y.swap_bytes() as _);
        println!(
            "Glyph bounding box origin {glyph_bounding_box_origin} max {} {}",
            raw.max_x.swap_bytes(),
            raw.max_y.swap_bytes()
        );
        let ret = Self {
            version: fixed_word_to_i32(raw.version.swap_bytes()),
            font_revision: fixed_word_to_i32(raw.font_revision.swap_bytes()),
            checksum_adjustment: raw.checksum_adjustment.swap_bytes(),
            magic: raw.magic.swap_bytes(),
            flags: raw.flags.swap_bytes(),
            units_per_em: raw.units_per_em.swap_bytes(),
            date_created: raw.date_created.swap_bytes(),
            date_modified: raw.date_modified.swap_bytes(),
            glyph_bounding_box: Rect::from_parts(
                glyph_bounding_box_origin,
                Size::new(
                    isize::abs((raw.max_x.swap_bytes() as isize) - glyph_bounding_box_origin.x),
                    isize::abs((raw.max_y.swap_bytes() as isize) - glyph_bounding_box_origin.y),
                ),
            ),
            mac_style: raw.mac_style.swap_bytes(),
            lowest_rec_ppem: raw.lowest_rec_ppem.swap_bytes(),
            font_direction_hint: raw.font_direction_hint.swap_bytes(),
            index_to_loc_format: raw.index_to_loc_format.swap_bytes(),
            glyph_data_format: raw.glyph_data_format.swap_bytes(),
        };
        assert_eq!(ret.magic, Self::MAGIC);
        ret
    }
}

pub struct FontParser<'a> {
    font_data: &'a [u8],
    cursor: usize,
}

impl<'a> FontParser<'a> {
    pub fn new(font_data: &'a [u8]) -> Self {
        Self {
            font_data,
            cursor: 0,
        }
    }

    fn read_at_offset<T: TransmuteFontBufInPlace>(&self, offset: usize) -> &'a T {
        unsafe {
            let ptr = self.font_data.as_ptr().offset(offset as isize);
            let reference: &'a T = &*{ ptr as *const T };
            reference
        }
    }

    fn read<T: TransmuteFontBufInPlace>(&mut self) -> &'a T {
        let ret = self.read_at_offset(self.cursor);
        self.cursor += mem::size_of::<T>();
        ret
    }

    pub fn parse(&mut self) {
        let offset_subtable = OffsetSubtable::new(self.read());
        println!("Got offset subtable {offset_subtable:?}",);

        let mut tables = BTreeMap::new();
        for i in 0..offset_subtable.num_tables {
            let table = Table::new(self.read());
            println!("Table #{i}: {table}");
            tables.insert(table.tag, table);
        }
        let head_header = tables.get("head").unwrap();
        println!("Found head_header: {head_header}");
        self.cursor = head_header.offset as _;
        let head = HeadTable::new(self.read());
        println!("Found head: {head:?}");
    }
}
