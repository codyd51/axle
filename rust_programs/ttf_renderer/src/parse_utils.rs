use core::fmt::{Debug, Display, Formatter};
use num_traits::PrimInt;

pub fn fixed_word_to_i32(fixed: u32) -> i32 {
    fixed as i32 / (1 << 16)
}

pub trait TransmuteFontBufInPlace {}

impl TransmuteFontBufInPlace for u8 {}

pub trait FromFontBufInPlace<T> {
    fn from_in_place_buf(raw: &T) -> Self;
}

impl<T: PrimInt> FromFontBufInPlace<BigEndianValue<T>> for T {
    fn from_in_place_buf(raw: &BigEndianValue<T>) -> T {
        raw.into_value()
    }
}

#[derive(Copy, Clone)]
pub struct BigEndianValue<T: PrimInt>(T);

impl<T: PrimInt + Display> Display for BigEndianValue<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "BEValue(le={})", self.into_value())
    }
}

impl<T: PrimInt + Display> Debug for BigEndianValue<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        Display::fmt(self, f)
    }
}

impl<T: PrimInt> TransmuteFontBufInPlace for BigEndianValue<T> {}

struct WrappedValue<T: PrimInt>(T);

impl<T: PrimInt> BigEndianValue<T> {
    pub(crate) fn into_value(self) -> T {
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
