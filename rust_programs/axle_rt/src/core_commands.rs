#[cfg(target_os = "axle")]
use crate::{amc_message_await, amc_message_send};

use crate::{AmcMessage, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

const AMC_CORE_SERVICE_NAME: &str = "com.axle.core";

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcMapPhysicalRangeRequest {
    event: u32,
    phys_base: usize,
    size: usize,
}

impl AmcMapPhysicalRangeRequest {
    pub fn new(phys_base: usize, size: usize) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            phys_base,
            size,
        }
    }
}

impl ExpectsEventField for AmcMapPhysicalRangeRequest {
    const EXPECTED_EVENT: u32 = 212;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcMapPhysicalRangeResponse {
    event: u32,
    virt_base: usize,
}

// Never constructed directly, sent from amc
impl AmcMapPhysicalRangeResponse {}

impl ExpectsEventField for AmcMapPhysicalRangeResponse {
    const EXPECTED_EVENT: u32 = 212;
}

#[cfg(target_os = "axle")]
pub fn amc_map_physical_range(phys_base: usize, size: usize) -> usize {
    let req = AmcMapPhysicalRangeRequest::new(phys_base, size);
    amc_message_send(AMC_CORE_SERVICE_NAME, req);
    let resp: AmcMessage<AmcMapPhysicalRangeResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
    resp.body().virt_base
}
