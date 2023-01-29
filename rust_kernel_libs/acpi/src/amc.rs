use crate::apic::cpu_core_private_info;
use alloc::collections::{BTreeMap, VecDeque};
use alloc::string::ToString;
use alloc::vec::Vec;
use alloc::{format, vec};
use core::ffi::{c_char, CStr};
use ffi_bindings::{cpu_id, println, AmcMessage, AmcService};
use lazy_static::lazy_static;
use spin::Mutex;

lazy_static! {
    static ref SERVICES_TO_INBOXES: spin::Mutex<BTreeMap<&'static AmcService, VecDeque<&'static AmcMessage>>> =
        Mutex::new(BTreeMap::new());
}

unsafe fn track_inbox_for_service_if_necessary(service: &'static AmcService) {
    let mut services_to_inboxes = SERVICES_TO_INBOXES.lock();
    if !services_to_inboxes.contains_key(service) {
        services_to_inboxes.insert(service, VecDeque::new());
    }
}

#[no_mangle]
pub unsafe fn amc_append_message_to_service_inbox(
    service_raw: *const AmcService,
    message: *const AmcMessage,
) {
    // Lifetimes are managed by the C bits of the kernel
    let service: &'static AmcService = &*service_raw;
    track_inbox_for_service_if_necessary(service);

    let mut services_to_inboxes = SERVICES_TO_INBOXES.lock();
    let message: &'static AmcMessage = &*message;
    services_to_inboxes
        .get_mut(service)
        .unwrap()
        .push_back(message);
}

#[no_mangle]
pub unsafe fn amc_select_message_to_deliver(
    service_raw: *const AmcService,
    desired_source_service_count: u32,
    desired_source_services_raw: *const *const u8,
    desired_u32_event_raw: *const u32,
) -> *const AmcMessage {
    let service = &*service_raw;
    track_inbox_for_service_if_necessary(service);
    let mut services_to_inboxes = SERVICES_TO_INBOXES.lock();
    let available_messages = services_to_inboxes.get_mut(service).unwrap();

    let should_match_any_service =
        desired_source_services_raw == core::ptr::null() || desired_source_service_count == 0;
    let should_allow_any_event_field = desired_u32_event_raw == core::ptr::null();

    let desired_source_services_slice = core::ptr::slice_from_raw_parts(
        desired_source_services_raw,
        desired_source_service_count as usize,
    );
    let desired_source_services = &*(desired_source_services_slice as *const [*const u8]);

    // Read messages in FIFO, from the array head to the tail
    let first_message_idx_matching_criteria =
        available_messages.iter().enumerate().find_map(|(i, msg)| {
            let msg_source_as_str = msg.source();

            if !should_match_any_service
                && !desired_source_services.iter().any(|&source_service| {
                    msg_source_as_str
                        == CStr::from_ptr(source_service as *const c_char)
                            .to_str()
                            .unwrap()
                })
            {
                // Source service not satisfactory
                return None;
            }

            if !should_allow_any_event_field {
                let msg_body_slice = core::ptr::slice_from_raw_parts(
                    core::ptr::addr_of!(msg.body),
                    msg.len as usize,
                );
                let msg_body_as_ref = &*(msg_body_slice as *const [u8]);
                let (_, msg_body_as_u32_buf, _) = msg_body_as_ref.align_to::<u32>();
                if msg_body_as_u32_buf[0] != *desired_u32_event_raw {
                    return None;
                }
            }

            // All checks passed, return this message
            Some(i)
        });

    if let Some(first_message_idx_matching_criteria) = first_message_idx_matching_criteria {
        available_messages
            .remove(first_message_idx_matching_criteria)
            .unwrap()
    } else {
        core::ptr::null()
    }
}

#[no_mangle]
pub unsafe fn amc_has_message_from_int(
    this_service_raw: *const AmcService,
    service_name_raw: *const c_char,
) -> bool {
    // Lifetimes are managed by the C bits of the kernel
    let service: &'static AmcService = &*this_service_raw;
    track_inbox_for_service_if_necessary(service);

    let mut services_to_inboxes = SERVICES_TO_INBOXES.lock();
    let available_messages = services_to_inboxes.get_mut(service).unwrap();

    let service_name = CStr::from_ptr(service_name_raw).to_str().unwrap();
    available_messages
        .iter()
        .any(|&msg| msg.source() == service_name)
}

#[no_mangle]
pub unsafe fn amc_service_has_message(service_raw: *const AmcService) -> bool {
    // Lifetimes are managed by the C bits of the kernel
    let service: &'static AmcService = &*service_raw;
    track_inbox_for_service_if_necessary(service);
    SERVICES_TO_INBOXES
        .lock()
        .get(service)
        .expect(&format!("No queue found for {}", service.name()))
        .len()
        > 0
}

#[no_mangle]
pub unsafe fn amc_service_inbox_len(service_raw: *const AmcService) -> usize {
    let service: &'static AmcService = &*service_raw;
    track_inbox_for_service_if_necessary(service);
    SERVICES_TO_INBOXES
        .lock()
        .get(service)
        .expect(&format!("No queue found for {}", service.name()))
        .len()
}
