extern crate alloc;
extern crate libc;

use axle_rt::{ContainsEventField, ExpectsEventField};

// PT: Must match the definitions in the corresponding C header

pub const IDE_SERVICE_NAME: &str = "com.axle.ide";

pub trait IdeEvent: ExpectsEventField + ContainsEventField {}
