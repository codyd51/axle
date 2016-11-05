#include "rect.h"
#include <std/kheap.h>
#include <std/printf.h>
#include <std/std.h>

static bool val_in_range(int value, int min, int max) { 
	return (value >= min) && (value <= max); 
}

bool rect_intersects(Rect A, Rect B) {
    bool x_overlap = val_in_range(A.origin.x, B.origin.x, B.origin.x + B.size.width) ||
                    val_in_range(B.origin.x, A.origin.x, A.origin.x + A.size.width);

    bool y_overlap = val_in_range(A.origin.y, B.origin.y, B.origin.y + B.size.height) ||
                    val_in_range(B.origin.y, A.origin.y, A.origin.y + A.size.height);

    return x_overlap && y_overlap;
}

Rect rect_make(Coordinate origin, Size size) {
	Rect rect;
	rect.origin = origin;
	rect.size = size;
	return rect;
}

Rect rect_zero() {
	return rect_make(point_zero(), size_zero());
}

array_m* rect_clip(Rect subject, Rect cutting) {
	printf("\n");
	//maximum possible 4 rectangles
	array_m* clipped = array_m_create(4);

	//if these rectangles don't intersect, do nothing
	if (!rect_intersects(subject, cutting)) {
		return clipped;
	}

	//if subject is completely occluded by cutting, do nothing
	if (rect_min_x(subject) >= rect_min_x(cutting) &&
		rect_min_y(subject) >= rect_min_y(cutting) &&
		rect_max_x(subject) <= rect_max_x(cutting) &&
		rect_max_y(subject) <= rect_max_y(cutting)) {
		return clipped;
	}

	//holds new rects before they get added to clipped
	Rect* tmp;

	//splitting
	//split by left edge if edge is between subject's left & right edges
	if (rect_min_x(cutting) <= rect_max_x(subject) && rect_min_x(cutting) >= rect_min_x(subject)) {
		printf("cutting's left edge was bounded by subject\n");

		//make new rect from subject's left to cutting rect's left
		tmp = kmalloc(sizeof(Rect));
		*tmp = rect_make(subject.origin, size_make(rect_min_x(cutting) - rect_min_x(subject), subject.size.height));

		//add to output list
		array_m_insert(clipped, tmp);

		//shrink subject to exclude split portion
		//subject.size.width = cutting.size.width;
		int diff = rect_min_x(cutting) - rect_min_x(subject);
		subject.origin.x = cutting.origin.x;
		subject.size.width -= diff;
	}

	//split by top edge if edge is between subject's top and bottom edges
	if (rect_min_y(cutting) >= rect_min_y(subject) && rect_min_y(cutting) <= rect_max_y(subject)) {
		printf("cutting's top edge was bounded by subject\n");
		
		//make new rect from subject's top to cutting rect's top 
		tmp = kmalloc(sizeof(Rect));
		*tmp = rect_make(subject.origin, size_make(subject.size.width, rect_min_y(cutting) - rect_min_y(subject)));

		//add to output list
		array_m_insert(clipped, tmp);

		//shrink subject to exclude split portion
		int diff = cutting.origin.y - subject.origin.y;
		subject.origin.y = cutting.origin.y;
		subject.size.height -= diff;
	}

	//split by right edge if edge is between subject's left and right edges
	if (rect_max_x(cutting) <= rect_max_x(subject) && rect_max_x(cutting) >= rect_min_x(subject)) {
		printf("cutting's right edge was bounded by subject\n");
		
		//make new rect from cutting's right to subject rect's right 
		tmp = kmalloc(sizeof(Rect));
		*tmp = rect_make(point_make(rect_max_x(cutting), subject.origin.y), size_make(rect_max_x(subject) - rect_max_x(cutting), subject.size.height));

		//add to output list
		array_m_insert(clipped, tmp);

		//shrink subject to exclude split portion
		int diff = rect_max_x(subject) - rect_max_x(cutting);
		subject.size.width -= diff;
	}

	//split by bottom edge if edge is between subject's top and bottom edges
	if (rect_max_y(cutting) >= rect_min_y(subject) && rect_max_y(cutting) <= rect_max_y(subject)) {
		printf("cutting's bottom edge was bounded by subject\n");
		
		//make new rect from cutting's bottom to subject rect's bottom 
		tmp = kmalloc(sizeof(Rect));
		*tmp = rect_make(point_make(rect_min_x(subject), rect_max_y(cutting)), size_make(subject.size.width, rect_max_y(subject) - rect_max_y(cutting)));

		//add to output list
		array_m_insert(clipped, tmp);

		//shrink subject to exclude split portion
		int diff = rect_max_y(subject) - rect_max_y(cutting);
		subject.size.height -= diff;
	}

	//finally, return output rects
	return clipped;
}
