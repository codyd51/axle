#include "calculator.h"
#include <gfx/lib/gfx.h>
#include <gfx/lib/button.h>
#include <std/std.h>

typedef enum {
	ADD_OP = 0,
	SUB_OP,
	MUL_OP,
	DIV_OP,
} calc_op;

static Label* result_label;
static int pending_val = 0;
static calc_op pending_operator;

void calc_num_click(Button* b) {
	char* appended = kmalloc(32);
	memset(appended, 0, 32);

	//if calculator is showing default '0' text, replace it
	if (strcmp(result_label->text, "0") == 0) {
		strcpy(appended, b->label->text);
	}
	//else, append to current string
	else {
		strcpy(appended, result_label->text);
		strcat(appended, b->label->text);
	}

	set_text(result_label, appended);
}

void calc_op_click(Button* b) {
	if (strcmp(b->label->text, "=") == 0) {
		int new_val = atoi(result_label->text);
		int result_num = 0;

		switch (pending_operator) {
			case ADD_OP:
				result_num = pending_val + new_val;
				break;
			case SUB_OP:
				result_num = pending_val - new_val;
				break;
			case MUL_OP:
				result_num = pending_val * new_val;
				break;
			case DIV_OP:
			default:
				//divison by 0 = axle sad
				if (new_val != 0) {
					result_num = pending_val / new_val;
				}
				break;
		}

		char* result = kmalloc(32);
		memset(result, 0, 32);
		itoa(result_num, result);
		set_text(result_label, result);
	}
	else {
		pending_val = atoi(result_label->text);
		if (strcmp(b->label->text, "+") == 0) {
			pending_operator = ADD_OP;
		}
		else if (strcmp(b->label->text, "-") == 0) {
			pending_operator = SUB_OP;
		}
		else if (strcmp(b->label->text, "X") == 0) {
			pending_operator = MUL_OP;
		}
		else if (strcmp(b->label->text, "/") == 0) {
			pending_operator = DIV_OP;
		}
		set_text(result_label, "0");
	}
}

void calculator_xserv(Coordinate origin) {
	Size button_size = size_make(60, 60);
	int result_view_height = 50;
	int button_spacing = 0;

	//width is button_size * 4 because 3 rows of # buttons + 1 row of operators
	Window* calc_win = create_window(rect_make(origin, size_make(button_size.width * 4, WINDOW_TITLE_VIEW_HEIGHT + result_view_height + button_spacing + (button_size.height * 4))));
	calc_win->title = "Calculator";

	//number display
	View* label_view = create_view(rect_make(point_zero(), size_make(calc_win->frame.size.width, result_view_height)));
	result_label = create_label(rect_make(point_make(10, 10), label_view->frame.size), "0");
	label_view->background_color = color_white();
	add_sublabel(label_view, result_label);
	add_subview(calc_win->content_view, label_view);

	View* button_view = create_view(rect_make(point_make(0, rect_max_y(label_view->frame)), size_make(calc_win->frame.size.width, calc_win->frame.size.height - label_view->frame.size.height)));
	button_view->background_color = color_make(200, 200, 255);
	add_subview(calc_win->content_view, button_view);

	//number buttons 1-9
	for (int col = 0; col < 3; col++) {
		for (int row = 2; row >= 0; row--) {
			int val = ((3 - col) * 3) + row - 2;
			char title[32];
			itoa(val, (char*)&title);

			Button* b = create_button(rect_make(point_make((row * button_size.width) + button_spacing, (col * button_size.height) + button_spacing), button_size), title);
			b->mousedown_handler = (event_handler)&calc_num_click;
			add_button(button_view, b);
		}
	}
	//3 * button spacing to account for above buttons
	Button* zero = create_button(rect_make(point_make(button_spacing, 3 * button_size.height + button_spacing), size_make(button_size.width * 2, button_size.height)), "0");
	zero->mousedown_handler = (event_handler)&calc_num_click;
	add_button(button_view, zero);

	Button* equals = create_button(rect_make(point_make(rect_max_x(zero->frame), 3 * button_size.height + button_spacing), size_make(button_size.width, button_size.height)), "=");
	equals->mousedown_handler = (event_handler)&calc_op_click;
	add_button(button_view, equals);
	
	//operator buttons
	for (int i = 0; i < 4; i++){
		char* title;
		switch (i) {
			case 0:
				title = "/";
				break;
			case 1:
				title = "X";
				break;
			case 2:
				title = "-";
				break;
			case 3:
			default:
				title = "+";
				break;
		}
		Button* b = create_button(rect_make(point_make((3 * button_size.width) + button_spacing, button_size.height * i + button_spacing), button_size), title);
		b->mousedown_handler = (event_handler)&calc_op_click;
		add_button(button_view, b);
	}

	present_window(calc_win);
}

