#include <gfx/lib/rect.h>
#include <gfx/lib/point.h>
#include <gfx/lib/color.h>
#include <gfx/lib/shapes.h>
#include <gfx/lib/rect.h>
#include "ca_layer.h"

void rainbow_animation(Screen* screen, Rect r, int animationStep) {
    Color colors[] = {
        color_red(),
        color_orange(),
        color_yellow(),
        color_green(),
        color_blue(),
        color_purple(),
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    for (int i = 0; i < count; i++) {
        Point origin = point_make(r.origin.x + (r.size.width / count) * i, r.origin.y);
        Size size = size_make((r.size.width / count), r.size.height);
        Rect seg = rect_make(origin, size);
        printf("origin {%d, %d} size {%d, %d}\n", origin.x, origin.y, size.width, size.height);

        Color col = colors[i];
        draw_rect(screen->vmem, seg, col, THICKNESS_FILLED);
        write_screen_region(seg);
        
        sleep(animationStep / count);
    }
}

void display_daisy_screen() {
    Screen* screen = gfx_screen();
    Color background_color = color_make(40, 40, 40);
    fill_screen(screen, background_color);
    sleep(1000);

    int x_offsets[] = {10, 50, 80, 40, -20, -150, -11};
    int y_offsets[] = {-70, -30, 0, 40, 100, 160, 0};

    // Draw heart
    Point heart_tip_top = point_make(screen->resolution.width / 2, screen->resolution.height * 0.35);
    Point left_cursor = heart_tip_top;
    Point right_cursor = heart_tip_top;
    for (int i = 0; i < sizeof(x_offsets) / sizeof(x_offsets[0]); i++) {
        int right_x = x_offsets[i];
        int left_x = -right_x;
        int y = y_offsets[i];

        Point left_new = point_make(left_cursor.x + left_x, left_cursor.y + y);
        Point right_new = point_make(right_cursor.x + right_x, right_cursor.y + y);

        Line left_line = line_make(left_cursor, left_new);
        Line right_line = line_make(right_cursor, right_new);

        draw_line(screen->vmem, left_line, color_red(), 1);
        draw_line(screen->vmem, right_line, color_orange(), 1);
        write_screen(screen);

        left_cursor = left_new;
        right_cursor = right_new;
        sleep(120);
    }
    sleep(3000);

    Size default_size = screen->default_font_size;
    Size font_size = size_make(default_size.width * 2, default_size.height * 2);
    const char* label_text = "Daisy Animated Boot Ephemeral Screen Time (DA BEST)";
    int text_width = strlen(label_text) * font_size.width;

    Point lab_origin = point_make((screen->resolution.width / 2) - (text_width / 2), screen->resolution.height * 0.65);
    Point text_cursor = lab_origin;

    // Draw initial text
    for (int i = 0; i < strlen(label_text); i++) {
        char ch = label_text[i];
        draw_char(screen->vmem, ch, text_cursor.x, text_cursor.y, color_blue(), font_size);
        text_cursor.x += font_size.width;
        write_screen(screen);
        sleep(20);
    }
    // Clear where we drew the initial text
    draw_rect(screen->vmem, rect_make(lab_origin, size_make(1400, 200)), background_color, THICKNESS_FILLED);

    Color colors[] = {
        color_blue(), 
        color_white(), 
        color_red(), 
        color_green(), 
        color_orange(), 
        color_white(),
        color_purple(),
        color_blue(), 
        color_white(), 
        color_red(), 
        color_green(), 
        color_purple(),
        color_white()
    };
    int color_count = sizeof(colors) / sizeof(colors[0]);

    // Draw stacked text outline
    for (int j = 0; j < color_count; j++) {
        // Reset cursors
        lab_origin = point_make((screen->resolution.width / 2) - (text_width / 2), screen->resolution.height * 0.65);
        lab_origin.x += ((color_count - j) * 5);
        lab_origin.y += ((color_count - j) * 5);
        text_cursor = lab_origin;

        for (int i = 0; i < strlen(label_text); i++) {
            char ch = label_text[i];
            draw_char(screen->vmem, ch, text_cursor.x, text_cursor.y, colors[j], font_size);
            text_cursor.x += font_size.width;
        }
        if (true || j > 0) {
            write_screen(screen);
            sleep(80);
        }
    }

    int screen_w = screen->resolution.width;
    int screen_h = screen->resolution.height;
    int heart_col_i = 0;
    float scales[] = {0.25, 0.75, 1.25, 0.3, 2.0, 0.66, 1.2, 0.3, 0.46, 1.0, 1.0};
    int iter_i = 0;
    int heart_start_time = time();
    while (time() < heart_start_time + 4000) {
        Point heart_tip_top = point_make(rand() % (screen_w-(screen_w/6)), rand() % (screen_h-(screen_h/6)));
        float scale = scales[(iter_i++) % (sizeof(scales) / sizeof(scales[0]))];

        Point left_cursor = heart_tip_top;
        Point right_cursor = heart_tip_top;
        for (int i = 0; i < sizeof(x_offsets) / sizeof(x_offsets[0]); i++) {
            int right_x = (int)(x_offsets[i] * scale);
            int left_x = -right_x;
            int y = (int)(y_offsets[i] * scale);

            Point left_new = point_make(left_cursor.x + left_x, left_cursor.y + y);
            Point right_new = point_make(right_cursor.x + right_x, right_cursor.y + y);

            Line left_line = line_make(left_cursor, left_new);
            Line right_line = line_make(right_cursor, right_new);

            draw_line(screen->vmem, left_line, colors[((heart_col_i++)%color_count)], 1);
            draw_line(screen->vmem, right_line, colors[((heart_col_i++)%color_count)], 1);
            write_screen(screen);

            left_cursor = left_new;
            right_cursor = right_new;
        }
        sleep(100);
    }

    // Draw random rectangles
    int start_time = time();
    while (time() < start_time + 3000) {
        Color c = color_make(rand() % 255, rand() % 255, rand() % 255);
        Point origin = point_make(rand() % screen->resolution.width, rand() % screen->resolution.height);
        Size size = size_make(20 + (int)(rand() % screen->resolution.width / 8), 20 + (int)(rand() % screen->resolution.height / 8));
        Rect r = rect_make(origin, size);
        draw_rect(screen->vmem, r, c, THICKNESS_FILLED);
        write_screen(screen);
    }

    // Draw screen washing
    Color wash_colors[] = {color_make(237, 33, 196), color_make(33, 138, 237), color_white()};
    for (int j = 0; j < sizeof(wash_colors) / sizeof(wash_colors[0]); j++) {
        Color color = wash_colors[j];
        Point origin = point_make(0, 0);
        int y_blocks = 10;
        int y_step = screen->resolution.height / y_blocks;
        for (int i = 0; i < y_blocks; i++) {
            Rect r = rect_make(origin, size_make(screen->resolution.width, y_step));
            draw_rect(screen->vmem, r, color, THICKNESS_FILLED);
            origin.y += y_step;
            write_screen(screen);
            sleep(80);
        }
    }
}

void display_boot_screen() {
    Screen* screen = gfx_screen();
    fill_screen(screen, color_black());

    Point p1 = point_make(screen->resolution.width / 2, screen->resolution.height * 0.175);
    Point p2 = point_make(screen->resolution.width / 2 - screen->resolution.width / 10, screen->resolution.height * 0.5);
    Point p3 = point_make(screen->resolution.width / 2 + screen->resolution.width / 10, screen->resolution.height * 0.5);
    Triangle triangle = triangle_make(p1, p2, p3);
    draw_triangle(screen->vmem, triangle, color_green(), THICKNESS_FILLED);

    Size default_size = screen->default_font_size;
    Size font_size = size_make(default_size.width * 2, default_size.height * 2);
    char* label_text = "axle os";
    int text_width = strlen(label_text) * font_size.width;
    Point lab_origin = point_make((screen->resolution.width / 2) - (text_width / 2), screen->resolution.height * 0.65);
    draw_string(screen->vmem, label_text, lab_origin, color_white(), font_size);

    float rect_length = screen->resolution.width / 3;
    Point origin = point_make((screen->resolution.width / 2) - (rect_length / 2), (screen->resolution.height / 4) * 3);
    Size sz = size_make(rect_length - 5, screen->resolution.height / 16);
    Rect border_rect = rect_make(origin, sz);

    //fill the rectangle with white initially
    draw_rect(screen->vmem, border_rect, color_white(), THICKNESS_FILLED);
    
    write_screen(screen);

    sleep(500);

    Point rainbow_origin = point_make(origin.x + 2, origin.y + 2);
    Size rainbow_size = size_make(rect_length - 6, sz.height - 3);
    Rect rainbow_rect = rect_make(rainbow_origin, rainbow_size);
    rainbow_animation(screen, rainbow_rect, 1000);

    sleep(500);
    fill_screen(screen, color_black());
}
