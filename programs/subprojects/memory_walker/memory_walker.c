#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <libgui/libgui.h>

#define PAGE_SIZE 0x1000

typedef struct {
	uint64_t present:1;
	uint64_t writable:1;
	uint64_t user_mode:1;
	uint64_t write_through:1;
	uint64_t cache_disabled:1;
	uint64_t accessed:1;
	uint64_t dirty:1;
	uint64_t use_page_attribute_table:1;
	uint64_t global_page:1;
	uint64_t available:3;
	uint64_t page_base:40;
	uint64_t available_high:7;
	// Available if PKE=0, otherwise memory protection key
	uint64_t contextual:4;
	uint64_t no_execute:1;
} pte_t;

#define MEMWALKER_REQUEST_PML1_ENTRY 666
typedef struct memwalker_request_pml1 {
    uint32_t event; // MEMWALKER_REQUEST_PML1_ENTRY
} memwalker_request_pml1_t;

typedef struct memwalker_request_pml1_response {
    uint32_t event; // MEMWALKER_REQUEST_PML1_ENTRY
    uint64_t pt_virt_base;
    uint64_t pt_phys_base;
    uint64_t mapped_memory_virt_base;
    uint64_t uninteresting_page_phys;
} memwalker_request_pml1_response_t;

#define AMC_FLOW_CONTROL_QUEUE_FULL 777
#define AMC_FLOW_CONTROL_QUEUE_READY 778

typedef struct amc_flow_control_msg_t {
	uint32_t event;
} amc_flow_control_msg_t;

const double _g_control_panel_height_fraction = 0.225;

typedef struct state {
	gui_button_t* control_scan_button;
	gui_button_t* upload_dump_button;
} state_t;

state_t _g_state = {0};

char* _g_memory_window = NULL;
pte_t* _g_page_table = NULL;
gui_text_view_t* _g_text_view = NULL;

static Rect _text_view_sizer(gui_text_view_t* text_view, Size window_size) {
	/*
	return rect_make(
		point_zero(), 
		size_make(
			window_size.width,
			(double)window_size.height * (1.0 - _g_control_panel_height_fraction)
		)
	);
	*/
	return rect_make(
		point_make(
			0,
			(double)window_size.height * _g_control_panel_height_fraction
		), 
		size_make(
			window_size.width,
			(double)window_size.height * (1.0 - _g_control_panel_height_fraction)
		)
	);
}

static Rect _control_panel_sizer(gui_view_t* v, Size window_size) {
	/*
	return rect_make(
		point_make(
			0,
			(double)window_size.height * (1.0 - _g_control_panel_height_fraction)
		),
		size_make(
			window_size.width,
			(double)window_size.height * _g_control_panel_height_fraction
		)
	);
	*/
	return rect_make(
		point_zero(),
		size_make(
			window_size.width,
			(double)window_size.height * _g_control_panel_height_fraction
		)
	);
}

static Rect _control_scan_button_sizer(gui_button_t* b, Size window_size) {
    Rect parent_frame = b->superview->content_layer_frame;
    uint32_t w = parent_frame.size.width * 0.3;
    uint32_t h = parent_frame.size.height * 0.5;
    return rect_make(
        point_make(
            (w / 4.0),
			(parent_frame.size.height * 0.5) - (h / 2.0)
        ),
        size_make(w, h)
    );
}

static Rect _upload_dump_button_sizer(gui_button_t* b, Size window_size) {
    Rect parent_frame = b->superview->content_layer_frame;
    uint32_t w = parent_frame.size.width * 0.3;
    uint32_t h = parent_frame.size.height * 0.5;
    return rect_make(
        point_make(
            parent_frame.size.width - w - (w / 4.0),
			(parent_frame.size.height * 0.5) - (h / 2.0)
        ),
        size_make(w, h)
    );
}

static void _control_scan_button_clicked(gui_view_t* view) {
	_timer_fired(_g_text_view);
}

static void _upload_dump_button_timer_fired(void) {
	gui_text_view_puts(_g_text_view, "[+] Done", color_green());
	_g_text_view->content_layer->scroll_layer.scroll_offset.y = (_g_text_view->content_layer->scroll_layer.max_y - _g_text_view->content_layer_frame.size.height + (_g_text_view->font_size.height * 2));
}

static void _upload_dump_button_clicked(gui_view_t* view) {
	gui_text_view_puts(_g_text_view, "[+] Uploading strings...\n", color_red());
	_g_text_view->content_layer->scroll_layer.scroll_offset.y = (_g_text_view->content_layer->scroll_layer.max_y - _g_text_view->content_layer_frame.size.height + (_g_text_view->font_size.height * 2));
	gui_timer_start(2000, _upload_dump_button_timer_fired, NULL);
}

uint64_t scan_memory(char* memory_window, pte_t* page_table, uint64_t chunk_base, uint64_t memory_chunk_size) {
    static bool finished_current_chunk = true;
    if (!finished_current_chunk) {
        printf("Skipping scan_memory because we're still working on the last chunk\n");
        return;
    }
	uint64_t string_count = 0;
	for (int frame_base = chunk_base; frame_base < chunk_base + memory_chunk_size; frame_base += PAGE_SIZE) {
		// Allow the UI to refresh every few MB scanned
		if (frame_base % (1024 * 1024 * 1) == 0) {
            printf("Running event loop pass\n");
			// TODO(PT): This is unsafe to call from a timer callback
			bool did_exit;
			//printf("Running event-loop pass...\n");
			gui_run_event_loop_pass(true, &did_exit);
			if (did_exit) {
				exit(0);
			}
		}

		// Display some progress
		if (string_count > 0 && string_count % 10000 == 0) {
			printf("Displaying progress update...\n");
			char buf[512];
			gui_text_view_puts(_g_text_view, "[+]\t(Found ", color_green());
			snprintf(&buf, sizeof(buf), "%ld", string_count);
			gui_text_view_puts(_g_text_view, buf, color_white());
			gui_text_view_puts(_g_text_view, " strings)\n", color_green());
			_g_text_view->content_layer->scroll_layer.scroll_offset.y = (_g_text_view->content_layer->scroll_layer.max_y - _g_text_view->content_layer_frame.size.height + (_g_text_view->font_size.height * 2));
		}

		page_table[0].page_base = frame_base / PAGE_SIZE;

		// Scan this frame for secrets
		// TODO(PT): This technique will miss strings that span across a frame boundary?
		//char* cursor = (char*)memory_window;
		bool is_in_string = false;
		//char* string_start = NULL;
		uint64_t string_start_idx = 0;
		for (uint64_t j = 0; j < PAGE_SIZE; j++) {
			if (isprint(memory_window[j]) && isascii(memory_window[j]) && (memory_window[j] < 128 && memory_window[j] != '\0')) {
				if (!is_in_string) {
					// We've entered a string
					is_in_string = true;
					//string_start = cursor;
					string_start_idx = j;
				}
			}
			else {
				if (is_in_string) {
					// We've exited a string - read it!
					//size_t string_len = (cursor - string_start) + 1;
					size_t string_len = (j - string_start_idx) + 1;
					// Ignore short strings to reduce false positives
					if (string_len >= 7) {
						bool filter = false;
						char* start = memory_window + string_start_idx;
						if (!filter || (filter && (!strncmp("MyS3", start, 4) || !strncmp("This", start, 4)))) {
							string_count += 1;
							if (string_count % 1000 == 0) {
								usleep(800);
							}

							// 1 extra byte for NULL terminator
							char* string = calloc(string_len + 1, sizeof(char));
							strncpy(string, memory_window + string_start_idx, string_len);
							// Don't allow random strings to turn into format specifiers in the kernel
							for (int ch_idx = 0; ch_idx < string_len; ch_idx++) {
								if (string[ch_idx] == '%') {
									string[ch_idx] = '!';
								}
							}
							uint64_t addr = frame_base + j;
							//printf("0x%016lx: %s\n", addr, string);
							printf("%s\n", string);
							// Add 1 so the NULL terminator gets sent too
							//amc_message_send("com.dangerous.memory_scan_viewer", memory_window + string_start_idx, min(string_len, 20));
							amc_message_send("com.dangerous.memory_scan_viewer", string, string_len+1);
                            free(string);

							// Wait if necessary
                            /*
							bool waiting = false;
							if (amc_has_message_from(AXLE_CORE_SERVICE_NAME)) {
								amc_message_t* msg;
								amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
								amc_flow_control_msg_t* flow_control = (amc_flow_control_msg_t*)msg->body;
								if (flow_control->event = AMC_FLOW_CONTROL_QUEUE_FULL) {
									printf("Pausing for full queue...\n");
									waiting = true;
									while (waiting) {
										amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
										flow_control = (amc_flow_control_msg_t*)msg->body;
										if (flow_control->event = AMC_FLOW_CONTROL_QUEUE_READY) {
											printf("Queue ready!\n");
											waiting = false;
										}
									}
								}
								else {
									printf("Unexpected message from core %d\n", flow_control->event);
								}
							}
                            */
						}
					}

					// Reset state
					is_in_string = false;
					//string_start = NULL;
				}
			}

			//cursor += 1;
		}

		// TODO(PT): Remember to run the event loop pass so the UI element shows up!
	}
    finished_current_chunk = true;
	return string_count;
}

void _timer_fired(gui_text_view_t* ctx) {
	gui_button_set_disabled(_g_state.control_scan_button, true);
	gui_button_set_title(_g_state.control_scan_button, "Scan ongoing...");

	gui_text_view_puts(_g_text_view, "[+] In-memory strings dump started...\n", color_red());

	printf("timer fired!\n");
	char buf[512];
	// Scan 4GB, 512MB at a time
	uint64_t memory_chunk_size = 2LL * 1024LL * 1024LL;
	uint64_t total_memory_to_scan = 4LL * 1024LL * 1024LL * 1024LL;
	uint64_t total_string_count = 0;
	for (uint64_t chunk_base = 0; chunk_base < total_memory_to_scan; chunk_base += memory_chunk_size) {
		gui_text_view_puts(_g_text_view, "[+] Scanning Phys [", color_green());
		snprintf(&buf, sizeof(buf), "0x%016lx - 0x%016lx", chunk_base, chunk_base + memory_chunk_size - 1);
		gui_text_view_puts(_g_text_view, buf, color_white());
		gui_text_view_puts(_g_text_view, "]\n", color_green());

		uint64_t string_count = scan_memory(_g_memory_window, _g_page_table, chunk_base, memory_chunk_size);
		total_string_count += string_count;

		gui_text_view_puts(_g_text_view, "[+]\tFound ", color_green());
		snprintf(&buf, sizeof(buf), "%ld", string_count);
		gui_text_view_puts(_g_text_view, buf, color_white());
		gui_text_view_puts(_g_text_view, " strings\n", color_green());
		_g_text_view->content_layer->scroll_layer.scroll_offset.y = (_g_text_view->content_layer->scroll_layer.max_y - _g_text_view->content_layer_frame.size.height + (_g_text_view->font_size.height * 2));
	}
	gui_text_view_puts(_g_text_view, "[+] All done, will upload ", color_green());
	snprintf(&buf, sizeof(buf), "%ld", total_string_count);
	gui_text_view_puts(_g_text_view, buf, color_white());
	gui_text_view_puts(_g_text_view, " strings.\n", color_green());
	printf("*** FINISHED MEMORY SCAN at %ld ***\n", ms_since_boot());

	gui_button_set_disabled(_g_state.upload_dump_button, false);
}

int main(int argc, char** argv) {
	amc_register_service("com.dangerous.memory_walker");

	// Request this before invoking libgui to avoid conflicts with message order
	memwalker_request_pml1_t request = {
		.event = MEMWALKER_REQUEST_PML1_ENTRY,
	};
	amc_message_send(AXLE_CORE_SERVICE_NAME, &request, sizeof(memwalker_request_pml1_t));
	amc_message_t* response_msg;
	amc_message_await__u32_event(AXLE_CORE_SERVICE_NAME, MEMWALKER_REQUEST_PML1_ENTRY, &response_msg);
	memwalker_request_pml1_response_t* response = (memwalker_request_pml1_response_t*)response_msg->body;
	uint64_t pt_virt = response->pt_virt_base;
	uint64_t pt_phys = response->pt_phys_base;
	uint64_t mapped_memory_virt_base = response->mapped_memory_virt_base;
	printf("Got mapped PT at 0x%016lx\n", pt_virt);
	printf("Phys PT at 0x%016lx\n", pt_phys);
	printf("Mapped memory base 0x%016lx\n", mapped_memory_virt_base);

	// Re-use what will become our window as a place to put some data, for testing
	uint64_t secret_data_phys = response->uninteresting_page_phys;
	/*
	char* secret_data_virt = (char*)mapped_memory_virt_base;
	strcpy(secret_data_virt, "This is a test string!");
	*/

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("Memory Scanner", 860, 700);
	Size window_size = window->size;

	Rect notepad_frame = rect_make(point_zero(), window_size);
	/*
	gui_text_view_t* t = gui_text_view_create(
		window,
		(gui_window_resized_cb_t)_text_view_sizer
	);
	*/

	// Scanner output view
	_g_text_view = gui_text_view_alloc();
	gui_text_view_init(_g_text_view, window, (gui_window_resized_cb_t)_text_view_sizer);
	_g_text_view->font_size = size_make(12, 18);
	gui_text_view_add_to_window(_g_text_view, window);
	gui_view_set_title(_g_text_view, "Status");

    // Control panel
	// Start scan - stop scan
	// Rescan?
	gui_view_t* control_panel_view = gui_view_create(window, (gui_window_resized_cb_t)_control_panel_sizer);
	gui_view_set_title(control_panel_view, "Control Panel");
    control_panel_view->background_color = color_make(160, 160, 160);
	_g_state.control_scan_button = gui_button_create(control_panel_view, (gui_window_resized_cb_t)_control_scan_button_sizer, "Scan Memory");
	_g_state.control_scan_button->button_clicked_cb = (gui_button_clicked_cb_t)_control_scan_button_clicked;

	_g_state.upload_dump_button = gui_button_create(control_panel_view, (gui_window_resized_cb_t)_upload_dump_button_sizer, "Upload Dump");
	gui_button_set_disabled(_g_state.upload_dump_button, true);
	_g_state.upload_dump_button->button_clicked_cb = (gui_button_clicked_cb_t)_upload_dump_button_clicked;

	gui_text_view_puts(_g_text_view, "[+] Requesting PML1 entry from kernel...\n", color_green());
	gui_text_view_puts(_g_text_view, "[+] Received PML1 entry from kernel!\n", color_green());
	char buf[512];
	gui_text_view_puts(_g_text_view, "[+] PML1 entry VirtAddr ", color_green());
	snprintf(&buf, sizeof(buf), "0x%016lx\n", pt_virt);
	gui_text_view_puts(_g_text_view, buf, color_white());

	gui_text_view_puts(_g_text_view, "[+] PML1 entry PhysAddr ", color_green());
	snprintf(&buf, sizeof(buf), "0x%016lx\n", pt_phys);
	gui_text_view_puts(_g_text_view, buf, color_white());

	gui_text_view_puts(_g_text_view, "[+] Virt window         ", color_green());
	snprintf(&buf, sizeof(buf), "0x%016lx\n", mapped_memory_virt_base);
	gui_text_view_puts(_g_text_view, buf, color_white());

	uint64_t* memory_window = (uint64_t*)mapped_memory_virt_base;

	pte_t* page_table = (pte_t*)pt_virt;

	uint64_t desired_address = 0x0LL;
	_g_memory_window = (char*)memory_window;
	_g_page_table = page_table;
	
	//gui_timer_start(2000, _timer_fired, _g_text_view);
	//_timer_fired(_g_text_view);

/*
	// Scan 4GB of physical memory
	for (int i = 0; i < (4 * 1024 * 1024 / 4); i++) {
		//printf("New desired address 0x%016lx\n", desired_address);
		page_table[0].page_base = desired_address / PAGE_SIZE;
		//printf("Remapped window to phys 0x%016lx, contents:\n", desired_address);

		// Scan this page for secrets
		// TODO(PT): This technique will miss strings that span more than a page?
		//char* cursor = (char*)memory_window;
		bool is_in_string = false;
		//char* string_start = NULL;
		uint64_t string_start_idx = 0;
		for (uint64_t j = 0; j < PAGE_SIZE - 1; j++) {
			if (isprint(memory_window_as_char[j])) {
				if (!is_in_string) {
					// We've entered a string
					is_in_string = true;
					//string_start = cursor;
					string_start_idx = j;
				}
			}
			else {
				if (is_in_string) {
					// We've exited a string - read it!
					//size_t string_len = (cursor - string_start) + 1;
					size_t string_len = (j - string_start_idx) + 1;
					// Ignore short strings to reduce false positives
					if (string_len >= 6) {
						// 1 extra byte for NULL terminator
						char* string = calloc(string_len + 1, sizeof(char));
						strncpy(string, memory_window_as_char + string_start_idx, string_len);
						printf("Found string: %s\n", string);
						free(string);
					}

					// Reset state
					is_in_string = false;
					//string_start = NULL;
				}
			}

			//cursor += 1;
		}

		// TODO(PT): Remember to run the event loop pass so the UI element shows up!
		desired_address += PAGE_SIZE;
	}
*/

	gui_enter_event_loop();

	return 0;
}
