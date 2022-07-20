use alloc::{rc::Rc, string::String};
use axle_rt::{amc_message_await, amc_register_service, AmcMessage};

use crate::{
    assembly_packer,
    new_try::{render_elf, FileLayout},
};
use linker_messages::{AssembleSource, AssembledElf};

pub fn main() {
    amc_register_service("com.axle.linker");

    let assemble_request: AmcMessage<AssembleSource> = amc_message_await(None);
    let layout = Rc::new(FileLayout::new(0x400000));
    let source = assemble_request.body().source.iter().map(|c| *c as char).collect::<String>();
    let (data_packer, instruction_packer) = assembly_packer::parse(&layout, &source);
    let elf = render_elf(&layout, &data_packer, &instruction_packer);
    AssembledElf::send(assemble_request.source(), &elf);
}
