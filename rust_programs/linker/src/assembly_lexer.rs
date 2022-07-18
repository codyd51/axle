#[derive(Debug, PartialEq)]
pub enum Token {
    Colon,
    Comma,
    Dollar,
    Dot,
    Hash,
    LeftParen,
    Percent,
    RightParen,
    Identifier(String),
}

pub struct AssemblyLexer {
    raw_text: Vec<char>,
    cursor: usize,
}

impl AssemblyLexer {
    pub fn new(raw_text: &str) -> Self {
        Self {
            raw_text: raw_text.chars().collect(),
            cursor: 0,
        }
    }

    fn next_char(&mut self) -> Option<char> {
        let ret = self.peek_char()?;
        self.cursor += 1;
        Some(ret)
    }

    fn peek_char(&mut self) -> Option<char> {
        if self.cursor < self.raw_text.len() {
            return Some(self.raw_text[self.cursor]);
        }
        None
    }

    pub fn next_token(&mut self) -> Option<Token> {
        // Skip over any whitespace
        loop {
            if !self.peek_char()?.is_whitespace() {
                break;
            }
            self.next_char()?;
        }

        let start_cursor = self.cursor;
        loop {
            if self.peek_char()?.is_whitespace() {
                break;
            }

            let maybe_single_character_token = match self.peek_char()? {
                '.' => Some(Token::Dot),
                '$' => Some(Token::Dollar),
                '#' => Some(Token::Hash),
                '%' => Some(Token::Percent),
                ',' => Some(Token::Comma),
                '(' => Some(Token::LeftParen),
                ')' => Some(Token::RightParen),
                ':' => Some(Token::Colon),
                _ => None,
            };
            if let Some(token) = maybe_single_character_token {
                // Are we already reading a different token?
                if self.cursor != start_cursor {
                    // Delimit the token here
                    break;
                }

                // If this is the start of a comment, eat everything on this line
                if token == Token::Hash {
                    while self.next_char()? != '\n' {}
                    // Return the next token after the newline
                    return self.next_token();
                }

                // Eat the single-character token
                self.next_char();
                return Some(token);
            }
            // Part of a multi-character token
            self.next_char();
        }
        // Non-single-character token
        let token_name = self.raw_text[start_cursor..self.cursor].to_vec();

        match token_name.iter().collect::<String>().as_str() {
            _ => Some(Token::Identifier(token_name.into_iter().collect())),
        }
    }
}

#[test]
fn test_lexer() {
    let source = "
.section .text
_start:
    mov $0xc, %rax		# _write syscall vector
	mov $0x1, %rbx		# File descriptor (ignored in axle, typically stdout)";
    let mut lexer = AssemblyLexer::new(source);
    let mut tokens = vec![];
    loop {
        if let Some(token) = lexer.next_token() {
            tokens.push(token);
        } else {
            break;
        }
    }
    assert_eq!(
        tokens,
        vec![
            Token::Dot,
            Token::Identifier("section".to_string()),
            Token::Dot,
            Token::Identifier("text".to_string()),
            Token::Identifier("_start".to_string()),
            Token::Colon,
            Token::Identifier("mov".to_string()),
            Token::Dollar,
            Token::Identifier("0xc".to_string()),
            Token::Comma,
            Token::Percent,
            Token::Identifier("rax".to_string()),
            Token::Identifier("mov".to_string()),
            Token::Dollar,
            Token::Identifier("0x1".to_string()),
            Token::Comma,
            Token::Percent,
            Token::Identifier("rbx".to_string()),
        ]
    );
}
