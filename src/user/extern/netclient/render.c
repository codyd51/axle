#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "render.h"
#include "shims.h"

static void _draw_node(layout_node_t* node, array_t* display_cmd_list);
static void _draw_node__root(layout_node_t* node, array_t* display_cmd_list);
static void _draw_node__block(layout_node_t* node, array_t* display_cmd_list);
static void _draw_node__inline(layout_node_t* node, array_t* display_cmd_list);

static void _draw_node__root(layout_node_t* node, array_t* display_cmd_list) {
    // Add a "draw rect" command describing the background color
    // White rectangle over the whole document
    draw_command_rectangle_t* bg = calloc(1, sizeof(draw_command_rectangle_t));
    bg->cmd = DRAW_COMMAND_RECTANGLE;
    bg->rect = node->base_node.margin_frame;
    bg->color = color_white();
    bg->thickness = THICKNESS_FILLED;
    array_insert(display_cmd_list, bg);

    draw_command_rectangle_t* content_border = calloc(1, sizeof(draw_command_rectangle_t));
    content_border->cmd = DRAW_COMMAND_RECTANGLE;
    content_border->rect = node->base_node.margin_frame;
    content_border->color = color_dark_gray();
    content_border->thickness = 1;
    array_insert(display_cmd_list, content_border);

    // Draw the root node's direct child
    _draw_node((layout_node_t*)node->base_node.children[0], display_cmd_list);
}

static void _draw_node__block(layout_node_t* node, array_t* display_cmd_list) {
    layout_block_node_t* bn = &node->block_node;
    if (bn->sets_background_color) {
        // Draw the node's background
        draw_command_rectangle_t* outline = calloc(1, sizeof(draw_command_rectangle_t));
        outline->cmd = DRAW_COMMAND_RECTANGLE;
        outline->rect = bn->content_frame;
        outline->color = bn->background_color;
        outline->thickness = THICKNESS_FILLED;
        array_insert(display_cmd_list, outline);
    }
    // Recursively draw the child nodes
    for (uint32_t i = 0; i < bn->child_count; i++) {
        layout_node_t* child = (layout_node_t*)bn->children[i];
        _draw_node(child, display_cmd_list);
    }
}

static void _draw_node__inline(layout_node_t* node, array_t* display_cmd_list) {
    layout_inline_node_t* in = &node->inline_node;
    if (in->text != NULL) {
        draw_command_text_t* draw_text = calloc(1, sizeof(draw_command_text_t));
        draw_text->cmd = DRAW_COMMAND_TEXT;
        draw_text->rect = in->content_frame;
        draw_text->text = strdup(in->text);
        draw_text->font_color = color_black();
        draw_text->font_size = in->font_size;
        array_insert(display_cmd_list, draw_text);
    }
}

static void _draw_node(layout_node_t* node, array_t* display_cmd_list) {
    if (node->base_node.mode == ROOT_LAYOUT) {
        _draw_node__root(node, display_cmd_list);
    }
    else if (node->base_node.mode == BLOCK_LAYOUT) {
        _draw_node__block(node, display_cmd_list);
    }
    else if (node->base_node.mode == INLINE_LAYOUT) {
        _draw_node__inline(node, display_cmd_list);
    }
    else {
        assert(false, "Unknown layout node type");
    }
}

array_t* draw_commands_generate_from_layout(layout_root_node_t* root_layout) {
    array_t* display_cmd_list = array_create(64);

    _draw_node((layout_node_t*)root_layout, display_cmd_list);

    for (uint32_t i = 0; i < display_cmd_list->size; i++) {
        draw_command_t* cmd = array_lookup(display_cmd_list, i);
        printf("[Draw ");
        switch (cmd->base.cmd) {
            case DRAW_COMMAND_RECTANGLE:
                printf("Rect: ");
                rect_print(cmd->rect.rect);
                break;
            case DRAW_COMMAND_TEXT:
                printf("Text: ");
                rect_print(cmd->text.rect);
                printf(" %s", cmd->text.text);
                break;
            default:
                assert(false, "Unknown draw command");
                break;
        }
        printf("]\n");
    }
    return display_cmd_list;
}
