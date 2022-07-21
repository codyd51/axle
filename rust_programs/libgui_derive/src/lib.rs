extern crate proc_macro;

use proc_macro::TokenStream;
use quote::quote;
use syn;

#[proc_macro_derive(NestedLayerSlice)]
pub fn nested_layer_slice_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_nested_layer_slice_derive(&ast)
}

fn impl_nested_layer_slice_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let gen = quote! {
        impl NestedLayerSlice for #name {
            fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
                self.view.get_parent()
            }

            fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
                self.view.set_parent(parent);
            }

            fn get_slice(&self) -> LayerSlice {
                self.view.get_slice()
            }
        }
    };
    gen.into()
}

#[proc_macro_derive(Drawable)]
pub fn drawable_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_drawable_derive(&ast)
}

fn impl_drawable_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let gen = quote! {
        impl Drawable for #name {
            fn frame(&self) -> Rect {
                self.view.frame()
            }

            fn content_frame(&self) -> Rect {
                Bordered::content_frame(self)
            }

            fn draw(&self) {
                Bordered::draw(self)
            }
        }
    };
    gen.into()
}

#[proc_macro_derive(Bordered)]
pub fn bordered_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_bordered_derive(&ast)
}

fn impl_bordered_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let gen = quote! {
        impl Bordered for #name {
            fn border_insets(&self) -> RectInsets {
                self.view.border_insets()
            }

            fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
                self.view.draw_inner_content(outer_frame, onto);
            }
        }
    };
    gen.into()
}

#[proc_macro_derive(UIElement)]
pub fn ui_element_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_ui_element_derive(&ast)
}

fn impl_ui_element_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    let gen = quote! {
        impl UIElement for #name {
            fn handle_mouse_entered(&self) {
                self.view.handle_mouse_entered()
            }

            fn handle_mouse_exited(&self) {
                self.view.handle_mouse_exited()
            }

            fn handle_mouse_moved(&self, mouse_point: Point) {
                self.view.handle_mouse_moved(mouse_point)
            }

            fn handle_left_click(&self, mouse_point: Point) {
                self.view.handle_left_click(mouse_point)
            }

            fn handle_key_pressed(&self, key: KeyCode) {
                self.view.handle_key_pressed(key)
            }

            fn handle_key_released(&self, key: KeyCode) {
                self.view.handle_key_released(key)
            }

            fn handle_superview_resize(&self, superview_size: Size) {
                self.view.handle_superview_resize(superview_size)
            }

            fn currently_contains_mouse(&self) -> bool {
                self.view.currently_contains_mouse()
            }
        }
    };
    gen.into()
}
