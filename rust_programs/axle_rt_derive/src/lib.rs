extern crate proc_macro;

use proc_macro::TokenStream;
use quote::quote;
use syn;

#[proc_macro_derive(ContainsEventField)]
pub fn contains_event_field_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_contains_event_field(&ast)
}

fn impl_contains_event_field(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let gen = quote! {
        impl ContainsEventField for #name {
            fn event(&self) -> u32 {
                self.event
            }
        }
    };
    gen.into()
}
