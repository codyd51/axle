#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use crate::{amc_message_await, amc_message_send, copy_str_into_sized_slice, AmcMessage};
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {}

use crate::core_commands::conditional_imports::*;

use crate::{ContainsEventField, ExpectsEventField};
use alloc::vec::Vec;
use axle_rt_derive::ContainsEventField;
use cstr_core::CString;

pub const AMC_CORE_SERVICE_NAME: &str = "com.axle.core";
const AMC_MAX_SERVICE_NAME_LEN: usize = 64;

#[derive(Debug)]
pub struct PhysVirtPair {
    pub phys: usize,
    pub virt: usize,
}

#[cfg(target_os = "axle")]
impl PhysVirtPair {
    fn new(phys: usize, virt: usize) -> Self {
        Self { phys, virt }
    }
}

#[derive(Debug)]
pub struct PhysRangeMapping {
    pub addr: PhysVirtPair,
    pub size: usize,
}

#[cfg(target_os = "axle")]
impl PhysRangeMapping {
    fn new(addr: PhysVirtPair, size: usize) -> Self {
        Self { addr, size }
    }
}

#[cfg(target_os = "axle")]
impl Drop for PhysRangeMapping {
    fn drop(&mut self) {
        amc_free_physical_range(self.addr.virt, self.size);
    }
}

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

#[cfg(target_os = "axle")]
pub fn amc_alloc_physical_range(size: usize) -> PhysRangeMapping {
    let req = AmcAllocPhysicalRangeRequest::new(size);
    amc_message_send(AMC_CORE_SERVICE_NAME, req);
    let resp: AmcMessage<AmcAllocPhysicalRangeResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
    let addr = PhysVirtPair::new(resp.body().phys_base, resp.body().virt_base);
    PhysRangeMapping::new(addr, size)
}

/* Free virtual memory mapping */

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcFreePhysicalRangeRequest {
    event: u32,
    vaddr: usize,
    size: usize,
}

impl AmcFreePhysicalRangeRequest {
    pub fn new(vaddr: usize, size: usize) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            vaddr,
            size,
        }
    }
}

impl ExpectsEventField for AmcFreePhysicalRangeRequest {
    const EXPECTED_EVENT: u32 = 214;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcFreePhysicalRangeResponse {
    event: u32,
}

// Never constructed directly, sent from amc
impl AmcFreePhysicalRangeResponse {}

impl ExpectsEventField for AmcFreePhysicalRangeResponse {
    const EXPECTED_EVENT: u32 = 214;
}

#[cfg(target_os = "axle")]
pub fn amc_free_physical_range(vaddr: usize, size: usize) {
    let req = AmcFreePhysicalRangeRequest::new(vaddr, size);
    amc_message_send(AMC_CORE_SERVICE_NAME, req);
    let resp: AmcMessage<AmcFreePhysicalRangeResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcQueryServiceRequest {
    pub event: u32,
    pub remote_service_name: [u8; AMC_MAX_SERVICE_NAME_LEN],
}

impl ExpectsEventField for AmcQueryServiceRequest {
    const EXPECTED_EVENT: u32 = 211;
}

impl AmcQueryServiceRequest {
    #[cfg(target_os = "axle")]
    pub fn send(remote_service_name: &str) -> AmcQueryServiceResponse {
        let mut name_buf = [0; AMC_MAX_SERVICE_NAME_LEN];
        let _name_len = copy_str_into_sized_slice(&mut name_buf, remote_service_name);
        let msg = Self {
            event: Self::EXPECTED_EVENT,
            remote_service_name: name_buf,
        };
        amc_message_send(AMC_CORE_SERVICE_NAME, msg);
        // Await the response
        let resp: AmcMessage<AmcQueryServiceResponse> =
            crate::amc_message_await__u32_event(AMC_CORE_SERVICE_NAME);
        *resp.body
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone, ContainsEventField)]
pub struct AmcQueryServiceResponse {
    pub event: u32,
    pub remote_service_name: [u8; AMC_MAX_SERVICE_NAME_LEN],
    pub service_exists: bool,
}

impl ExpectsEventField for AmcQueryServiceResponse {
    const EXPECTED_EVENT: u32 = 211;
}

// Start/control processes
#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcExecBuffer {
    event: u32,
    program_name: *const u8,
    with_supervisor: bool,
    buffer_addr: *const u8,
    buffer_size: u32,
}

impl AmcExecBuffer {
    pub fn from(program_name: &str, buf: &Vec<u8>, with_supervisor: bool) -> Self {
        let buffer_addr = buf.as_ptr();
        // TODO(PT): Change the C API to accept a char array instead of char pointer
        let c_str = CString::new(program_name).unwrap();
        let program_name_ptr = c_str.as_ptr() as *const u8;
        AmcExecBuffer {
            event: Self::EXPECTED_EVENT,
            program_name: program_name_ptr,
            with_supervisor,
            buffer_addr,
            buffer_size: buf.len() as _,
        }
    }
}

impl ExpectsEventField for AmcExecBuffer {
    const EXPECTED_EVENT: u32 = 204;
}

#[repr(C)]
#[derive(Debug)]
pub enum SupervisedProcessEvent {
    ProcessCreate(u64),
    ProcessStart(u64, u64),
    ProcessExit(u64, u64),
    ProcessWrite(u64, u64, [u8; 128]),
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcSupervisedProcessEventMsg {
    event: u32,
    pub supervised_process_event: SupervisedProcessEvent,
}

impl ExpectsEventField for AmcSupervisedProcessEventMsg {
    const EXPECTED_EVENT: u32 = 215;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcAwmMapFramebuffer {
    event: u32,
}

impl AmcAwmMapFramebuffer {
    pub fn new() -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
        }
    }
}

impl ExpectsEventField for AmcAwmMapFramebuffer {
    const EXPECTED_EVENT: u32 = 201;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcAwmMapFramebufferResponse {
    event: u32,
    pub fb_type: u32,
    pub address: usize,
    pub width: u32,
    pub height: u32,
    pub bits_per_pixel: u32,
    pub bytes_per_pixel: u32,
    pub pixels_per_scanline: u32,
    pub size: u32,
}

impl ExpectsEventField for AmcAwmMapFramebufferResponse {
    const EXPECTED_EVENT: u32 = 201;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcSharedMemoryCreateRequest {
    event: u32,
    remote_service_name: [u8; AMC_MAX_SERVICE_NAME_LEN],
    buffer_size: u32,
}

impl AmcSharedMemoryCreateRequest {
    #[cfg(target_os = "axle")]
    pub fn send(remote_service_name: &str, buffer_size: u32) -> AmcSharedMemoryCreateResponse {
        let mut name_buf = [0; AMC_MAX_SERVICE_NAME_LEN];
        let _name_len = copy_str_into_sized_slice(&mut name_buf, remote_service_name);
        let msg = Self {
            event: Self::EXPECTED_EVENT,
            remote_service_name: name_buf,
            buffer_size,
        };
        amc_message_send(AMC_CORE_SERVICE_NAME, msg);
        // Await the response
        let resp: AmcMessage<AmcSharedMemoryCreateResponse> =
            crate::amc_message_await__u32_event(AMC_CORE_SERVICE_NAME);
        *resp.body
    }
}

impl ExpectsEventField for AmcSharedMemoryCreateRequest {
    const EXPECTED_EVENT: u32 = 210;
}

#[repr(C)]
#[derive(Debug, ContainsEventField, Copy, Clone)]
pub struct AmcSharedMemoryCreateResponse {
    event: u32,
    pub local_buffer_start: usize,
    pub remote_buffer_start: usize,
}

impl ExpectsEventField for AmcSharedMemoryCreateResponse {
    const EXPECTED_EVENT: u32 = 210;
}

#[repr(C)]
#[derive(Debug, ContainsEventField, Copy, Clone)]
pub struct AmcSleepUntilDelayOrMessage {
    event: u32,
    sleep_for_ms: u32,
}

impl AmcSleepUntilDelayOrMessage {
    pub fn new(sleep_for_ms: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            sleep_for_ms,
        }
    }
}

impl ExpectsEventField for AmcSleepUntilDelayOrMessage {
    const EXPECTED_EVENT: u32 = 207;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcRegisterServiceDiedNotif {
    event: u32,
    remote_service_name: [u8; AMC_MAX_SERVICE_NAME_LEN],
}

impl AmcRegisterServiceDiedNotif {
    #[cfg(target_os = "axle")]
    pub fn send(service_name: &str) {
        let mut name_buf = [0; AMC_MAX_SERVICE_NAME_LEN];
        let _name_len = copy_str_into_sized_slice(&mut name_buf, service_name);
        let msg = Self {
            event: Self::EXPECTED_EVENT,
            remote_service_name: name_buf,
        };
        amc_message_send(AMC_CORE_SERVICE_NAME, msg);
    }
}

impl ExpectsEventField for AmcRegisterServiceDiedNotif {
    const EXPECTED_EVENT: u32 = 208;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AmcServiceDiedNotif {
    event: u32,
    pub dead_service: [u8; AMC_MAX_SERVICE_NAME_LEN],
}

impl ExpectsEventField for AmcServiceDiedNotif {
    const EXPECTED_EVENT: u32 = 208;
}

/* End of event modeling */
