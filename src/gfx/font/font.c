#include "font.h"
#include <std/std.h>
#include <gfx/lib/gfx.h>

Font* setup_font() {
	Font* font_map = kmalloc(sizeof(Font));
	memset(font_map->characters, 0, FONT8_SIZE);

	uint8_t a_vals[] = {0x18, 0x3C, 0x66, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3};
	memcpy(&(font_map->characters[8*'A']), a_vals, sizeof(a_vals));
	memcpy(&(font_map->characters[8*'a']), a_vals, sizeof(a_vals));

	uint8_t b_vals[] = {0xfc, 0xc6, 0xcc, 0xfe, 0xc6, 0xc3, 0xc6, 0xfc};
	memcpy(&(font_map->characters[8*'B']), b_vals, sizeof(b_vals));
	memcpy(&(font_map->characters[8*'b']), b_vals, sizeof(b_vals));

	uint8_t c_vals[] = {0x3c, 0x66, 0xc3, 0xc0, 0xc0, 0xc3, 0x66, 0x3c};
	memcpy(&(font_map->characters[8*'C']), c_vals, sizeof(c_vals));
	memcpy(&(font_map->characters[8*'c']), c_vals, sizeof(c_vals));

	uint8_t d_vals[] = {0xfc, 0xc6, 0xc3, 0xc3, 0xc3, 0xc3, 0xc6, 0xfc};
	memcpy(&(font_map->characters[8*'D']), d_vals, sizeof(d_vals));
	memcpy(&(font_map->characters[8*'d']), d_vals, sizeof(d_vals));

	uint8_t e_vals[] = {0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF};
	memcpy(&(font_map->characters[8*'E']), e_vals, sizeof(e_vals));
	memcpy(&(font_map->characters[8*'e']), e_vals, sizeof(e_vals));

	uint8_t f_vals[] = {0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0};
	memcpy(&(font_map->characters[8*'F']), f_vals, sizeof(f_vals));
	memcpy(&(font_map->characters[8*'f']), f_vals, sizeof(f_vals));

	uint8_t g_vals[] = {0x3C, 0x66, 0xC3, 0xC0, 0xCF, 0x63, 0x7E, 0x3C};
	memcpy(&(font_map->characters[8*'G']), g_vals, sizeof(g_vals));
	memcpy(&(font_map->characters[8*'g']), g_vals, sizeof(g_vals));
	
	uint8_t h_vals[] = {0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3};
	memcpy(&(font_map->characters[8*'H']), h_vals, sizeof(h_vals));
	memcpy(&(font_map->characters[8*'h']), h_vals, sizeof(h_vals));
	
	uint8_t i_vals[] = {0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF};
	memcpy(&(font_map->characters[8*'I']), i_vals, sizeof(i_vals));
	memcpy(&(font_map->characters[8*'i']), i_vals, sizeof(i_vals));
	
	uint8_t j_vals[] = {0xFF, 0xFF, 0x0C, 0x0C, 0x0C, 0xCC, 0x7C, 0x38};
	memcpy(&(font_map->characters[8*'J']), j_vals, sizeof(j_vals));
	memcpy(&(font_map->characters[8*'j']), j_vals, sizeof(j_vals));
	
	uint8_t k_vals[] = {0xC6, 0xCC, 0xD8, 0xF0, 0xD8, 0xCC, 0xC6, 0xC3};
	memcpy(&(font_map->characters[8*'K']), k_vals, sizeof(k_vals));
	memcpy(&(font_map->characters[8*'k']), k_vals, sizeof(k_vals));
	
	uint8_t l_vals[] = {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF};
	memcpy(&(font_map->characters[8*'L']), l_vals, sizeof(l_vals));
	memcpy(&(font_map->characters[8*'l']), l_vals, sizeof(l_vals));
	
	uint8_t m_vals[] = {0xC3, 0xE7, 0xDB, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
	memcpy(&(font_map->characters[8*'M']), m_vals, sizeof(m_vals));
	memcpy(&(font_map->characters[8*'m']), m_vals, sizeof(m_vals));
	
	uint8_t n_vals[] = {0xC3, 0xE3, 0xD3, 0xD3, 0xCB, 0xCB, 0xC7, 0xC3};
	memcpy(&(font_map->characters[8*'N']), n_vals, sizeof(n_vals));
	memcpy(&(font_map->characters[8*'n']), n_vals, sizeof(n_vals));
	
	uint8_t o_vals[] = {0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C};
	memcpy(&(font_map->characters[8*'O']), o_vals, sizeof(o_vals));
	memcpy(&(font_map->characters[8*'o']), o_vals, sizeof(o_vals));
	
	uint8_t p_vals[] = {0xFC, 0xC6, 0xC3, 0xC6, 0xFC, 0xC0, 0xC0, 0xC0};
	memcpy(&(font_map->characters[8*'P']), p_vals, sizeof(p_vals));
	memcpy(&(font_map->characters[8*'p']), p_vals, sizeof(p_vals));
	
	uint8_t q_vals[] = {0x18, 0x3C, 0x66, 0xC3, 0xDB, 0x6D, 0x3D, 0x1B};
	memcpy(&(font_map->characters[8*'Q']), q_vals, sizeof(q_vals));
	memcpy(&(font_map->characters[8*'q']), q_vals, sizeof(q_vals));
	
	uint8_t r_vals[] = {0xFC, 0xC6, 0xC6, 0xFC, 0xD8, 0xCC, 0xC6, 0xC3};
	memcpy(&(font_map->characters[8*'R']), r_vals, sizeof(r_vals));
	memcpy(&(font_map->characters[8*'r']), r_vals, sizeof(r_vals));
		
	uint8_t s_vals[] = {0x3C, 0x62, 0x40, 0x78, 0x1E, 0x02, 0x46, 0x3C};
	memcpy(&(font_map->characters[8*'S']), s_vals, sizeof(s_vals));
	memcpy(&(font_map->characters[8*'s']), s_vals, sizeof(s_vals));
	
	uint8_t t_vals[] = {0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};
	memcpy(&(font_map->characters[8*'T']), t_vals, sizeof(t_vals));
	memcpy(&(font_map->characters[8*'t']), t_vals, sizeof(t_vals));
	
	uint8_t u_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x66, 0x7E, 0x18};
	memcpy(&(font_map->characters[8*'U']), u_vals, sizeof(u_vals));
	memcpy(&(font_map->characters[8*'u']), u_vals, sizeof(u_vals));
	
	uint8_t v_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x66, 0x3C, 0x18};
	memcpy(&(font_map->characters[8*'V']), v_vals, sizeof(v_vals));
	memcpy(&(font_map->characters[8*'v']), v_vals, sizeof(v_vals));
	
	uint8_t w_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xDB, 0xE7, 0xC3};
	memcpy(&(font_map->characters[8*'W']), w_vals, sizeof(w_vals));
	memcpy(&(font_map->characters[8*'w']), w_vals, sizeof(w_vals));

	uint8_t x_vals[] = {0xC3, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0xC3};
	memcpy(&(font_map->characters[8*'X']), x_vals, sizeof(x_vals));
	memcpy(&(font_map->characters[8*'x']), x_vals, sizeof(x_vals));

	uint8_t y_vals[] = {0xC3, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18};
	memcpy(&(font_map->characters[8*'Y']), y_vals, sizeof(y_vals));
	memcpy(&(font_map->characters[8*'y']), y_vals, sizeof(y_vals));

	uint8_t z_vals[] = {0xFF, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xFF};
	memcpy(&(font_map->characters[8*'Z']), z_vals, sizeof(z_vals));
	memcpy(&(font_map->characters[8*'z']), z_vals, sizeof(z_vals));

	uint8_t space_vals[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	memcpy(&(font_map->characters[8*' ']), space_vals, sizeof(space_vals));

	return font_map;
}

void draw_char(Screen* screen, Font* font_map, char ch, int x, int y, int color) {
	uint8_t* bitmap = &(font_map->characters[8*ch]);
	for (int i = 0; i < 8; i++) {
		uint8_t row = bitmap[i];
		for (int j = 0; j < 8; j++) {
			if ((row >> (7 - j)) & 1) {
				putpixel(screen, x + j, y + i, color);
			}
		}
	}
}


