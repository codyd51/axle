use alloc::vec::Vec;
use lazy_static::lazy_static;
use spin::Mutex;

lazy_static! {
    // Note that the same IDT is shared across every CPU core
    static ref IDT_FREE_VECTORS: spin::Mutex<Vec<usize>> = Mutex::new(Vec::new());
}

pub fn idt_set_free_vectors(free_vectors: &Vec<usize>) {
    for vec in free_vectors.iter() {
        IDT_FREE_VECTORS.lock().push(*vec);
    }
}

#[no_mangle]
pub fn idt_allocate_vector() -> usize {
    // Pop from the front so the allocations are a bit more intuitive
    IDT_FREE_VECTORS.lock().remove(0)
}
