#include "font.h"
#include <std/std.h>
#include <gfx/lib/gfx.h>

int char_index(char ch) {
	switch (ch) {
		case 'a':
		case 'A':
			return 0;
		case 'b':
		case 'B':
			return 1;	
		case 'c':
		case 'C':
			return 2;
		case 'd':
		case 'D':	
			return 3;
		case 'e':
		case 'E':
			return 4;
		case 'f':
		case 'F':
			return 5;
		case 'g':
		case 'G':
			return 6;
		case 'h':
		case 'H':
			return 7;
		case 'i':
		case 'I':
			return 8;
		case 'j':
		case 'J':
			return 9;
		case 'k':
		case 'K':
			return 10;
		case 'l':
		case 'L':
			return 11;
		case 'm':
		case 'M':
			return 12;
		case 'n':
		case 'N':
			return 13;
		case 'o':
		case 'O':
			return 14;
		case 'p':
		case 'P':
			return 15;
		case 'q':
		case 'Q':
			return 16;
		case 'r':
		case 'R':
			return 17;
		case 's':
		case 'S':
			return 18;
		case 't':
		case 'T':
			return 19;
		case 'u':
		case 'U':
			return 20;
		case 'v':
		case 'V':
			return 21;
		case 'w':
		case 'W':
			return 22;
		case 'x':
		case 'X':
			return 23;
		case 'y':
		case 'Y':
			return 24;
		case 'z':
		case 'Z':
			return 25;
		case ' ':
			return 26;
	}
	return -1;
}

Font* setup_font() {
	Font* font_map = kmalloc(sizeof(Font));
	
	int a_vals[] = {0x18, 0x3C, 0x66, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3};
	char_t* a = kmalloc(sizeof(char_t));
	memcpy(a->rows, a_vals, sizeof(a_vals));
	font_map->characters[0] = a;

	int b_vals[] = {0xFC, 0xC6, 0xCC, 0xFE, 0xC6, 0xC3, 0xC6, 0xFC};
	char_t* b = kmalloc(sizeof(char_t));
	memcpy(b->rows, b_vals, sizeof(b_vals));
	font_map->characters[1] = b;

	int c_vals[] = {0x3C, 0x66, 0xC3, 0xC0, 0xC0, 0xC3, 0x66, 0x3C};
	char_t* c = kmalloc(sizeof(char_t));
	memcpy(c->rows, c_vals, sizeof(c_vals));
	font_map->characters[2] = c;

	int d_vals[] = {0xFC, 0xC6, 0xC3, 0xC3, 0xC3, 0xC3, 0xC6, 0xFC};
	char_t* d = kmalloc(sizeof(char_t));
	memcpy(d->rows, d_vals, sizeof(d_vals));
	font_map->characters[3] = d;

	int e_vals[] = {0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF};
	char_t* e = kmalloc(sizeof(char_t));
	memcpy(e->rows, e_vals, sizeof(e_vals));
	font_map->characters[4] = e;

	int f_vals[] = {0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0};
	char_t* f = kmalloc(sizeof(char_t));
	memcpy(f->rows, f_vals, sizeof(f_vals));
	font_map->characters[5] = f;

	int g_vals[] = {0x3C, 0x66, 0xC3, 0xC0, 0xCF, 0x63, 0x7E, 0x3C};
	char_t* g = kmalloc(sizeof(char_t));
	memcpy(g->rows, g_vals, sizeof(g_vals));
	font_map->characters[6] = g;
	
	int h_vals[] = {0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3};
	char_t* h = kmalloc(sizeof(char_t));
	memcpy(h->rows, h_vals, sizeof(h_vals));
	font_map->characters[7] = h;
	
	int i_vals[] = {0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF};
	char_t* i = kmalloc(sizeof(char_t));
	memcpy(i->rows, i_vals, sizeof(i_vals));
	font_map->characters[8] = i;
	
	int j_vals[] = {0xFF, 0xFF, 0x0C, 0x0C, 0x0C, 0xCC, 0x7C, 0x38};
	char_t* j = kmalloc(sizeof(char_t));
	memcpy(j->rows, j_vals, sizeof(j_vals));
	font_map->characters[9] = j;
	
	int k_vals[] = {0xC6, 0xCC, 0xD8, 0xF0, 0xD8, 0xCC, 0xC6, 0xC3};
	char_t* k = kmalloc(sizeof(char_t));
	memcpy(k->rows, k_vals, sizeof(k_vals));
	font_map->characters[10] = k;
	
	int l_vals[] = {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF};
	char_t* l = kmalloc(sizeof(char_t));
	memcpy(l->rows, l_vals, sizeof(l_vals));
	font_map->characters[11] = l;
	
	int m_vals[] = {0xC3, 0xE7, 0xDB, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
	char_t* m = kmalloc(sizeof(char_t));
	memcpy(m->rows, m_vals, sizeof(m_vals));
	font_map->characters[12] = m;
	
	int n_vals[] = {0xC3, 0xE3, 0xD3, 0xD3, 0xCB, 0xCB, 0xC7, 0xC3};
	char_t* n = kmalloc(sizeof(char_t));
	memcpy(n->rows, n_vals, sizeof(n_vals));
	font_map->characters[13] = n;
	
	int o_vals[] = {0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C};
	char_t* o = kmalloc(sizeof(char_t));
	memcpy(o->rows, o_vals, sizeof(o_vals));
	font_map->characters[14] = o;
	
	int p_vals[] = {0xFC, 0xC6, 0xC3, 0xC6, 0xFC, 0xC0, 0xC0, 0xC0};
	char_t* p = kmalloc(sizeof(char_t));
	memcpy(p->rows, p_vals, sizeof(p_vals));
	font_map->characters[15] = p;
	
	int q_vals[] = {0x18, 0x3C, 0x66, 0xC3, 0xDB, 0x6D, 0x3D, 0x1B};
	char_t* q = kmalloc(sizeof(char_t));
	memcpy(q->rows, q_vals, sizeof(q_vals));
	font_map->characters[16] = q;
	
	int r_vals[] = {0xFC, 0xC6, 0xC6, 0xFC, 0xD8, 0xCC, 0xC6, 0xC3};
	char_t* r = kmalloc(sizeof(char_t));
	memcpy(r->rows, r_vals, sizeof(r_vals));
	font_map->characters[17] = r;
		
	int s_vals[] = {0x3C, 0x62, 0x40, 0x78, 0x1E, 0x02, 0x46, 0x3C};
	char_t* s = kmalloc(sizeof(char_t));
	memcpy(s->rows, s_vals, sizeof(s_vals));
	font_map->characters[18] = s;
	
	int t_vals[] = {0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};
	char_t* t = kmalloc(sizeof(char_t));
	memcpy(t->rows, t_vals, sizeof(t_vals));
	font_map->characters[19] = t;
	
	int u_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x66, 0x7E, 0x18};
	char_t* u = kmalloc(sizeof(char_t));
	memcpy(u->rows, u_vals, sizeof(u_vals));
	font_map->characters[20] = u;
	
	int v_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x66, 0x3C, 0x18};
	char_t* v = kmalloc(sizeof(char_t));
	memcpy(v->rows, v_vals, sizeof(v_vals));
	font_map->characters[21] = v;
	
	int w_vals[] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xDB, 0xE7, 0xC3};
	char_t* w = kmalloc(sizeof(char_t));
	memcpy(w->rows, w_vals, sizeof(w_vals));
	font_map->characters[22] = w;

	int x_vals[] = {0xC3, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0xC3};
	char_t* x = kmalloc(sizeof(char_t));
	memcpy(x->rows, x_vals, sizeof(x_vals));
	font_map->characters[23] = x;

	int y_vals[] = {0xC3, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18};
	char_t* y = kmalloc(sizeof(char_t));
	memcpy(y->rows, y_vals, sizeof(y_vals));
	font_map->characters[24] = y;

	int z_vals[] = {0xFF, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xFF};
	char_t* z = kmalloc(sizeof(char_t));
	memcpy(z->rows, z_vals, sizeof(z_vals));
	font_map->characters[25] = z;

	int space_vals[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	char_t* space = kmalloc(sizeof(char_t));
	memcpy(space->rows, space_vals, sizeof(space_vals));
	font_map->characters[26] = space;

	return font_map;
}

int is_bit_set(int c, int n) {
	static int mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
	return ((c & mask[n]) != 0);
}

void draw_char(Screen* screen, Font* font_map, char ch, int x, int y, int color) {
	int index = char_index(ch);
	char_t* c = font_map->characters[index];
	for (int i = 0; i < 8; i++) {
		int row = c->rows[i];
		for (int j = 0; j < 8; j++) {
			if (is_bit_set(row, j) != 0) {
				putpixel(screen, x + j, y + i, color);
			}
		}
	}
}
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING 2
void draw_string(Screen* screen, Font* font_map, char* str, int x, int y, int color) {
	int idx = 0;
	while (str[idx] != NULL) {
		//go to next line if necessary
		if ((x + CHAR_WIDTH + CHAR_PADDING) >  screen->window->size.width || str[idx] == '\n') {
			x = 0;
			y += CHAR_HEIGHT + CHAR_PADDING;
		}

		draw_char(screen, font_map, str[idx], x, y, color);
		
		x += CHAR_WIDTH + CHAR_PADDING;

		idx++;
	}
}

