use std::fmt::Display;

#[derive(Debug, PartialEq, Clone)]
pub enum SymbolExprOperand {
    OutputCursor,
    StartOfSymbol(String),
}

impl Display for SymbolExprOperand {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            SymbolExprOperand::OutputCursor => write!(f, "Op(.)"),
            SymbolExprOperand::StartOfSymbol(sym_name) => {
                write!(f, "Op(SymStart(\"{}\"))", sym_name)
            }
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum AsmExpr {
    Subtract(SymbolExprOperand, SymbolExprOperand),
}

impl Display for AsmExpr {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            AsmExpr::Subtract(op1, op2) => write!(f, "{op1} - {op2}"),
        }
    }
}
