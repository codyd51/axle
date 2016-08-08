#include "rexle.h"
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/lib/shapes.h>
#include <stdint.h>
#include <std/std.h>
#include <std/math.h>
#include <std/sincostan.h>
#include <std/array_m.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/util/kbman/kbman.h>

typedef enum {
	WALL_NONE = 0,
	WALL_1,
	WALL_2,
	WALL_3,
	WALL_4,
	WALL_5,
	WALL_RED 	= 6,
	WALL_ORANGE 	= 7,
	WALL_YELLOW 	= 8,
	WALL_GREEN	= 9,
	WALL_BLUE	= 10,
	WALL_PURPLE	= 11,
	WALL_GRAY 	= 12,
	WALL_BLACK	= 13,
	WALL_WHITE	= 14,
} WALL_TYPE;

#define map_width 24
#define map_height 24
int world[map_width][map_height] = 
{
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,2,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,8,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,9,0,0,0,0,0,1},
  {1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,10,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,11,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,12,0,0,1},
  {1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,13,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,14,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

typedef struct Vec2d {
	double x;
	double y;
} Vec2d;

Vec2d vec2d(double x, float y) {
	Vec2d vec;
	vec.x = x;
	vec.y = y;
	return vec;
}

float wave(float x, float amount) {
	return (sin(x*amount) + 1.) * .5;
}

#define tex_width 64
#define tex_height 64
array_m* sample_tex_create(Size screen_size) {
	array_m* textures = array_m_create(32);
	//initialize raw Color arrays
	for (int i = 0; i < textures->max_size; i++) {
		Color** raw = kmalloc(sizeof(Color*) * tex_width);
		for (int i = 0; i < tex_height; i++) {
			Color* row = kmalloc(sizeof(Color) * tex_width);
			raw[i] = row;
		}
		Bmp* tex = create_bmp(rect_make(point_make(0, 0), size_make(tex_width, tex_height)), raw);
		array_m_insert(textures, tex);
	}
	for (int tex_idx = 0; tex_idx < textures->max_size; tex_idx++) {
		Bmp* bmp = array_m_lookup(textures, tex_idx);
		for (int x = 0; x < tex_width; x++) {
			for (int y = 0; y < tex_height; y++) {
				switch (tex_idx) {
					Vec2d uv = vec2d(x / (double)(tex_width), x / (double)(tex_height));
					case WALL_1: {
						if ((x % 32) == 16 || (y % 16) == 8) {
							bmp->raw[x][y] = color_gray();
						}
						else bmp->raw[x][y] = color_make(220, 40, 40);
					} break;
					case WALL_2: {
						if (x == y || (tex_width - x == y)) {
							bmp->raw[x][y] = color_white();
						}
						else bmp->raw[x][y] = color_make(130, 220, 50);
					} break;
					case WALL_3: {
						bmp->raw[x][y] = color_make(x * (255/(double)tex_width), y * (255/(double)tex_width), 100);
					} break;
					case WALL_4: {
						if ((x % 32) <= 16) {
							if ((y % 32) <= 16) {
								bmp->raw[x][y] = color_purple();
							}
							else bmp->raw[x][y] = color_gray();
						}
						else {
							if ((y % 32) <= 16) {
								bmp->raw[x][y] = color_gray();
							}
							else bmp->raw[x][y] = color_purple();
						}
					} break;
					case WALL_5:
					case WALL_RED:
						bmp->raw[x][y] = color_make(220, 40, 40);
						break;
					case WALL_ORANGE:
						bmp->raw[x][y] = color_make(130, 220, 50);
						break;
					case WALL_YELLOW:
						bmp->raw[x][y] = color_make(230, 170, 100);
						break;
					case WALL_GREEN:
						bmp->raw[x][y] = color_make(130, 50, 220);
						break;
					case WALL_BLUE:
					case WALL_PURPLE:
						bmp->raw[x][y] = color_make(130, 50, 220);
						break;
					default: {
						double xorcolor = (x * 256 / tex_width) ^ (y * 256 / tex_height);
						bmp->raw[x][y] = color_make(xorcolor, xorcolor, xorcolor);
					} break;
				}
			}
		}
	}
	return textures;
}

int rexle() {
	//switch graphics modes
	Screen* screen = switch_to_vesa();
	Size screen_size = screen->window->frame.size;

	//initialize textures
	array_m* textures = sample_tex_create(screen_size);

	//FPS counter
	Label* fps = create_label(rect_make(point_make(3, 3), size_make(300, 50)), "FPS Counter");
	fps->text_color = color_make(2, 0, 0);
	add_sublabel(screen->window->content_view, fps);

	double timestamp = 0; //current frame timestamp
	double time_prev = 0; //prev frame timestamp

	Vec2d pos = vec2d(22, 12); //starting position
	Vec2d dir = vec2d(-1, 0); //direction vector
	Vec2d plane = vec2d(0, 0.66); //2d raycaster version of camera plane

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
			int tex_idx = world[(int)map_pos.x][(int)map_pos.y];
			Bmp* tex = array_m_lookup(textures, tex_idx);
			//calculate where wall was hit
			double wall_x;
			if (!side) wall_x = ray_pos.y + perp_wall_dist * ray_dir.y;
			else wall_x = ray_pos.x + perp_wall_dist * ray_dir.x;
			wall_x -= floor(wall_x);

			//x coordinate on texture
			int tex_x = (int)(wall_x * (double)tex_width);
			if (!side && ray_dir.x > 0) tex_x = tex_width - tex_x - 1;
			if (side && ray_dir.y < 0) tex_x = tex_width - tex_x - 1;

			for (int y = start; y < end; y++) {
				int d = y * 256 - screen_size.height * 128 + line_h * 128;
				int tex_y = ((d * tex_height) / line_h) / 256;

				//we have x and y, find color at this point in texture
				Color col = tex->raw[tex_x % tex_width][tex_y % tex_height];
				//make color darker if far side
				if (side) {
					col.val[0] /= 2;
					col.val[1] /= 2;
					col.val[2] /= 2;
				}	

				putpixel(screen, x, y, col);
			}

			/*
			//wall color
			Color col;
			switch(world[(int)map_pos.x][(int)map_pos.y]) {
				case 1:  col = color_make(220, 40, 40); 		break;
				case 2:  col = color_make(130, 220, 50); 		break;
				case 3:  col = color_make(230, 170, 100);	 	break;
				case 4:  col = color_make(130, 50, 220);		break;
				default: col = color_make(100, 170, 230);		break;
				/*
				case 1:  col = color_make(0x0C, 0, 0); 		break;
				case 2:  col = color_make(0x0A, 0, 0); 		break;
				case 3:  col = color_make(0x09, 0, 0);	 	break;
				case 4:  col = color_make(0x0E, 0, 0);		break;
				default: col = color_make(0x08, 0, 0);		break;
				**
			}

			//give x and y sides different brightness
			if (side == 1) {
				/*
				switch (col.val[0]) {
					case 0x0C:
						col.val[0] = 0x04;		break;
					case 0x0A:
						col.val[0] = 0x02;		break;
					case 0x09:
						col.val[0] = 0x01;		break;
					case 0x0E:
						col.val[0] = 0x2C;		break;
					default:
						col.val[0] = 0x00;		break;
				}
				**
				col.val[0] /= 2;
				col.val[1] /= 2;
				col.val[2] /= 2;
			};
			*/
/*
			Line slice = line_make(point_make(x, start), point_make(x, end));
			draw_line(screen, slice, col, 1);
*/

			//draw ceiling above this ray
			Line ceiling = line_make(point_make(x, 0), point_make(x, start));
			draw_line(screen, ceiling, color_make(140, 140, 60), 1);

			//draw floor below the ray
			Line floor = line_make(point_make(x, end), point_make(x, screen_size.height));
			draw_line(screen, floor, color_make(190, 190, 190), 1);
			
			//draw borders
			Color border = color_black();
			putpixel(screen, x, start, border);
			putpixel(screen, x, end, border);
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
		itoa(frame_time * 100000, &buf);
		strcat(buf, " ns/frame");
		fps->text = buf;
		draw_label(screen, fps);

		write_screen(screen);
	}
	switch_to_text();
}
