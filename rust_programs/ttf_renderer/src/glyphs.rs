use crate::metrics::{GlyphRenderMetrics, LongHorMetric, VerticalMetrics};
use crate::parse_utils::{BigEndianValue, FromFontBufInPlace, TransmuteFontBufInPlace};
use crate::parser::FontParser;
use crate::println;
use crate::GlyphMetrics;
use agx_definitions::{Point, PointF64, Polygon, Rect, Size};
use alloc::borrow::ToOwned;
use alloc::vec;
use alloc::vec::Vec;
use itertools::Itertools;
use num_traits::PrimInt;

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
pub struct CompoundGlyphComponent {
    pub(crate) glyph_index: usize,
    pub(crate) origin: Point,
}

impl CompoundGlyphComponent {
    fn new(glyph_index: usize, origin: Point) -> Self {
        Self {
            glyph_index,
            origin,
        }
    }
}

#[derive(Debug, Clone)]
pub struct CompoundGlyphRenderInstructions {
    pub(crate) children: Vec<CompoundGlyphComponent>,
    pub(crate) use_metrics_from_child_idx: Option<usize>,
}

impl CompoundGlyphRenderInstructions {
    fn new(
        children: Vec<CompoundGlyphComponent>,
        use_metrics_from_child_idx: Option<usize>,
    ) -> Self {
        Self {
            children,
            use_metrics_from_child_idx,
        }
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
    pub(crate) hinting_program_bytes: Option<Vec<u8>>,
}

impl GlyphRenderDescription {
    pub(crate) fn polygons_glyph(
        glyph_bounding_box: &Rect,
        polygons: &Vec<Polygon>,
        hinting_program_bytes: Option<Vec<u8>>,
    ) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::PolygonsGlyph(
                PolygonsGlyphRenderInstructions::new(polygons),
            ),
            hinting_program_bytes,
        }
    }

    fn blank_glyph(glyph_bounding_box: &Rect) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::BlankGlyph(
                BlankGlyphRenderInstructions::new(),
            ),
            hinting_program_bytes: None,
        }
    }

    pub(crate) fn compound_glyph(
        glyph_bounding_box: &Rect,
        render_instructions: CompoundGlyphRenderInstructions,
    ) -> Self {
        Self {
            render_metrics: GlyphRenderMetrics::new(glyph_bounding_box),
            render_instructions: GlyphRenderInstructions::CompoundGlyph(render_instructions),
            hinting_program_bytes: None,
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

struct CompoundGlyphComponentHeaderRaw {
    flags: BigEndianValue<u16>,
    glyph_index: BigEndianValue<u16>,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum CompoundGlyphComponentFlag {
    HeaderValuesAreWords,
    HeaderValuesAreCoordinates,
    RoundCoordinatesToGrid,
    CustomScale,
    MoreComponentsFollow,
    HasDifferentScalesForXAndY,
    HasTwoByTwoTransformation,
    HasInstructions,
    UseMetricsFromThisComponent,
    ComponentsOverlap,
}

struct CompoundGlyphComponentFlags(Vec<CompoundGlyphComponentFlag>);

#[derive(Debug, Clone, Copy)]
enum ArgumentType {
    U8,
    I8,
    U16,
    I16,
}

impl CompoundGlyphComponentFlags {
    fn another_component_follows(&self) -> bool {
        self.0
            .contains(&CompoundGlyphComponentFlag::MoreComponentsFollow)
    }

    fn args_type(&self) -> ArgumentType {
        // > If HeaderValuesAreCoordinates is set, the arguments are signed values.
        // > Otherwise, they are unsigned point numbers.
        let are_args_signed = self
            .0
            .contains(&CompoundGlyphComponentFlag::HeaderValuesAreCoordinates);
        if self
            .0
            .contains(&CompoundGlyphComponentFlag::HeaderValuesAreWords)
        {
            match are_args_signed {
                true => ArgumentType::I16,
                false => ArgumentType::U16,
            }
        } else {
            match are_args_signed {
                true => ArgumentType::I8,
                false => ArgumentType::U8,
            }
        }
    }

    fn compound_should_use_metrics_from_this_component(&self) -> bool {
        self.0
            .contains(&CompoundGlyphComponentFlag::UseMetricsFromThisComponent)
    }

    fn has_simple_scale(&self) -> bool {
        self.0.contains(&CompoundGlyphComponentFlag::CustomScale)
    }

    fn has_different_scales_for_x_and_y(&self) -> bool {
        self.0
            .contains(&CompoundGlyphComponentFlag::HasDifferentScalesForXAndY)
    }

    fn has_two_by_two_transformation(&self) -> bool {
        self.0
            .contains(&CompoundGlyphComponentFlag::HasTwoByTwoTransformation)
    }
}

impl From<u16> for CompoundGlyphComponentFlags {
    fn from(value: u16) -> Self {
        let mut out = vec![];

        if value & (1 << 0) != 0 {
            out.push(CompoundGlyphComponentFlag::HeaderValuesAreWords);
        }
        if value & (1 << 1) != 0 {
            out.push(CompoundGlyphComponentFlag::HeaderValuesAreCoordinates);
        }
        if value & (1 << 2) != 0 {
            out.push(CompoundGlyphComponentFlag::RoundCoordinatesToGrid);
        }
        if value & (1 << 3) != 0 {
            out.push(CompoundGlyphComponentFlag::CustomScale);
        }
        if value & (1 << 5) != 0 {
            out.push(CompoundGlyphComponentFlag::MoreComponentsFollow);
        }
        if value & (1 << 6) != 0 {
            out.push(CompoundGlyphComponentFlag::HasDifferentScalesForXAndY);
        }
        if value & (1 << 7) != 0 {
            out.push(CompoundGlyphComponentFlag::HasTwoByTwoTransformation);
        }
        if value & (1 << 8) != 0 {
            out.push(CompoundGlyphComponentFlag::HasInstructions);
        }
        if value & (1 << 9) != 0 {
            out.push(CompoundGlyphComponentFlag::UseMetricsFromThisComponent);
        }
        if value & (1 << 10) != 0 {
            out.push(CompoundGlyphComponentFlag::ComponentsOverlap);
        }

        CompoundGlyphComponentFlags(out)
    }
}

fn read_value_of_type(parser: &FontParser, cursor: &mut usize, value_type: ArgumentType) -> isize {
    match value_type {
        ArgumentType::U8 => u8::from_in_place_buf(parser.read_with_cursor(cursor)) as isize,
        ArgumentType::I8 => i8::from_in_place_buf(parser.read_with_cursor(cursor)) as isize,
        ArgumentType::U16 => u16::from_in_place_buf(parser.read_with_cursor(cursor)) as isize,
        ArgumentType::I16 => i16::from_in_place_buf(parser.read_with_cursor(cursor)) as isize,
    }
}

fn parse_compound_glyph(parser: &FontParser, mut cursor: usize) -> CompoundGlyphRenderInstructions {
    // One of the components may instruct us to use its metrics, instead of looking for metrics
    // for the compound in the `htmx`/`vmtx` table
    let mut use_metrics_of_child_idx = None;
    let mut children = vec![];
    // We don't know upfront how many component glyphs we'll need to parse. Each component will
    // tell us in its flags whether another component follows.
    loop {
        //println!("Found component");
        let flags = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        let flags = CompoundGlyphComponentFlags::from(flags);
        let glyph_index_of_component = u16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
        //println!("\tGlyph index of component: {glyph_index_of_component}");

        let args_type = flags.args_type();
        //println!("\tGot args type {args_type:?}");
        let x = read_value_of_type(parser, &mut cursor, args_type);
        let y = read_value_of_type(parser, &mut cursor, args_type);
        //println!("\tGot X,Y: {x}, {y}");

        if flags.compound_should_use_metrics_from_this_component() {
            // This flag can only be used in one component
            assert_eq!(use_metrics_of_child_idx, None);
            use_metrics_of_child_idx = Some(children.len());
        }

        if flags.has_simple_scale() {
            let two_dot_14 = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            //println!("Got custom scale {two_dot_14}");
        } else if flags.has_different_scales_for_x_and_y() {
            let x_scale = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            let y_scale = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            //println!("Got X and Y scales {x_scale}, {y_scale}");
        } else if flags.has_two_by_two_transformation() {
            let x_scale = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            let scale01 = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            let scale10 = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            let y_scale = i16::from_in_place_buf(parser.read_with_cursor(&mut cursor));
            //println!("Got two-by-two transformation {x_scale}, {scale01}, {scale10}, {y_scale}");
        }

        // Custom instructions are unsupported for now
        /*
        assert!(
            !flags
                .0
                .contains(&CompoundGlyphComponentFlag::HasInstructions),
            "Unsupported compound glyph component flag"
        );
        */

        children.push(CompoundGlyphComponent::new(
            glyph_index_of_component as _,
            Point::new(x as _, y as _),
        ));

        if !flags.another_component_follows() {
            break;
        }
    }
    CompoundGlyphRenderInstructions::new(children, use_metrics_of_child_idx)
}

pub(crate) fn parse_glyph(
    parser: &FontParser,
    glyph_index: usize,
    all_glyphs_bounding_box: &Rect,
    units_per_em: usize,
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
        let render_instructions = parse_compound_glyph(parser, cursor);
        return GlyphRenderDescription::compound_glyph(
            &all_glyphs_bounding_box,
            render_instructions,
        );
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
    let instructions = parser
        .read_bytes_with_cursor(&mut cursor, instructions_len as usize)
        .to_vec();
    /*
    println!("Got instructions:");
    for byte in instructions.iter() {
        println!("\t{byte:02x}");
    }
    */

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
        .map(|(&x, &y)| PointF64::new(x as _, (units_per_em as isize - y) as _))
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
        polygons.push(Polygon::new(&points[start_idx as usize..=end_idx as usize]))
    }

    /*
    println!(
        "Finished parsing glyph #{glyph_index} with polygon count {}",
        polygons.len()
    );
    */
    GlyphRenderDescription::polygons_glyph(
        &glyph_description.bounding_box,
        &polygons,
        Some(instructions),
    )
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
