#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include "../math.h"
#include "../lib/putpixel.h"
#include "../lib/shapes.h"
#include "rect.h"

static bool val_in_range(int value, int min, int max) { 
	return (value >= min) && (value < max); 
}

bool rect_intersects(Rect A, Rect B) {
    bool x_overlap = val_in_range(rect_min_x(A), rect_min_x(B), rect_max_x(B)) ||
                    val_in_range(rect_min_x(B), rect_min_x(A), rect_max_x(A));

    bool y_overlap = val_in_range(rect_min_y(A), rect_min_y(B), rect_max_y(B)) ||
                    val_in_range(rect_min_y(B), rect_min_y(A), rect_max_y(A));

    return x_overlap && y_overlap;
}

Rect* Rect_new(int top, int left, int bottom, int right) {
    //Attempt to allocate the object
    Rect* rect;
    if(!(rect = (Rect*)malloc(sizeof(Rect))))
        return rect;

    //Assign intial values
	rect->origin.x = left;
	rect->origin.y = top;
	rect->size.width = right - left;
	rect->size.height = bottom - top;

    return rect;
}

/*
List* Rect_split(Rect subject_rect, Rect cutting_rect) {
    //Allocate the list of result rectangles
    List* output_rects;
    if(!(output_rects = List_new()))
        return output_rects;

    Rect subject_copy = subject_rect;

    //We need a rectangle to hold new rectangles before
    //they get pushed into the output list
    Rect* temp_rect;

    //Begin splitting
    //1 -Split by left edge if that edge is between the subject's left and right edges 
    if(rect_min_x(cutting_rect) > rect_min_x(subject_copy) && rect_min_x(cutting_rect) <= rect_max_x(subject_copy)) {

        //Try to make a new rectangle spanning from the subject rectangle's left and stopping before 
        //the cutting rectangle's left
        if(!(temp_rect = Rect_new(rect_min_y(subject_copy), rect_min_x(subject_copy),
                                  rect_max_y(subject_copy), rect_min_x(cutting_rect) - 1))) {

            //If the object creation failed, we need to delete the list and exit failed
            free(output_rects);

            return (List*)0;
        }

        //Add the new rectangle to the output list
        List_add(output_rects, temp_rect);

        //Shrink the subject rectangle to exclude the split portion
		int diff = rect_min_x(cutting_rect) - rect_min_x(subject_copy);
		rect_min_x(subject_copy) += diff;
		subject_copy.size.width -= diff;
    }

    //2 -Split by top edge if that edge is between the subject's top and bottom edges 
    if(rect_min_y(cutting_rect) > rect_min_y(subject_copy) && rect_min_y(cutting_rect) <= rect_max_y(subject_copy)) {

        //Try to make a new rectangle spanning from the subject rectangle's top and stopping before 
        //the cutting rectangle's top
        if(!(temp_rect = Rect_new(rect_min_y(subject_copy), rect_min_x(subject_copy),
                                  rect_min_y(cutting_rect) - 1, rect_max_x(subject_copy)))) {

            //If the object creation failed, we need to delete the list and exit failed
            //This time, also delete any previously allocated rectangles
            for(; output_rects->count; temp_rect = List_remove_at(output_rects, 0))
                free(temp_rect);

            free(output_rects);

            return (List*)0;
        }

        //Add the new rectangle to the output list
        List_add(output_rects, temp_rect);

        //Shrink the subject rectangle to exclude the split portion
		int diff = rect_min_y(cutting_rect) - rect_min_y(subject_copy);
		rect_min_y(subject_copy) += diff;
		subject_copy.size.height -= diff;
    }

    //3 -Split by right edge if that edge is between the subject's left and right edges 
    if(rect_max_x(cutting_rect) >= rect_min_x(subject_copy) && rect_max_x(cutting_rect) < rect_max_x(subject_copy)) {

        //Try to make a new rectangle spanning from the subject rectangle's right and stopping before 
        //the cutting rectangle's right
        if(!(temp_rect = Rect_new(rect_min_y(subject_copy), rect_max_x(cutting_rect) + 1,
                                  rect_max_y(subject_copy), rect_max_x(subject_copy)))) {

            //Free on fail
            for(; output_rects->count; temp_rect = List_remove_at(output_rects, 0))
                free(temp_rect);

            free(output_rects);

            return (List*)0;
        }

        //Add the new rectangle to the output list
        List_add(output_rects, temp_rect);

        //Shrink the subject rectangle to exclude the split portion
		int shrink_amount = rect_max_x(subject_copy) - rect_max_x(cutting_rect);
		subject_copy.size.width -= shrink_amount;
    }

    //4 -Split by bottom edge if that edge is between the subject's top and bottom edges 
    if(rect_max_y(cutting_rect) >= rect_min_y(subject_copy) && rect_max_y(cutting_rect) < rect_max_y(subject_copy)) {

        //Try to make a new rectangle spanning from the subject rectangle's bottom and stopping before 
        //the cutting rectangle's bottom
        if(!(temp_rect = Rect_new(rect_max_y(cutting_rect) + 1, rect_min_x(subject_copy),
                                  rect_max_y(subject_copy), rect_max_x(subject_copy)))) {

            //Free on fail
            for(; output_rects->count; temp_rect = List_remove_at(output_rects, 0))
                free(temp_rect);

            free(output_rects);

            return (List*)0;
        }

        //Add the new rectangle to the output list
        List_add(output_rects, temp_rect);

        //Shrink the subject rectangle to exclude the split portion
		int shrink_amount = rect_max_y(subject_copy) - rect_max_y(cutting_rect);
		subject_copy.size.height -= shrink_amount;
    }
 
    //Finally, after all that, we can return the output rectangles 
    return output_rects;
}
*/

Rect rect_union(Rect a, Rect b) {
	Rect ret;
	ret.origin.x = MIN(rect_min_x(a), rect_min_x(b));
	ret.origin.y = MIN(rect_min_y(a), rect_min_y(b));
    uint32_t max_x = MAX(rect_max_x(a), rect_max_x(b));
    uint32_t max_y = MAX(rect_max_y(a), rect_max_y(b));
    ret.size.width = max_x - rect_min_x(ret);
    ret.size.height = max_y - rect_min_y(ret);
	return ret;
}

bool rect_is_null(Rect rect) {
	return (rect_min_x(rect) == 0 &&
			rect_min_y(rect) == 0 &&
			rect_max_x(rect) == 0 &&
			rect_max_y(rect) == 0);
}

Rect rect_null() {
	return rect_zero();
}

Rect rect_intersect(Rect a, Rect b) {
	Rect result;

	//check for no overlap
	if (!(rect_min_x(a) <= rect_max_x(b)) &&
		  rect_max_x(a) >= rect_min_x(b) &&
		  rect_min_y(a) <= rect_max_y(b) &&
		  rect_max_y(a) >= rect_min_y(b)) {
		return rect_zero();
	}

	//result starts as copy of first rect
	result = a;
	
	//shrink to rightmost left edge
	if (rect_min_x(b) >= rect_min_x(result) && rect_min_x(b) <= rect_max_x(result)) {
        int diff = rect_min_x(b) - rect_min_x(result);
        result.origin.x += diff;
        result.size.width -= diff;
	}

	//shrink to bottommost top edge
	if (rect_min_y(b) >= rect_min_y(result) && rect_min_y(b) <= rect_max_y(result)) {
        int diff = rect_min_y(b) - rect_min_y(result);
        result.origin.y += diff;
        result.size.height -= diff;
	}

	//shrink to leftmost right edge
	if (rect_max_x(b) >= rect_min_x(result) && rect_max_x(b) <= rect_max_x(result)) {
		result.size.width = b.size.width;
	}

	//shrink to topmost bottom edge
	if (rect_max_x(b) >= rect_min_y(result) && rect_max_y(b) <= rect_max_y(result)) {
		result.size.height = b.size.height;
	}

	return result;
}

bool rect_contains_point(Rect r, Point p) {
	if (p.x >= rect_min_x(r) && p.y >= rect_min_y(r) && p.x < rect_max_x(r) && p.y < rect_max_y(r)) {
		return true;
	}
	return false;
}

Rect convert_rect(Rect outer, Rect inner) {
	Rect ret;
	ret.origin.x = inner.origin.x + outer.origin.x;
	ret.origin.y = inner.origin.y + outer.origin.y;
	ret.size.width = MIN(inner.size.width, outer.size.width);
	ret.size.height = MIN(inner.size.height, outer.size.height);
	return ret;
}

Rect rect_inset(Rect src, int dx, int dy) {
	Rect ret;
	ret.origin.x = src.origin.x - dx;
	ret.origin.y = src.origin.y - dy;
	ret.size.width = src.size.width + (dx * 2);
	ret.size.height = src.size.height + (dy * 2);

	if (ret.size.width < 0 || ret.size.height < 0) {
		return rect_null();
	}
	return ret;
}

