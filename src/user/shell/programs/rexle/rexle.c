#include "rexle.h"
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/lib/shapes.h>
#include <gfx/lib/rect.h>
#include <stdint.h>
#include <std/std.h>
#include <std/math.h>
#include <std/sincostan.h>
#include <std/array_m.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/util/kbman/kbman.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/kb/kb.h>
#include "map1.h"

typedef enum {
	WALL_NONE = 0,
	WALL_1,
	WALL_2,
	WALL_3,
	WALL_4,
	WALL_5,
	WALL_RED 	= 6,
	WALL_ORANGE = 7,
	WALL_YELLOW = 8,
	WALL_GREEN	= 9,
	WALL_BLUE	= 10,
	WALL_PURPLE	= 11,
	WALL_GRAY 	= 12,
	WALL_BLACK	= 13,
	WALL_WHITE	= 14,
} WALL_TYPE;

extern void draw_label(ca_layer* dest, Label* label);
void rexle_int();

enum {
	MODE_VESA,
	MODE_VGA,
};

void rexle(int argc, char** argv) {
	int mode = MODE_VESA;
	if (argc > 1) {
		if (!strcmp(argv[1], "vga")) {
			mode = MODE_VGA;
		}
	}

	if (!fork("rexle")) {
		rexle_int(mode);
		_kill();
	}
}

void rexle_int(int mode) {
	//switch graphics modes
	Screen* screen = gfx_screen();
	Size viewport_size = size_make(screen->resolution.width / 2.5, screen->resolution.height / 2.5);
	Point viewport_origin = point_make((screen->resolution.width / 2) - (viewport_size.width / 2), (screen->resolution.height / 2) - (viewport_size.height / 2));
	Rect viewport_rect = rect_make(viewport_origin, viewport_size);

	become_first_responder();

	//initialize textures
	array_m* textures = array_m_create(8);
	if (mode == MODE_VESA) {
		char files[6][32 + 1] = {	"mossy.bmp",

									"bluestone.bmp",
									"colorstone.bmp",
									"redbrick.bmp",
									"eagle.bmp",
									"wood.bmp",
								};
		for (int i = 0; i < 6; i++) {
			Bmp* bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), files[i]);
			if (bmp) {
				array_m_insert(textures, bmp);
			}
		}
	}
	else if (mode == MODE_VGA || !textures->size) {
		ca_layer* layer = create_layer(size_make(100, 100));
		for (int y = 0; y < layer->size.height; y++) {
			for (int x = 0; x < layer->size.width; x++) {
				int idx = (y * layer->size.width) + x * gfx_bpp();
				if (mode == MODE_VGA) {
					if (x == y) {
						layer->raw[idx] = 0x30;
					}
					else if (x == (100 - y)) {
						layer->raw[idx] = 0x60;
					}
					else {
						layer->raw[idx] = 0x40;
					}
				}
				else {
					layer->raw[idx + 0] = x % 255;
					layer->raw[idx + 1] = y % 255;
					layer->raw[idx + 2] = (x * y) % 255;
				}
			}
		}
		Bmp* bmp = create_bmp(rect_make(point_zero(), layer->size), layer);
		array_m_insert(textures, bmp);
	}

	//FPS counter
	Label* fps = create_label(rect_make(point_make(rect_min_x(viewport_rect) + 3, rect_min_y(viewport_rect) + 3), size_make(100, 15)), "FPS Counter");
	fps->text_color = color_black();
	//add_sublabel(screen->window->content_view, fps);

	double timestamp = 0; //current frame timestamp
	double time_prev = 0; //prev frame timestamp

	Vec2d pos = vec2d(22.0, 12.0); //starting position
	Vec2d dir = vec2d(-1.01, 0.01); //direction vector
	Vec2d plane = vec2d(0.0, 0.66); //2d raycaster version of camera plane

	bool running = 1;
	while (running) {
		for (int x = 0; x < viewport_size.width; x++) {
			int real_x = x + rect_min_x(viewport_rect);

			//ray position + distance
			double cam_x = 2 * x / (double)viewport_size.width - 1; //x in camera space
			Vec2d ray_pos = vec2d(pos.x, pos.y);
			Vec2d ray_dir = vec2d(dir.x + plane.x * cam_x,
					      dir.y + plane.y * cam_x);
			//current position in grid
			Vec2d map_pos = vec2d((int)(ray_pos.x), (int)(ray_pos.y));

			//length from current pos to next side
			Vec2d side_dist;

			//length from one side to next
			Vec2d delta_dist = vec2d(sqrt(1 + (ray_dir.y * ray_dir.y) / (ray_dir.x * ray_dir.x)),
					         sqrt(1 + (ray_dir.x * ray_dir.x) / (ray_dir.y * ray_dir.y)));

			//direction to step on each axis (+1/-1)
			Vec2d step;

			int hit = 0; //wall hit?
			int side; //NS or EW wall?

			//calculate step and initial side_dist
			if (ray_dir.x < 0) {
				step.x = -1;
				side_dist.x = (ray_pos.x - map_pos.x) * delta_dist.x;
			}
			else {
				step.x = 1;
				side_dist.x = (map_pos.x + 1.0 - ray_pos.x) * delta_dist.x;
			}
			if (ray_dir.y < 0) {
				step.y = -1;
				side_dist.y = (ray_pos.y - map_pos.y) * delta_dist.y;
			}
			else {
				step.y = 1;
				side_dist.y = (map_pos.y + 1.0 - ray_pos.y) * delta_dist.y;
			}

			//DDA
			while (!hit) {
				//jump to next map square, OR in each direction
				if (side_dist.x < side_dist.y) {
					side_dist.x += delta_dist.x;
					map_pos.x += step.x;
					side = 0;
				}
				else {
					side_dist.y += delta_dist.y;
					map_pos.y += step.y;
					side = 1;
				}
				//did we hit a wall?
				if (world[(int)map_pos.x][(int)map_pos.y]) hit = 1;
			}

			//calculate distance projected on camera direction
			double perp_wall_dist;
			if (!side) perp_wall_dist = (map_pos.x - ray_pos.x + (1 - step.x) / 2) / ray_dir.x;
			else 	   perp_wall_dist = (map_pos.y - ray_pos.y + (1 - step.y) / 2) / ray_dir.y;

			//height of line to draw
			int line_h = (int)(viewport_size.height / perp_wall_dist);
			//find lowest and heighest pixel to fill on stripe
			int start = -line_h / 2 + viewport_size.height / 2;
			start = MAX(start, 0);
			int end = line_h / 2 + viewport_size.height / 2;
			if (end >= viewport_size.height) end = viewport_size.height - 1;

			//texture rendering
			int tex_idx = world[(int)map_pos.x][(int)map_pos.y] - 1;
			Bmp* tex = (Bmp*)array_m_lookup(textures, tex_idx % textures->size);
			int tex_width = tex->layer->size.width;
			int tex_height = tex->layer->size.height;

			//calculate where wall was hit
			double wall_x;
			if (!side) wall_x = ray_pos.y + perp_wall_dist * ray_dir.y;
			else wall_x = ray_pos.x + perp_wall_dist * ray_dir.x;
			wall_x -= floor(wall_x);

			//x coordinate on texture
			int tex_x = (int)(wall_x * (double)tex_width);
			if (!side && ray_dir.x > 0) tex_x = tex_width - tex_x - 1;
			if (side && ray_dir.y < 0) tex_x = tex_width - tex_x - 1;

			//this texture was not taking up the majority of the screen
			//we must now do perspective calculations on every pixel in this vertical line of the texture
			for (int y = start; y < end; y++) {
				int real_y = y + rect_min_y(viewport_rect);

				int d = y * 256 - viewport_size.height * 128 + line_h * 128;
				int tex_y = ((d * tex_height) / line_h) / 256;

				//we have x and y, find color at this point in texture
				Point tex_px = point_make(tex_x % tex_width, tex_y % tex_height);

				uint8_t* raw = (uint8_t*)(tex->layer->raw + (tex_px.y * tex_width * gfx_bpp()) + (tex_px.x * gfx_bpp()));
				Color col;
				col.val[0] = *raw++;
				if (mode == MODE_VESA) {
					col.val[1] = *raw++;
					col.val[2] = *raw++;

					//swap BGR
					uint8_t tmp = col.val[0];
					col.val[0] = col.val[2];
					col.val[2] = tmp;
				}

				//make color darker if far side
				if (side) {
					col.val[0] /= 2;
					if (mode == MODE_VESA) {
						col.val[1] /= 2;
						col.val[2] /= 2;
					}
				}
				putpixel(screen->vmem, real_x, real_y, col);
			}

			int y_off = rect_min_y(viewport_rect);
			//draw ceiling above this ray
			Line ceiling = line_make(point_make(real_x, y_off), point_make(real_x, y_off + start));
			draw_line(screen->vmem, ceiling, color_make(130, 40, 100), 1);

			//draw floor below the ray
			Line floor = line_make(point_make(real_x, y_off + end), point_make(real_x, y_off + viewport_size.height));
			draw_line(screen->vmem, floor, color_make(135, 150, 200), 1);
		}

		//timing
		time_prev = timestamp;
		timestamp = time();
		double frame_time = (timestamp - time_prev) / 1000.0;

		//speed modifiers
		double move_speed = frame_time * 5.0; //squares/sec
		double rot_speed = frame_time * 3.0; //rads/sec

		//move forward if not blocked by wall
		if (key_down('w')) {
			if (world[(int)(pos.x + dir.x * move_speed)][(int)pos.y] == WALL_NONE) {
				pos.x += dir.x * move_speed;
			}
			if (world[(int)pos.x][(int)(pos.y + dir.y * move_speed)] == WALL_NONE) {
				pos.y += dir.y * move_speed;
			}
		}
		//move backwards if not blocked by wall
		if (key_down('s')) {
			if (world[(int)(pos.x - dir.x * move_speed)][(int)pos.y] == WALL_NONE) {
				pos.x -= dir.x * move_speed;
			}
			if (world[(int)pos.x][(int)(pos.y - dir.y * move_speed)] == WALL_NONE) {
				pos.y -= dir.y * move_speed;
			}
		}
		//rotate right
		if (key_down('d')) {
			//camera and plane must both be rotated
			double old_dir_x = dir.x;
			dir.x = dir.x * cos(-rot_speed) - dir.y * sin(-rot_speed);
			dir.y = old_dir_x * sin(-rot_speed) + dir.y * cos(-rot_speed);

			double old_plane_x = plane.x;
			plane.x = plane.x * cos(-rot_speed) - plane.y * sin(-rot_speed);
			plane.y = old_plane_x * sin(-rot_speed) + plane.y * cos(-rot_speed);
		}
		//rotate left
		if (key_down('a')) {
			//camera and plane must both be rotated
			double old_dir_x = dir.x;
			dir.x = dir.x * cos(rot_speed) - dir.y * sin(rot_speed);
			dir.y = old_dir_x * sin(rot_speed) + dir.y * cos(rot_speed);

			double old_plane_x = plane.x;
			plane.x = plane.x * cos(rot_speed) - plane.y * sin(rot_speed);
			plane.y = old_plane_x * sin(rot_speed) + plane.y * cos(rot_speed);
		}

		int real_fps = 1 / frame_time;
		char buf[32];
		itoa(real_fps, (char*)&buf);
		strcat(buf, " FPS");
		fps->text = buf;
		draw_label(screen->vmem, fps);

		write_screen(screen);

		if (key_down('q')) {
			running = 0;
		}

		//eat keypresses
		while (haskey()) kgetch();
	}

	//cleanup
	//free textures
	for (int i = 0; i < textures->size; i++) {
		Bmp* bmp = array_m_lookup(textures, i);
		printf_dbg("freeing bmp [%d]%x", i, bmp);
		bmp_teardown(bmp);
	}
	gfx_teardown(screen);

	resign_first_responder();
}
