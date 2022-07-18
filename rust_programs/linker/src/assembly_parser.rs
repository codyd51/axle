use crate::assembly_lexer::{AssemblyLexer, Token};

pub struct AssemblyParser {
    lexer: AssemblyLexer,
}

impl AssemblyParser {
    pub fn new(lexer: AssemblyLexer) -> Self {
        Self { lexer }
    }

    pub fn parse_statement(&mut self) -> Option<()> {
        let token = self.lexer.next_token()?;

        match token {
            Token::Dot => {
                // Parse a directive
                let directive_name_token = self.lexer.next_token().unwrap();
                let directive_name = match directive_name_token {
                    Token::Identifier(name) => name,
                    _ => panic!("Expected an identifier"),
                };
                println!("Got directive .{directive_name}");
            }
            _ => (),
        }
        Some(())
    }

    pub fn parse(&mut self) {
        let mut statements = vec![];
        loop {
            if let Some(statement) = self.parse_statement() {
                statements.push(statement);
            } else {
                break;
            }
        }
        /*
        loop {
            let token = self.lexer.next_token();
            match token {
                Some(_) => println!("{:?}", token.unwrap()),
                None => break,
            }
        }
        */
    }
}
