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
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/kbman/kbman.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/vesa/vesa.h>
#include "map2.h"

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

void rexle() {
	if (!fork("rexle")) {
		rexle_int();
		_kill();
	}
}

int rexle_int() {
	become_first_responder();

	//switch graphics modes
	Screen* screen = switch_to_vesa(0x112, true);
	//Screen* screen = switch_to_vga();
	Size screen_size = screen->window->frame.size;

	//initialize textures
	array_m* textures = array_m_create(16);
	Bmp* bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "wood.bmp");
	array_m_insert(textures, bmp);
	bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "bluestone.bmp");
	array_m_insert(textures, bmp);
	bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "colorstone.bmp");
	array_m_insert(textures, bmp);
	bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "redbrick.bmp");
	array_m_insert(textures, bmp);
	bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "eagle.bmp");
	array_m_insert(textures, bmp);
	bmp = load_bmp(rect_make(point_make(0, 0), size_make(100, 100)), "mossy.bmp");
	array_m_insert(textures, bmp);

	//FPS counter
	Label* fps = create_label(rect_make(point_make(3, 3), size_make(300, 50)), "FPS Counter");
	fps->text_color = color_white();
	add_sublabel(screen->window->content_view, fps);

	//Memory usage tracker
	Label* mem = create_label(rect_make(point_make(screen->window->size.width - 200, 3), size_make(200, 50)), "Memory tracker");
	mem->text_color = color_white();
	add_sublabel(screen->window->content_view, mem);

	double timestamp = 0; //current frame timestamp
	double time_prev = 0; //prev frame timestamp

	Vec2d pos = vec2d(22.0, 12.0); //starting position
	Vec2d dir = vec2d(-1.01, 0.01); //direction vector
	Vec2d plane = vec2d(0.0, 0.66); //2d raycaster version of camera plane

	bool running = 1;
	while (running) {
		for (int x = 0; x < screen_size.width; x++) {
			//ray position + distance
			double cam_x = 2 * x / (double)screen_size.width - 1; //x in camera space
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
			int line_h = (int)(screen_size.height / perp_wall_dist);
			//find lowest and heighest pixel to fill on stripe
			int start = -line_h / 2 + screen_size.height / 2;
			start = MAX(start, 0);
			int end = line_h / 2 + screen_size.height / 2;
			if (end >= screen_size.height) end = screen_size.height - 1;

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
				int d = y * 256 - screen_size.height * 128 + line_h * 128;
				int tex_y = ((d * tex_height) / line_h) / 256;

				//we have x and y, find color at this point in texture
				Coordinate tex_px = point_make(tex_x % tex_width, tex_y % tex_height);
				Color col = *((Color *)&(tex->layer->raw[tex_px.y * tex_width + tex_px.x]));

				//swap BGR
				uint8_t tmp = col.val[0];
				col.val[0] = col.val[2];
				col.val[2] = tmp;

				//make color darker if far side
				if (side) {
					col.val[0] /= 2;
					col.val[1] /= 2;
					col.val[2] /= 2;
				}

				putpixel(screen, x, y, col);
			}

			//draw ceiling above this ray
			Line ceiling = line_make(point_make(x, 0), point_make(x, start));
			draw_line(screen, ceiling, color_make(130, 40, 100), 1);

			//draw floor below the ray
			Line floor = line_make(point_make(x, end), point_make(x, screen_size.height));
			draw_line(screen, floor, color_make(135, 150, 200), 1);
		}

		//timing
		time_prev = timestamp;
		timestamp = time();
		double frame_time = (timestamp - time_prev) / 1000.0;

		//speed modifiers
		double move_speed = frame_time * 5.0; //squares/sec
		double rot_speed = frame_time * 3.0; //rads/sec

		//move forward if not blocked by wall
		if (key_down(KEY_UP)) {
			if (world[(int)(pos.x + dir.x * move_speed)][(int)pos.y] == WALL_NONE) {
				pos.x += dir.x * move_speed;
			}
			if (world[(int)pos.x][(int)(pos.y + dir.y * move_speed)] == WALL_NONE) {
				pos.y += dir.y * move_speed;
			}
		}
		//move backwards if not blocked by wall
		if (key_down(KEY_DOWN)) {
			if (world[(int)(pos.x - dir.x * move_speed)][(int)pos.y] == WALL_NONE) {
				pos.x -= dir.x * move_speed;
			}
			if (world[(int)pos.x][(int)(pos.y - dir.y * move_speed)] == WALL_NONE) {
				pos.y -= dir.y * move_speed;
			}
		}
		//rotate right
		if (key_down(KEY_RIGHT)) {
			//camera and plane must both be rotated
			double old_dir_x = dir.x;
			dir.x = dir.x * cos(-rot_speed) - dir.y * sin(-rot_speed);
			dir.y = old_dir_x * sin(-rot_speed) + dir.y * cos(-rot_speed);

			double old_plane_x = plane.x;
			plane.x = plane.x * cos(-rot_speed) - plane.y * sin(-rot_speed);
			plane.y = old_plane_x * sin(-rot_speed) + plane.y * cos(-rot_speed);
		}
		//rotate left
		if (key_down(KEY_LEFT)) {
			//camera and plane must both be rotated
			double old_dir_x = dir.x;
			dir.x = dir.x * cos(rot_speed) - dir.y * sin(rot_speed);
			dir.y = old_dir_x * sin(rot_speed) + dir.y * cos(rot_speed);

			double old_plane_x = plane.x;
			plane.x = plane.x * cos(rot_speed) - plane.y * sin(rot_speed);
			plane.y = old_plane_x * sin(rot_speed) + plane.y * cos(rot_speed);
		}

		char buf[32];
		double fps_time = 1 / frame_time;
		itoa(frame_time * 1000000, &buf);
		strcat(buf, " ns/frame");
		fps->text = buf;
		draw_label(screen, fps);

		char mem_buf[32];
		itoa(used_mem(), &mem_buf);
		strcat(mem_buf, " bytes in use");
		mem->text = mem_buf;
		draw_label(screen, mem);

		write_screen(screen);

		char ch = kgetch();
		if (ch == 'q') {
			running = 0;
			break;
		}
	}
	gfx_teardown(screen);
	switch_to_text();
	resign_first_responder();
}
