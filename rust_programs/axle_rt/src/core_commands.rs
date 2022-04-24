#[cfg(target_os = "axle")]
use crate::{amc_message_await, amc_message_send};

use crate::{AmcMessage, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

const AMC_CORE_SERVICE_NAME: &str = "com.axle.core";

/* Map physical range */

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

/* Alloc virtual memory mapping */

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcAllocPhysicalRangeRequest {
    event: u32,
    size: usize,
}

impl AmcAllocPhysicalRangeRequest {
    pub fn new(size: usize) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            size,
        }
    }
}

impl ExpectsEventField for AmcAllocPhysicalRangeRequest {
    const EXPECTED_EVENT: u32 = 213;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcAllocPhysicalRangeResponse {
    event: u32,
    phys_base: usize,
    virt_base: usize,
}

// Never constructed directly, sent from amc
impl AmcAllocPhysicalRangeResponse {}

impl ExpectsEventField for AmcAllocPhysicalRangeResponse {
    const EXPECTED_EVENT: u32 = 213;
}

pub struct PhysVirtPair {
    pub phys: usize,
    pub virt: usize,
}

impl PhysVirtPair {
    fn new(phys: usize, virt: usize) -> Self {
        Self { phys, virt }
    }
}

#[cfg(target_os = "axle")]
pub fn amc_alloc_physical_range(size: usize) -> PhysVirtPair {
    let req = AmcAllocPhysicalRangeRequest::new(size);
    amc_message_send(AMC_CORE_SERVICE_NAME, req);
    let resp: AmcMessage<AmcAllocPhysicalRangeResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
    PhysVirtPair::new(resp.body().phys_base, resp.body().virt_base)
}
