#include "rexle.h"
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/lib/shapes.h>
#include <stdint.h>
#include <std/std.h>
#include <std/math.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/util/kbman/kbman.h>

typedef enum {
	WALL_NONE = 0,
	WALL_1,
	WALL_2,
	WALL_3,
	WALL_4,
	WALL_5,
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
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,0,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
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

int rexle() {
	//switch graphics modes
	Screen* screen = switch_to_vesa();
	Size screen_size = screen->window->frame.size;

	Font* font = setup_font();
	Label* fps_label = create_label(rect_make(point_make(0, 0), size_make(100, 100)), "test");
	fps_label->text_color = color_make(12, 0, 0);
	add_sublabel(screen->window->content_view, fps_label);

	double time = 0; //current frame timestamp
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

			//wall color
			Color col;
			switch(world[(int)map_pos.x][(int)map_pos.y]) {
				case 1:  col = color_red(); 		break;
				case 2:  col = color_green(); 		break;
				case 3:  col = color_blue();	 	break;
				case 4:  col = color_purple();		break;
				default: col = color_black();	break;
				/*
				case 1:  col = color_make(3, 0, 0); 		break;
				case 2:  col = color_make(4, 0, 0); 		break;
				case 3:  col = color_make(1, 0, 0);	 	break;
				case 4:  col = color_make(3, 0, 0);		break;
				default: col = color_make(4, 0, 0);		break;
				*/
			}

			//give x and y sides different brightness
			if (side == 1) {
				col.val[0] /= 2;
				col.val[1] /= 2;
				col.val[2] /= 2;
			};

			Line slice = line_make(point_make(x, start), point_make(x, end));
			draw_line(screen, slice, col, 1);
		}

		//timing
		time_prev = time;
		time = tick_count();
		double frame_time = (time - time_prev) / 1000.0;
	
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
		//draw_label(screen, fps_label);
		write_screen(screen);
		//clear screen
		fill_screen(screen, color_black());
	}
	switch_to_text();
}
