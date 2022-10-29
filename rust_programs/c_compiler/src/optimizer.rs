use alloc::vec::Vec;
use compilation_definitions::instructions::Instr;
use compilation_definitions::prelude::RegView;
use derive_more::Constructor;
use itertools::Itertools;

#[derive(PartialEq)]
enum OptimizerPassResult {
    WorkCompleted,
    NothingToDo,
}

pub struct Optimizer;

impl Optimizer {
    fn run_pass(instrs: &Vec<Instr>) -> (OptimizerPassResult, Vec<Instr>) {
        // Peephole optimizer
        // Eliminate "push <reg>, pop <reg>" sequences
        let mut result = OptimizerPassResult::NothingToDo;
        let mut optimized_instrs = vec![];
        let mut pairwise_iter = instrs.iter().tuple_windows();
        while let Some((instr1, instr2)) = pairwise_iter.next() {
            if let Instr::PushFromReg(RegView(reg1, access_type1)) = instr1 {
                if let Instr::PopIntoReg(RegView(reg2, access_type2)) = instr2 {
                    if reg1 == reg2 {
                        // Omit both instructions
                        pairwise_iter.next();
                        // And note that we've done some work
                        result = OptimizerPassResult::WorkCompleted;
                        continue;
                    }
                }
            }
            optimized_instrs.push(instr1.clone());
        }
        // tuple_windows() won't iterate over the last instruction, so we need to append it manually
        optimized_instrs.push(instrs.last().unwrap().clone());
        (result, optimized_instrs)
    }

    pub fn optimize(instrs: &Vec<Instr>) -> Vec<Instr> {
        let mut optimized_instrs = instrs.clone();
        loop {
            let tup = Optimizer::run_pass(&optimized_instrs);
            let result = tup.0;
            optimized_instrs = tup.1;
            if result == OptimizerPassResult::NothingToDo {
                break;
            }
        }
        Vec::from(optimized_instrs)
    }
}

mod test {
    use crate::optimizer::Optimizer;
    use compilation_definitions::instructions::{Instr, MoveRegToReg};
    use compilation_definitions::prelude::*;

    #[test]
    fn test_remove_push_pop() {
        // Given an instruction sequence with a useless push-pop embedded
        let instrs = vec![
            Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
            Instr::PushFromReg(RegView::rcx()),
            Instr::PopIntoReg(RegView::rcx()),
            Instr::Return,
        ];
        // When I optimize the instruction stream
        let optimized_instrs = Optimizer::optimize(&instrs);
        // Then the push <reg>, pop <reg> pattern is omitted
        assert_eq!(
            optimized_instrs,
            vec![
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
                Instr::Return,
            ]
        )
    }

    #[test]
    fn test_keep_push_pop_with_different_registers() {
        // Given an instruction sequence with a push-pop embedded
        // But the push and pop refer to different registers
        let instrs = vec![
            Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
            Instr::PushFromReg(RegView::rsp()),
            Instr::PopIntoReg(RegView::rbp()),
            Instr::Return,
        ];
        // When I optimize the instruction stream
        let optimized_instrs = Optimizer::optimize(&instrs);
        // Then the push <reg>, pop <reg> pattern is kept
        assert_eq!(
            optimized_instrs,
            vec![
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
                Instr::PushFromReg(RegView::rsp()),
                Instr::PopIntoReg(RegView::rbp()),
                Instr::Return,
            ]
        )
    }

    #[test]
    fn test_remove_push_pop_recursively() {
        // Given an instruction sequence with nested push-pop sequences embedded
        let instrs = vec![
            Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
            Instr::PushFromReg(RegView::rdx()),
            Instr::PushFromReg(RegView::rsp()),
            Instr::PushFromReg(RegView::rbp()),
            Instr::PushFromReg(RegView::rax()),
            Instr::PushFromReg(RegView::rcx()),
            Instr::PopIntoReg(RegView::rcx()),
            Instr::PopIntoReg(RegView::rax()),
            Instr::PopIntoReg(RegView::rbp()),
            Instr::PopIntoReg(RegView::rsp()),
            Instr::PopIntoReg(RegView::rcx()),
            Instr::Return,
        ];
        // When I optimize the instruction stream
        let optimized_instrs = Optimizer::optimize(&instrs);
        // Then the recursive push <reg, pop <reg> patterns are removed
        assert_eq!(
            optimized_instrs,
            vec![
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rax(), RegView::rbx())),
                Instr::PushFromReg(RegView::rdx()),
                Instr::PopIntoReg(RegView::rcx()),
                Instr::Return,
            ]
        )
    }
}
