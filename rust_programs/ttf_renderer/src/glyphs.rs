use crate::metrics::{GlyphRenderMetrics, LongHorMetric, VerticalMetrics};
use crate::parse_utils::{BigEndianValue, FromFontBufInPlace, TransmuteFontBufInPlace};
use crate::parser::FontParser;
use crate::GlyphMetrics;
use agx_definitions::{Point, PointF64, Polygon, Rect, Size};
use alloc::borrow::ToOwned;
use alloc::vec;
use alloc::vec::Vec;
use itertools::Itertools;

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

struct GlyphOutlineFlags(Vec<GlyphOutlineFlag>);

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
pub enum GlyphRenderInstructions {
    PolygonsGlyph(PolygonsGlyphRenderInstructions),
    BlankGlyph(BlankGlyphRenderInstructions),
    CompoundGlyph(CompoundGlyphRenderInstructions),
}

#[derive(Debug, Clone)]
pub struct GlyphRenderDescription {
    pub(crate) render_metrics: GlyphRenderMetrics,
    pub render_instructions: GlyphRenderInstructions,
}

impl GlyphRenderDescription {
    pub(crate) fn polygons_glyph(glyph_bounding_box: &Rect, polygons: &Vec<Polygon>) -> Self {
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

    pub(crate) fn compound_glyph(glyph_bounding_box: &Rect) -> Self {
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
pub(crate) struct GlyphDescription {
    contour_count: isize,
    bounding_box: Rect,
}

impl FromFontBufInPlace<GlyphDescriptionRaw> for GlyphDescription {
    fn from_in_place_buf(raw: &GlyphDescriptionRaw) -> Self {
        let contour_count = raw.contour_count.into_value() as isize;
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

impl From<u8> for GlyphOutlineFlags {
    fn from(byte: u8) -> Self {
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
        GlyphOutlineFlags(out)
    }
}

fn get_glyph_offset_and_length(parser: &FontParser, glyph_index: usize) -> (usize, usize) {
    // Length must be inferred from the offset of the next glyph
    let glyph_offset = parser.get_glyph_offset(glyph_index);
    let next_glyph_offset = parser.get_glyph_offset(glyph_index + 1);
    (glyph_offset, next_glyph_offset - glyph_offset)
}

pub(crate) fn parse_glyph(
    parser: &FontParser,
    glyph_index: usize,
    all_glyphs_bounding_box: &Rect,
) -> GlyphRenderDescription {
    let glyph_header = parser.table_headers.get("glyf").unwrap();
    let (glyph_local_offset, glyph_data_length) = get_glyph_offset_and_length(parser, glyph_index);
    let glyph_offset = glyph_header.offset + glyph_local_offset;
    // Empty glyph?
    if glyph_data_length == 0 {
        return GlyphRenderDescription::blank_glyph(&all_glyphs_bounding_box);
    }

    //println!("GlyphIndex({glyph_index}) offset = {glyph_offset}");
    let mut cursor = glyph_offset;
    let glyph_description =
        GlyphDescription::from_in_place_buf(parser.read_with_cursor(&mut cursor));

    // Handle compound glyphs
    // Hack to make compound glyphs work
    if glyph_description.contour_count <= 0 {
        //println!("Compound glyph!");
        return GlyphRenderDescription::compound_glyph(&all_glyphs_bounding_box);
    }

    // This informs how many points there are in total
    let mut last_point_indexes = vec![];
    for _ in 0..glyph_description.contour_count {
        let contour_end = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        last_point_indexes.push(contour_end as isize);
    }
    let max_point_idx = last_point_indexes.iter().max().unwrap();
    let point_count = max_point_idx + 1;

    // Skip instructions
    let instructions_len = parser
        .read_with_cursor::<BigEndianValue<u16>>(&mut cursor)
        .into_value();
    //println!("\tSkipping instructions len: {instructions_len}");
    cursor += instructions_len as usize;

    let mut all_flags = vec![];
    let mut flag_count_to_parse = point_count as usize;
    while flag_count_to_parse > 0 {
        let flag_byte: u8 = *parser.read_with_cursor(&mut cursor);
        let flags = GlyphOutlineFlags::from(flag_byte).0;
        if flags.contains(&GlyphOutlineFlag::Repeat) {
            let mut flags = flags.clone();
            flags.retain(|&f| f != GlyphOutlineFlag::Repeat);
            let repeat_count: u8 = *parser.read_with_cursor(&mut cursor);
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
        interpret_values_via_flags(parser, &mut cursor, &all_flags, CoordinateComponentType::X);
    let y_values =
        interpret_values_via_flags(parser, &mut cursor, &all_flags, CoordinateComponentType::Y);
    let points: Vec<PointF64> = x_values
        .iter()
        .zip(y_values.iter())
        // Flip the Y axis of every point to match our coordinate system
        .map(|(&x, &y)| PointF64::new(x as _, (all_glyphs_bounding_box.max_y() - y) as _))
        //.map(|(&x, &y)| Point::new(x, y))
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
    parser: &FontParser,
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
            let value_without_sign = *parser.read_with_cursor::<u8>(cursor) as isize;
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
                let value = parser
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
