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
    let generics = &ast.generics;
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    let gen = quote! {
        impl #impl_generics NestedLayerSlice for #name #ty_generics #where_clause {
            fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
                self.view.get_parent()
            }

            fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
                self.view.set_parent(parent);
            }

            fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
                self.view.get_slice()
            }

            fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
                self.view.get_slice_for_render()
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
    let generics = &ast.generics;
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

    let gen = quote! {
        impl #impl_generics Drawable for #name #ty_generics #where_clause {
            fn frame(&self) -> Rect {
                self.view.frame()
            }

            fn content_frame(&self) -> Rect {
                Bordered::content_frame(self)
            }

            fn draw(&self) -> Vec<Rect> {
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
    let generics = &ast.generics;
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    let gen = quote! {
        impl #impl_generics Bordered for #name #ty_generics #where_clause {
            fn outer_border_insets(&self) -> RectInsets {
                self.view.outer_border_insets()
            }

            fn inner_border_insets(&self) -> RectInsets {
                self.view.inner_border_insets()
            }

            fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
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
    let generics = &ast.generics;
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    let gen = quote! {
        impl #impl_generics UIElement for #name #ty_generics #where_clause {
            fn handle_mouse_entered(&self) {
                self.view.handle_mouse_entered()
            }

            fn handle_mouse_exited(&self) {
                self.view.handle_mouse_exited()
            }

            fn handle_mouse_moved(&self, mouse_point: Point) {
                self.view.handle_mouse_moved(mouse_point)
            }

            fn handle_mouse_scrolled(&self, mouse_point: Point, delta_z: isize) {
                self.view.handle_mouse_scrolled(mouse_point, delta_z)
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
