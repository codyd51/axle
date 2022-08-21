use alloc::collections::BTreeMap;
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};

use crate::println;

#[derive(Debug, PartialEq, Clone)]
pub enum Token {
    Comma,
    CurlyBraceLeft,
    CurlyBraceRight,
    Dot,
    Float(f64),
    Identifier(String),
    Int(usize),
    Minus,
    ParenLeft,
    ParenRight,
    Percent,
    Quote,
    Semicolon,
}

pub struct Lexer {
    raw_text: Vec<char>,
    cursor: usize,
}

impl Lexer {
    pub fn new(raw_text: &str) -> Self {
        Self {
            raw_text: raw_text.chars().collect(),
            cursor: 0,
        }
    }

    pub fn reset(&mut self) {
        self.cursor = 0;
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

    fn digit_chars_to_value(digits: &Vec<char>) -> usize {
        digits.iter().fold(0, |acc, digit_ch| {
            (acc * 10) + (digit_ch.to_digit(10).unwrap() as usize)
        })
    }

    fn read_digits_to_delimiter(&mut self) -> (usize, String) {
        let mut digits = vec![];
        loop {
            if let Some(peek) = self.peek_char() {
                if peek.is_digit(10) {
                    digits.push(self.next_char().unwrap());
                } else {
                    // Non-digit character, we're done parsing a number
                    break;
                }
            } else {
                // Ran out of characters in the input stream
                break;
            }
        }
        let digits_as_value = Self::digit_chars_to_value(&digits);
        (digits_as_value, digits.iter().collect())
    }

    pub fn next_token(&mut self) -> Option<Token> {
        // Skip over any whitespace
        loop {
            if !self.peek_char()?.is_whitespace() {
                break;
            }
            self.next_char()?;
        }

        let first_char = self.peek_char()?;
        if first_char.is_digit(10) {
            // This may be an int or a float. Lex to the next terminator.
            let digits = self.read_digits_to_delimiter();
            if self.peek_char() == Some('.') {
                // We're parsing a float and need to parse the portion to the right of the decimal
                // Consume the decimal point
                self.match_char('.');
                let digits_before_decimal = digits;
                let digits_after_decimal = self.read_digits_to_delimiter();
                println!("Found float {digits_before_decimal:?}, {digits_after_decimal:?}");

                let value_before_decimal = digits_before_decimal.0 as f64;
                let value_after_decimal = digits_after_decimal.0 as f64;
                // Ref: https://stackoverflow.com/questions/68818046/how-can-i-turn-a-integral
                let float_value = format!("{value_before_decimal}.{value_after_decimal}")
                    .parse()
                    .unwrap();
                println!("{value_before_decimal} {value_after_decimal} {float_value}");
                return Some(Token::Float(float_value));
            }

            // We're parsing an int
            return Some(Token::Int(digits.0));
        }

        // TODO(PT): Handle comments here, before looking at single-character tokens

        // Is this a single-character token?
        let single_character_tokens = BTreeMap::from([
            ('%', Token::Percent),
            (',', Token::Comma),
            ('"', Token::Quote),
            ('-', Token::Minus),
            ('(', Token::ParenLeft),
            (')', Token::ParenRight),
            ('{', Token::CurlyBraceLeft),
            ('}', Token::CurlyBraceRight),
            (';', Token::Semicolon),
        ]);
        if let Some(token) = single_character_tokens.get(&first_char) {
            // Consume the character
            self.match_char(first_char);
            return Some(token.clone());
        }

        // We're parsing an identifier - consume characters until we hit a delimiter
        let mut identifier_chars = vec![];
        loop {
            let next_char = self.peek_char();
            if let Some(next_char) = next_char {
                let is_delimiter =
                    next_char.is_whitespace() || single_character_tokens.get(&next_char).is_some();
                if is_delimiter {
                    break;
                }
                identifier_chars.push(self.next_char().unwrap());
            } else {
                // Ran out of characters in the input stream
                break;
            }
        }

        Some(Token::Identifier(identifier_chars.iter().collect()))
    }

    pub fn peek_token(&mut self) -> Option<Token> {
        let start_cursor = self.cursor;
        let token = self.next_token();
        self.cursor = start_cursor;
        token
    }

    fn match_char(&mut self, expected_ch: char) {
        assert_eq!(self.next_char(), Some(expected_ch))
    }

    pub fn match_token(&mut self, expected_token: Token) -> Token {
        let token = self
            .next_token()
            .expect("Expected {expected_token:?}, but we ran out tokens");
        assert_eq!(token, expected_token);
        token
    }

    pub fn match_identifier(&mut self) -> String {
        let token = self.next_token();
        match token.expect("Expected an identifier, but we ran out of tokens") {
            Token::Identifier(string) => string,
            _ => panic!("Expected an identifier, but got a different token type"),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::lexer::{Lexer, Token};
    use alloc::string::{String, ToString};
    use alloc::vec;

    #[test]
    fn lex_simple() {
        let source = r"
            int _start() {
                return 5;
            }";
        let mut lexer = Lexer::new(source);
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
                Token::Identifier("int".into()),
                Token::Identifier("_start".into()),
                Token::ParenLeft,
                Token::ParenRight,
                Token::CurlyBraceLeft,
                Token::Identifier("return".into()),
                Token::Int(5),
                Token::Semicolon,
                Token::CurlyBraceRight,
            ]
        );
    }

    #[test]
    fn lex_float() {
        let source = "3.14;";
        let mut lexer = Lexer::new(source);
        assert_eq!(lexer.peek_token(), Some(Token::Float(3.14)));
        assert_eq!(lexer.next_token(), Some(Token::Float(3.14)));
        assert_eq!(lexer.peek_token(), Some(Token::Semicolon));
        assert_eq!(lexer.next_token(), Some(Token::Semicolon));
        assert_eq!(
            Lexer::new("5.99999").next_token(),
            Some(Token::Float(5.99999))
        );
    }

    #[test]
    fn lex_int() {
        let source = "3;1;4";
        let mut lexer = Lexer::new(source);
        assert_eq!(lexer.peek_token(), Some(Token::Int(3)));
        assert_eq!(lexer.next_token(), Some(Token::Int(3)));
        assert_eq!(lexer.next_token(), Some(Token::Semicolon));
        assert_eq!(lexer.next_token(), Some(Token::Int(1)));
        assert_eq!(lexer.next_token(), Some(Token::Semicolon));
        assert_eq!(lexer.next_token(), Some(Token::Int(4)));
    }

    #[test]
    fn lex_identifier() {
        let source = "foo";
        let mut lexer = Lexer::new(source);
        assert_eq!(lexer.peek_token(), Some(Token::Identifier("foo".into())));
        assert_eq!(lexer.next_token(), Some(Token::Identifier("foo".into())));
    }

    #[test]
    fn lex_none() {
        let source = "";
        let mut lexer = Lexer::new(source);
        assert_eq!(lexer.peek_token(), None);
        assert_eq!(lexer.next_token(), None);
    }
}
