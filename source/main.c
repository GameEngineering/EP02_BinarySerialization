
/*================================================================
	* Copyright: 2020 John Jackson
	* serialization

	Custom Byte Buffer / Binary Serialization example

	Shown in video: https://www.youtube.com/watch?v=GXdT8twQxxI

	The purpose of this example is to demonstrate how to serialize arbitrary data to disk using a custom byte buffer implementation.
	A simple pixel art editor is thrown in to demonstrate some of these ideas in practice.

	Libraries used : 
	{
		* Nuklear UI: https://github.com/Immediate-Mode-UI/Nuklear
		* TinyFileDialog: https://github.com/native-toolkit/tinyfiledialogs
	}

	Press `esc` to exit the application.
=================================================================*/

// Nuklear will define these, so we will not implement them ourselves
#include "gs_nk_incl.h"

#include "font.h"
// #include "tinyfiledialogs.h"
#include "font/font_data.c"

gs_command_buffer_t gcb = {0};

/* Platform */
gs_nk_ctx_t         gs_nk = {0};
struct nk_colorf    bg = {0};
struct nk_colorf    g_nk_color = {0};

//================================================================

void print_bin(uint8_t byte)
{
    int i = CHAR_BIT; /* however many bits are in a byte on your platform */
    while(i--) {
        putchar('0' + ((byte >> i) & 1)); /* loop through and print the bits */
    }
}

// Forward Decls
void pan_camera();

/*==============
// Byte Buffer
==============*/

typedef struct byte_buffer_t
{
	uint8_t* data;
	uint32_t position;
	uint32_t capacity;
	uint32_t size;
} byte_buffer_t;

#define byte_buffer_default_capacity 	1024

byte_buffer_t byte_buffer_new()
{
	byte_buffer_t buffer = {0};
	buffer.data = malloc(byte_buffer_default_capacity);
	memset(buffer.data, 0, byte_buffer_default_capacity);
	buffer.capacity = byte_buffer_default_capacity;
	buffer.size = 0;
	buffer.position = 0;
	return buffer; 
}

void byte_buffer_resize(byte_buffer_t* buffer, size_t sz);

void __byte_buffer_write_impl(byte_buffer_t* buffer, void* data, size_t sz)
{ 
	size_t total_write_size = buffer->position + sz;
	if (total_write_size >= buffer->capacity)
	{
		size_t capacity = buffer->capacity ? buffer->capacity * 2 : byte_buffer_default_capacity;
		while (capacity < total_write_size)
		{
			capacity *= 2;
		}
		byte_buffer_resize(buffer, capacity);
	}
	memcpy(buffer->data + buffer->position, data, sz);
	buffer->position += sz;
	buffer->size += sz; 
}

// Generic write function
#define byte_buffer_write(_buffer, T, _val)\
do {\
	byte_buffer_t* _bb = (_buffer);\
	size_t _sz = sizeof(T);\
	T _v = _val;\
	__byte_buffer_write_impl(_bb, (void*)&(_v), _sz);\
} while (0)

// Generic read function
#define byte_buffer_read(_buffer, T, _val_p)\
do {\
	T* _v = (T*)(_val_p);\
	byte_buffer_t* _bb = (_buffer);\
	*(_v) = *(T*)(_bb->data + _bb->position);\
	_bb->position += sizeof(T);\
} while (0)

// Utiltiy functions
void byte_buffer_clear(byte_buffer_t* buffer)
{
	buffer->size = 0;
	buffer->position = 0;
}

void byte_buffer_free(byte_buffer_t* buffer)
{
	byte_buffer_clear(buffer);
	free(buffer->data);
	buffer->data = NULL;
	buffer->capacity = 0;
}

void byte_buffer_resize(byte_buffer_t* buffer, size_t sz)
{
	uint8_t* data = realloc(buffer->data, sz);
	if (data == NULL) {
		return;
	}

	buffer->data = data;
	buffer->capacity = sz;
}

void byte_buffer_seek_to_beg(byte_buffer_t* buffer)
{
	buffer->position = 0;
}

void byte_buffer_seek_to_end(byte_buffer_t* buffer)
{
	buffer->position = buffer->size;
}

// Writing
gs_result 
byte_buffer_write_to_file(byte_buffer_t* buffer, const char* path)
{
	// gs_result res = gs_platform_write_file_contents(path, "wb", buffer->data, sizeof(uint8_t), buffer->size);
 //    gs_assert(res == GS_RESULT_SUCCESS);
	// return res;
    FILE* write = NULL;
    write = fopen(path, "wb");
    fwrite(buffer->data, sizeof(uint8_t), buffer->size, write);
    return GS_RESULT_SUCCESS;
}

// Reading
gs_result
byte_buffer_read_from_file(byte_buffer_t* buffer, const char* path)
{
	// Clear previous data if any
	if (buffer->data) {
		byte_buffer_free(buffer);
        buffer->data = NULL;
	}

    // This is giving me garbage data, apparenty...
    // buffer->data = (u8*)gs_platform_read_file_contents(path, "rb", &buffer->size);
    FILE *fp = fopen(path, "rb");
    if (fp != NULL) {
        /* Go to the end of the file. */
        if (fseek(fp, 0L, SEEK_END) == 0) {
            /* Get the size of the file. */
            size_t bufsize = ftell(fp);
            if (bufsize == -1) { /* Error */ }

            /* Allocate our buffer to that size. */
            buffer->data = gs_malloc(bufsize);
            memset(buffer->data, 0, bufsize);

            /* Go back to the start of the file. */
            if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

            /* Read the entire file into memory. */
            size_t newLen = fread(buffer->data, sizeof(char), bufsize, fp);
            if ( ferror( fp ) != 0 ) {
                gs_println("Error reading file", stderr);
            } 
            // else {
            //     source[newLen++] = '\0'; /* Just to be safe. */
            // }
            buffer->size = (int32_t)bufsize;
            // if (sz) {
            //     *sz = (int32_t)bufsize;
            // }
        }
        fclose(fp);
    }

	if (!buffer->data) {
		gs_assert(false);
		return GS_RESULT_FAILURE;
	}

	buffer->position = 0;
	buffer->capacity = buffer->size;
	return GS_RESULT_SUCCESS;
}

//================================================================

/*=========
// NK - UI
=========*/

struct pixel_frame_t;
void nk_init_ui();
void nk_do_ui(gs_command_buffer_t* cb, struct pixel_frame_t* pf);

//================================================================

/*=======================================
// Texture Buffer For Simple Pixel Editor
=======================================*/

b32 color_equal(gs_color_t c0, gs_color_t c1) 
{
	return  (c0.r == c1.r) &&
			(c0.g == c1.g) &&
			(c0.b == c1.b) &&
			(c0.a == c1.a);
}

#define default_pixel_frame_width 		128
#define default_pixel_frame_height		128
#define ui_frame_width 					128
#define ui_frame_height 				128

#ifdef GS_PLATFORM_WEB
    #define GS_SHADER_VERSION "#version 300 es\n"
#else
    #define GS_SHADER_VERSION "#version 330 core\n"
#endif

const char* pixel_frame_v_src =
GS_SHADER_VERSION
"layout(location = 0) in vec2 a_pos;\n"
"layout(location = 1) in vec2 a_uv;\n"
"uniform mat4 u_proj;\n"
"uniform mat4 u_view;\n"
"out vec2 uv;\n"
"void main()\n"
"{\n"
"	gl_Position = u_proj * u_view * vec4(a_pos, 0.0, 1.0);\n"
"	uv = a_uv;\n"
"}";

const char* pixel_frame_f_src =
GS_SHADER_VERSION
"precision mediump float;\n"
"uniform sampler2D u_tex;\n"
"in vec2 uv;\n"
"out vec4 frag_color;\n"
"void main()\n"
"{\n"
"	frag_color = texture(u_tex, uv);\n"
"}";

/*
	These will be all the required op codes for undo/redo in our pixel editor
	We'll treat our undo/redo buffer as a simple VM
*/

typedef enum pixel_frame_action_op_code_type
{
	pixel_frame_action_op_write_pixels = 0x00,
	pixel_frame_action_op_clear_pixels = 0x01
} pixel_frame_action_op_code_type;

typedef struct pixel_frame_delta_color_t
{
	u8 r, g, b, a;
	u8 sign_bits;
} pixel_frame_delta_color_t;

typedef struct pixel_frame_action_write_t
{
	uint16_t idx;
	u8 r, g, b, a;
	u8 sign_bits;
} pixel_frame_action_write_t;

typedef struct pixel_frame_t
{
	gs_color_t* pixel_data;
	gs_color_t* ui_data;
	uint32_t pixel_frame_width;
	uint32_t pixel_frame_height;
	gs_handle(gs_graphics_texture_t) bg_texture;
	gs_handle(gs_graphics_texture_t) texture;
	gs_handle(gs_graphics_texture_t) ui_texture;
	gs_handle(gs_graphics_shader_t) shader;
	gs_handle(gs_graphics_uniform_t) u_tex;
	gs_handle(gs_graphics_uniform_t) u_proj;
	gs_handle(gs_graphics_uniform_t) u_view;
	gs_handle(gs_graphics_vertex_buffer_t) vbo;
	gs_handle(gs_graphics_index_buffer_t) ibo;
	gs_handle(gs_graphics_pipeline_t) pip;
	uint32_t paint_radius;
	b32 mouse_down;
	byte_buffer_t undo_buffer;
	byte_buffer_t redo_buffer;
	uint32_t current_write_size;
} pixel_frame_t;

typedef void (*put_pixel_func)(void*, gs_color_t, int32_t, int32_t);

// Bresenham Line -> from https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm
void drawLine(void* data, gs_color_t color, int32_t x0, int32_t x1, int32_t y0, int32_t y1, put_pixel_func func)
{
	int32_t dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
	int32_t dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
	int32_t err = (dx>dy ? dx : -dy)/2, e2;
 
	for(;;)
	{
		func(data, color, x0, y0);
		if (x0==x1 && y0==y1) break;
		e2 = err;
		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

// Function to put pixels 
// at subsequence points 
void drawCircle(void* data, gs_color_t color, b32 filled, int32_t xc, int32_t yc, int32_t x, int32_t y, put_pixel_func func) 
{
    if (filled)
    {
    	drawLine(data, color, xc - x, xc + x, yc + y, yc + y, func);
    	drawLine(data, color, xc - x, xc + x, yc - y, yc - y, func);
    	drawLine(data, color, xc - y, xc + y, yc + x, yc + x, func);
    	drawLine(data, color, xc - y, xc + y, yc - x, yc - x, func);
    }
    else 
    {
	    func(data, color, xc+x, yc+y); 
	    func(data, color, xc-x, yc+y); 

	    func(data, color, xc+x, yc-y); 
	    func(data, color, xc-x, yc-y); 

	    func(data, color, xc+y, yc+x); 
	    func(data, color, xc-y, yc+x); 

	    func(data, color, xc+y, yc-x); 
	    func(data, color, xc-y, yc-x);
    }
}

// Function for circle-generation 
// using Bresenham's algorithm 
void circleBres(void* data, gs_color_t color, b32 filled, int32_t xc, int32_t yc, int32_t r, put_pixel_func func) 
{ 
    int x = 0, y = r; 
    int d = 3 - 2 * r; 
    drawCircle(data, color, filled, xc, yc, x, y, func); 
    while (y >= x) 
    { 
        // For each pixel we will 
        // draw all eight pixels
        x++; 
  
        // Check for decision parameter 
        // and correspondingly  
        // update d, x, y 
        if (d > 0) 
        { 
            y--;  
            d = d + 4 * (x - y) + 10; 
        } 
        else
            d = d + 4 * x + 6; 
        drawCircle(data, color, filled, xc, yc, x, y, func); 
    } 
} 

gs_graphics_texture_desc_t pixel_frame_texture_parameter_desc()
{
	gs_graphics_texture_desc_t desc = {0};
	desc.mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST;
	desc.min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST;
    desc.wrap_s = GS_GRAPHICS_TEXTURE_WRAP_REPEAT;
    desc.wrap_t = GS_GRAPHICS_TEXTURE_WRAP_REPEAT;
	desc.num_mips = 0;
	desc.width = default_pixel_frame_width;
	desc.height = default_pixel_frame_height;
	desc.format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8;
	return desc;
}

pixel_frame_t pixel_frame_new()
{
	pixel_frame_t pf = {0};

	const gs_color_t checker_blue = (gs_color_t){50, 80, 180, 100};
	const gs_color_t checker_white = (gs_color_t){255, 255, 255, 100};

	const uint32_t bg_width = 5;
	const uint32_t bg_height = 5;
	gs_color_t* bg_data = gs_malloc(bg_width * bg_height * sizeof(gs_color_t));

	// Background image
	for (uint32_t h = 0; h < bg_width; ++h)
	{
		for (uint32_t w = 0; w < bg_height; ++w)
		{
			const uint32_t idx = h * bg_width + w;

			if (h % 2 == 0) {
				bg_data[idx] = w % 2 == 0 ? checker_white : checker_blue;
			}
			else {
				bg_data[idx] = w % 2 == 0 ? checker_blue : checker_white;
			}
		}
	}

    // Create dynamic texture
    pf.bg_texture = gs_graphics_texture_create (
        &(gs_graphics_texture_desc_t){
            .width = bg_width,
            .height = bg_height,
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .data = bg_data
        }
    );
	gs_free(bg_data);

	pf.pixel_frame_width = default_pixel_frame_width;
	pf.pixel_frame_height = default_pixel_frame_height;
	pf.pixel_data = gs_malloc(default_pixel_frame_width * default_pixel_frame_height * sizeof(gs_color_t));
	memset(pf.pixel_data, 0, default_pixel_frame_width * default_pixel_frame_height * sizeof(gs_color_t));
    pf.texture = gs_graphics_texture_create (
        &(gs_graphics_texture_desc_t){
            .width = default_pixel_frame_width,
            .height = default_pixel_frame_height,
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .data = pf.pixel_data
        }
    );

	// UI data
	pf.ui_data = gs_malloc(ui_frame_width * ui_frame_height * sizeof(gs_color_t));
	memset(pf.ui_data, 0, ui_frame_width * ui_frame_height * sizeof(gs_color_t));
	pf.paint_radius = 1;
    pf.ui_texture = gs_graphics_texture_create (
        &(gs_graphics_texture_desc_t){
            .width = ui_frame_height,
            .height = ui_frame_height,
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST, 
            .data = pf.ui_data
        }
    );

    // Create shader
    pf.shader = gs_graphics_shader_create (
        &(gs_graphics_shader_desc_t) {
            .sources = (gs_graphics_shader_source_desc_t[]){
                {.type = GS_GRAPHICS_SHADER_STAGE_VERTEX, .source = pixel_frame_v_src},
                {.type = GS_GRAPHICS_SHADER_STAGE_FRAGMENT, .source = pixel_frame_f_src},
            },
            .size = 2 * sizeof(gs_graphics_shader_source_desc_t),
            .name = "pf_shader"
        }
    );

    pf.u_tex = gs_graphics_uniform_create (
        &(gs_graphics_uniform_desc_t) {
            .name = "u_tex",
            .layout = &(gs_graphics_uniform_layout_desc_t){.type = GS_GRAPHICS_UNIFORM_SAMPLER2D}
        }
    );

    pf.u_proj = gs_graphics_uniform_create (
        &(gs_graphics_uniform_desc_t) {
            .name = "u_proj",
            .layout = &(gs_graphics_uniform_layout_desc_t){.type = GS_GRAPHICS_UNIFORM_MAT4}
        }
    );

    pf.u_view = gs_graphics_uniform_create (
        &(gs_graphics_uniform_desc_t) {
            .name = "u_view",
            .layout = &(gs_graphics_uniform_layout_desc_t){.type = GS_GRAPHICS_UNIFORM_MAT4}
        }
    );

    // Vertex data for quad
    float v_data[] = {
        // Positions  UVs
        -1.f, -1.f,  0.0f, 0.0f,  // Top Left
         1.f, -1.f,  1.0f, 0.0f,  // Top Right 
        -1.f,  1.f,  0.0f, 1.0f,  // Bottom Left
         1.f,  1.f,  1.0f, 1.0f   // Bottom Right
    };

    // Construct vertex buffer
    pf.vbo = gs_graphics_vertex_buffer_create(
        &(gs_graphics_vertex_buffer_desc_t) {
            .data = v_data,
            .size = sizeof(v_data)
        }
    );

    // Index data for quad
    uint32_t i_data[] = {
        0, 3, 2,    // First Triangle
        0, 1, 3     // Second Triangle
    };

    // Construct vertex buffer
    pf.ibo = gs_graphics_index_buffer_create(
        &(gs_graphics_index_buffer_desc_t) {
            .data = i_data,
            .size = sizeof(i_data)
        }
    );

    pf.pip = gs_graphics_pipeline_create (
        &(gs_graphics_pipeline_desc_t) {
            .raster = {
                .shader = pf.shader,
                .index_buffer_element_size = sizeof(uint32_t)
            },
            .blend = {
                .func = GS_GRAPHICS_BLEND_EQUATION_ADD,
                .src = GS_GRAPHICS_BLEND_MODE_SRC_ALPHA,
                .dst = GS_GRAPHICS_BLEND_MODE_ONE_MINUS_SRC_ALPHA              
            },
            .layout = {
                .attrs = (gs_graphics_vertex_attribute_desc_t[]){
                    {.format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "a_pos"},	// Position
                    {.format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "a_uv"}		// UV
                },
                .size = 2 * sizeof(gs_graphics_vertex_attribute_desc_t)
            }
        }
    );

	pf.mouse_down = false;
	pf.undo_buffer = byte_buffer_new();
	pf.redo_buffer = byte_buffer_new();
	pf.current_write_size = 0;

	return pf;
}

b32 pixel_frame_action_is_recording(pixel_frame_t* pf)
{
	return (pf->current_write_size != 0);
}

// Undo and Redo ops
void pixel_frame_action_start_record(pixel_frame_t* pf, pixel_frame_action_op_code_type op)
{
	// Cannot start another operation until the previous is done recording
	if (pixel_frame_action_is_recording(pf)) {
		return;
	}

	gs_println("Starting action...");
	// Write op code into buffer
	byte_buffer_write(&pf->undo_buffer, uint32_t, (uint32_t)op);
	pf->current_write_size += sizeof(pixel_frame_action_op_code_type);

	// Clear out redo buffer from previous actions
	byte_buffer_clear(&pf->redo_buffer);
}

void pixel_frame_action_end_record(pixel_frame_t* pf)
{
	if (!pixel_frame_action_is_recording(pf)) {
		return;
	}

	if (pf->current_write_size > sizeof(pixel_frame_action_op_code_type)) {
		// Write current write size into buffer then reset write size
		byte_buffer_write(&pf->undo_buffer, uint32_t, pf->current_write_size);
	}
	else {
		pf->undo_buffer.position -= sizeof(pixel_frame_action_op_code_type);	
		pf->undo_buffer.size -= sizeof(pixel_frame_action_op_code_type);	
	}

	gs_printf("Ending action: sz: %zu, ", pf->current_write_size);
    gs_printf("undo_size: %zu, ", pf->undo_buffer.size);
    gs_printf("undo_cap: %zu, ", pf->undo_buffer.capacity);
    gs_println("redo_cap: %zu", pf->redo_buffer.capacity);

	pf->current_write_size = 0;
}

// Writing actions pushing ops into undo buffer
void pixel_frame_write_action_op(pixel_frame_t* pf, pixel_frame_action_write_t data)
{
	if (!pixel_frame_action_is_recording(pf)) {
		
		return;
	}

	// Write pixels...okie dokie, what does that data look like?
	// Do I need to keep track of previous pixel data? Not really, right? Because we know the starting state for all pixel data. It's memset to nothing.
	// Push data into byte buffer
	byte_buffer_write(&pf->undo_buffer, pixel_frame_action_write_t, data);
	pf->current_write_size += sizeof(data);
}

void pixel_frame_undo_action(pixel_frame_t* pf)
{
	if (pixel_frame_action_is_recording(pf)) {

		return;
	}

	if (pf->undo_buffer.position == 0)
	{
		return;
	}

	// To undo, we'll transfer an action from the undo buffer into the redo buffer, assuming there is data to be read
	// Move position back to read size from buffer
	pf->undo_buffer.position -= sizeof(uint32_t);

	uint32_t sz;
	byte_buffer_read(&pf->undo_buffer, uint32_t, &sz);

	// Move back in buffer entire write size now, including size variable
	pf->undo_buffer.position -= (sz + sizeof(uint32_t));

	// Save current position. We'll use this reset ourselves after applying undo.
	uint32_t start_position = pf->undo_buffer.position;

	pixel_frame_action_op_code_type op;
	byte_buffer_read(&pf->undo_buffer, uint32_t, &op);

	switch (op)
	{
		case pixel_frame_action_op_write_pixels:
		{
			const uint32_t num_pixels = (sz - sizeof(uint32_t)) / sizeof(pixel_frame_action_write_t);

			// Write op into redo buffer
			byte_buffer_write(&pf->redo_buffer, uint32_t, pixel_frame_action_op_write_pixels);

			// Write data into redo buffer and reset pixels before action
			for (u32 i = 0; i < num_pixels; ++i)
			{
				pixel_frame_action_write_t wd;
				byte_buffer_read(&pf->undo_buffer, pixel_frame_action_write_t, &wd);

				// Compute new delta based on this change
				gs_color_t c = pf->pixel_data[ wd.idx ];

				u8 r = wd.r; 
				u8 g = wd.g; 
				u8 b = wd.b; 
				u8 a = wd.a;

				gs_color_t p;
				p.r = (wd.sign_bits & 0x01) ? c.r + r : c.r - r;
				p.g = (wd.sign_bits & 0x02) ? c.g + g : c.g - g;
				p.b = (wd.sign_bits & 0x04) ? c.b + b : c.b - b;
				p.a = (wd.sign_bits & 0x08) ? c.a + a : c.a - a;

				pf->pixel_data[ wd.idx ] = p;

				// Calculate new delta for redo
				wd.sign_bits = 0x0;
				wd.sign_bits |= c.r > p.r ? 0x01 : 0x0;
				wd.sign_bits |= c.g > p.g ? 0x02 : 0x0;
				wd.sign_bits |= c.b > p.b ? 0x04 : 0x0;
				wd.sign_bits |= c.a > p.a ? 0x08 : 0x0;

				byte_buffer_write(&pf->redo_buffer, pixel_frame_action_write_t, wd);
			}

			// Write total size into redo buffer
			byte_buffer_write(&pf->redo_buffer, uint32_t, sz);

			// Set back to position before this action was recorded
			pf->undo_buffer.position = start_position;
		} break;

		default:
		{
			gs_assert(false);
		} break;
	}
}

typedef struct texture_t {
	uint32_t width;
	uint32_t height;
	uint32_t num_comps;
	void* pixels;
} texture_t;

void pixel_frame_redo_action(pixel_frame_t* pf)
{
 	if (pixel_frame_action_is_recording(pf)) {

		return;
	}

	if (pf->redo_buffer.position == 0)
	{
		return;
	}

	// Same as undo
	pf->redo_buffer.position -= sizeof(uint32_t);

	uint32_t sz;
	byte_buffer_read(&pf->redo_buffer, uint32_t, &sz);

	// Move back in buffer entire write size now, including size variable
	pf->redo_buffer.position -= (sz + sizeof(uint32_t));

	// Save current position. We'll use this reset ourselves after applying undo.
	uint32_t start_position = pf->redo_buffer.position;

	pixel_frame_action_op_code_type op;
	byte_buffer_read(&pf->redo_buffer, uint32_t, &op);

	switch (op)
	{
		case pixel_frame_action_op_write_pixels:
		{
			const uint32_t num_pixels = (sz - sizeof(uint32_t)) / sizeof(pixel_frame_action_write_t);

			// Write op into undo buffer
			byte_buffer_write(&pf->undo_buffer, uint32_t, pixel_frame_action_op_write_pixels);

			// Write data into redo buffer and reset pixels before action
			for (u32 i = 0; i < num_pixels; ++i)
			{
				pixel_frame_action_write_t wd;
				byte_buffer_read(&pf->redo_buffer, pixel_frame_action_write_t, &wd);

				// Compute new delta based on this change
				gs_color_t c = pf->pixel_data[ wd.idx ];

				u8 r = wd.r; 
				u8 g = wd.g; 
				u8 b = wd.b; 
				u8 a = wd.a; 

				gs_color_t p;
				p.r = (wd.sign_bits & 0x01) ? c.r + r : c.r - r;
				p.g = (wd.sign_bits & 0x02) ? c.g + g : c.g - g;
				p.b = (wd.sign_bits & 0x04) ? c.b + b : c.b - b;
				p.a = (wd.sign_bits & 0x08) ? c.a + a : c.a - a;

				pf->pixel_data[ wd.idx ] = p;

				// Calculate new delta for redo
				wd.sign_bits = 0x0;
				wd.sign_bits |= c.r > p.r ? 0x01 : 0x0;
				wd.sign_bits |= c.g > p.g ? 0x02 : 0x0;
				wd.sign_bits |= c.b > p.b ? 0x04 : 0x0;
				wd.sign_bits |= c.a > p.a ? 0x08 : 0x0;

				byte_buffer_write(&pf->undo_buffer, pixel_frame_action_write_t, wd);
			}

			// Write total size into redo buffer
			byte_buffer_write(&pf->undo_buffer, uint32_t, sz);

			// Set back to position before this action was recorded
			pf->redo_buffer.position = start_position;
		} break;

		default:
		{
			gs_assert(false);
		} break;
	}
}

gs_vec2 pixel_frame_calculate_mouse_position(pixel_frame_t* pf, gs_camera_t* camera)
{
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());
	gs_vec2 pmp = gs_platform_mouse_positionv();
	gs_vec3 _tpmp = gs_camera_screen_to_world(camera, (gs_vec3){pmp.x, pmp.y, 0.f}, (s32)ws.x, (s32)ws.y);

	gs_vec2 tpmp = (gs_vec2){_tpmp.x, _tpmp.y};
	tpmp.x = tpmp.x * 0.5f + 0.5f;
	tpmp.y = tpmp.y * 0.5f + 0.5f;
	tpmp.y = gs_map_range(1.f, 0.f, 0.f, 1.f, tpmp.y);

	// Need to place mouse into frame
	f32 x_scale = tpmp.x;
	f32 y_scale = tpmp.y;
    f32 pw = x_scale * (f32)pf->pixel_frame_width;
    f32 ph = y_scale * (f32)pf->pixel_frame_height;
	return (gs_vec2){x_scale * (f32)pf->pixel_frame_width, y_scale * (f32)pf->pixel_frame_height};
}

int32_t pixel_frame_compute_idx_from_position(pixel_frame_t* pf, int32_t x, int32_t y)
{
	if (x >= pf->pixel_frame_width || y >= pf->pixel_frame_height || x < 0 || y < 0 ) {
		return -1;
	}

	return (y * pf->pixel_frame_width + x);
}

void pixel_frame_draw_pixel(void* data, gs_color_t color, int32_t x, int32_t y)
{
	pixel_frame_t* pf = (pixel_frame_t*)data;

	if (x < 0 || x >= pf->pixel_frame_width || y < 0 || y >= pf->pixel_frame_height)
		return;

	int32_t idx = pixel_frame_compute_idx_from_position(pf, x, y);

	if (idx != -1) {

		// Get converted color from color picker
		gs_color_t c = color;
		gs_color_t p = pf->pixel_data[ idx ];

		// Compute delta and signs
		uint8_t sign_bits = 0x0;
		uint8_t r = (uint8_t)abs((int32_t)c.r - (int32_t)p.r);
		uint8_t g = (uint8_t)abs((int32_t)c.g - (int32_t)p.g);
		uint8_t b = (uint8_t)abs((int32_t)c.b - (int32_t)p.b);
		uint8_t a = (uint8_t)abs((int32_t)c.a - (int32_t)p.a);

		sign_bits |= c.r < p.r ? 0x01 : 0x0;
		sign_bits |= c.g < p.g ? 0x02 : 0x0;
		sign_bits |= c.b < p.b ? 0x04 : 0x0;
		sign_bits |= c.a < p.a ? 0x08 : 0x0;

		pixel_frame_action_write_t wd = 
		{
			(uint16_t)idx, 
			r, g, b, a,
			sign_bits
		};

		// Only write if there's an actual change
		if (!color_equal(c, p)) {

			pixel_frame_write_action_op(pf, wd);
			pf->pixel_data[ idx ] = color;
		}
	}
}

void pixel_frame_draw_ui_pixel(void* data, gs_color_t color, int32_t x, int32_t y)
{
	pixel_frame_t* pf = (pixel_frame_t*)data;

	if (x >= ui_frame_width || x < 0 || y >= ui_frame_height || y < 0) 
		return;

	uint32_t idx = y * ui_frame_width + x;
		
	// Get converted color from color picker
	gs_color_t c = (gs_color_t){g_nk_color.r * 255, g_nk_color.g * 255, g_nk_color.b * 255, g_nk_color.a * 100};
	pf->ui_data[ idx ] = color;
}

void pixel_frame_clear_data(pixel_frame_t* pf)
{
	if (!pixel_frame_action_is_recording(pf)) 
	{
		// Start record
		pixel_frame_action_start_record(pf, pixel_frame_action_op_write_pixels);

		const gs_color_t clear_color = (gs_color_t){ 0, 0, 0, 0 };

		// Clear all data
		for (uint32_t h = 0; h < pf->pixel_frame_height; ++h) {
			for (uint32_t w = 0; w < pf->pixel_frame_width; ++w) {

				pixel_frame_draw_pixel(pf, clear_color, w, h);
			}
		}

		// End record
		pixel_frame_action_end_record(pf);
	}
	// memset(pf->pixel_data, 0, pf->pixel_frame_width * pf->pixel_frame_height * sizeof(gs_color_t));
}


void pixel_frame_save_image_to_disk(pixel_frame_t* pf)
{
    /*
	if (pixel_frame_action_is_recording(pf)) {
		return;
	}

	// We'll just write it to 'test.bin'
	byte_buffer_t buffer = byte_buffer_new();

	// Serialize all pixel frame data to disk
	for (uint32_t h = 0; h < pf->pixel_frame_height; ++h)
	{
		for (uint32_t w = 0; w < pf->pixel_frame_width; ++w)
		{
			uint32_t idx = h * pf->pixel_frame_width + w;
			byte_buffer_write(&buffer, uint8_t, pf->pixel_data[ idx ].r);
			byte_buffer_write(&buffer, uint8_t, pf->pixel_data[ idx ].g);
			byte_buffer_write(&buffer, uint8_t, pf->pixel_data[ idx ].b);
			byte_buffer_write(&buffer, uint8_t, pf->pixel_data[ idx ].a);
		}
	}

	const char* filter_patterns[] = {
		"*.pix"
	};

	const char* file_name = tinyfd_saveFileDialog(
		"Save Image", 
		"./image.pix", 
		sizeof(filter_patterns) / sizeof(char*), 
		filter_patterns, 
		NULL 
	);

	// Write to file if valid
	if (file_name) {
		byte_buffer_write_to_file(&buffer, file_name);
	}

	byte_buffer_free(&buffer);
    */

}

void pixel_frame_load_image_from_disk(pixel_frame_t* pf)
{
    return;

    /*
	if (pixel_frame_action_is_recording(pf)) {
		return;
	}

	const char* filter_patterns[] = {
		"*.pix", "*.png"
	};

	const char* file_name = tinyfd_openFileDialog(
		"Open Image", 
		"./", 
		sizeof(filter_patterns) / sizeof(char*),
		filter_patterns,
		NULL,
		false
	);

	if (file_name == NULL) {
		return;
	}

	if (!gs_platform_file_exists(file_name)) {
		return;
		}

	char file_ext[256];
	gs_util_get_file_extension(file_ext, sizeof(file_ext), file_name);

	if (gs_string_compare_equal(file_ext, "pix"))
	{
		byte_buffer_t buffer = byte_buffer_new();

		byte_buffer_read_from_file(&buffer, file_name);

		// Serialize all pixel frame data to disk
		for (uint32_t h = 0; h < pf->pixel_frame_height; ++h)
		{
			for (uint32_t w = 0; w < pf->pixel_frame_width; ++w)
			{
				uint32_t idx = h * pf->pixel_frame_width + w;
				byte_buffer_read(&buffer, uint8_t, &pf->pixel_data[idx].r);
				byte_buffer_read(&buffer, uint8_t, &pf->pixel_data[idx].g);
				byte_buffer_read(&buffer, uint8_t, &pf->pixel_data[idx].b);
				byte_buffer_read(&buffer, uint8_t, &pf->pixel_data[idx].a);
			}
		}

		// Clear out undo/redo buffers
		byte_buffer_clear(&pf->undo_buffer);
		byte_buffer_clear(&pf->redo_buffer);
		byte_buffer_free(&buffer);
	}
	else if (gs_string_compare_equal(file_ext, "png"))
	{
		// Need to load in .png and then change size of texture...	
	}
    */
}

b32 pixel_frame_hovering_ui_window()
{
	gs_vec2 mp = gs_platform_mouse_positionv();
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());

	uint32_t sw = 250.f;

	return mp.x < sw;
}

// Drag/drop texture into frame?
// Save/Load images
// Change colors	(might just have a set number of colors - not sure how I'd represent 32 bit colors without some
// 						complicated gui mechanism)
// Radius for painting

void pixel_frame_update(gs_command_buffer_t* cb, pixel_frame_t* pf, gs_camera_t* camera)
{
	b32 need_update = false;
	static gs_vec2 original_mouse_position;

	b32 alt_down = gs_platform_key_down(GS_KEYCODE_LEFT_ALT);
	b32 space_down = gs_platform_key_down(GS_KEYCODE_SPACE);

	gs_vec2 mp = pixel_frame_calculate_mouse_position(pf, camera);
	const uint32_t mp_x = (uint32_t)mp.x;
	const uint32_t mp_y = pf->pixel_frame_height - (uint32_t)mp.y - 1;
	const int32_t idx = pixel_frame_compute_idx_from_position(pf, mp_x, mp_y);

	// Painting operation
	if (gs_platform_mouse_down(GS_MOUSE_LBUTTON) && !pixel_frame_hovering_ui_window()) 
	{
		if (!alt_down && !space_down)
		{
			if (!pf->mouse_down) {
				pf->mouse_down = true;

				// Start recording
				pixel_frame_action_start_record(pf, pixel_frame_action_op_write_pixels);	
			}

			if (idx != -1) {

				// Get converted color from color picker
				gs_color_t col = (gs_color_t){g_nk_color.r * 255, g_nk_color.g * 255, g_nk_color.b * 255, g_nk_color.a * 255};

				if (pf->paint_radius > 1) {
					circleBres(pf, col, true, mp.x, pf->pixel_frame_height - mp.y, pf->paint_radius, &pixel_frame_draw_pixel);
				} else {
					pixel_frame_draw_pixel(pf, col, mp_x, mp_y);
				}
			}
		}
		// Color picking
		else if (alt_down)
		{
			if (idx != -1) {

				gs_color_t c = pf->pixel_data[idx];
				g_nk_color.r = (f32)c.r / 255.f;
				g_nk_color.g = (f32)c.g / 255.f;
				g_nk_color.b = (f32)c.b / 255.f;
				g_nk_color.a = (f32)c.a / 255.f;
			}
		}
		// Camera pan
		else if (space_down)
		{
			pan_camera();
		}
	}

	// Need to record actions here
	if (gs_platform_mouse_released(GS_MOUSE_LBUTTON))
	{
		// Start recording
		pixel_frame_action_end_record(pf);
		pf->mouse_down = false;
	}

	// Paint radius
	if (gs_platform_key_pressed(GS_KEYCODE_LEFT_BRACKET))
	{
		pf->paint_radius = gs_max((int32_t)1, (int32_t)pf->paint_radius - 1);
	}

	if (gs_platform_key_pressed(GS_KEYCODE_RIGHT_BRACKET))
	{
		pf->paint_radius = gs_min(20, pf->paint_radius + 1);
	}

	if (gs_platform_key_down(GS_KEYCODE_LEFT_CONTROL) && gs_platform_key_pressed(GS_KEYCODE_Z))
	{
		pixel_frame_undo_action(pf);
	}

	if (gs_platform_key_down(GS_KEYCODE_LEFT_CONTROL) && gs_platform_key_pressed(GS_KEYCODE_Y))
	{
		pixel_frame_redo_action(pf);
	}

	if (gs_platform_key_down(GS_KEYCODE_LEFT_CONTROL) && gs_platform_key_pressed(GS_KEYCODE_S))
	{
		pixel_frame_save_image_to_disk(pf);
	}

	if (gs_platform_key_down(GS_KEYCODE_LEFT_CONTROL) && gs_platform_key_pressed(GS_KEYCODE_O))
	{
		pixel_frame_load_image_from_disk(pf);
	}

	if (gs_platform_key_pressed(GS_KEYCODE_C))
	{
		pixel_frame_clear_data(pf);
	}

	// Update GPU texture data
	gs_graphics_texture_desc_t desc = pixel_frame_texture_parameter_desc();
	desc.width = pf->pixel_frame_width;
	desc.height = pf->pixel_frame_height;
	desc.data = pf->pixel_data;

	// Update texture data
	gs_graphics_texture_request_update(cb, pf->texture, &desc);

	// Draw ui into buffer
	memset(pf->ui_data, 0, ui_frame_width * ui_frame_height * sizeof(gs_color_t));
	const gs_color_t col = (gs_color_t){ (u8)g_nk_color.r * 255, (u8)g_nk_color.g * 255, (u8)g_nk_color.b * 255, 50 };
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());
	gs_vec2 m_pos = gs_platform_mouse_positionv();
	f32 scl_x = m_pos.x / ws.x;
	f32 scl_y = m_pos.y / ws.y;
	if (pf->paint_radius > 0) {
		circleBres(pf, col, false, mp.x, ui_frame_height - mp.y, pf->paint_radius, &pixel_frame_draw_ui_pixel);
	}

	desc = pixel_frame_texture_parameter_desc();
	desc.width = ui_frame_width;
	desc.height = ui_frame_height;
	desc.data = pf->ui_data;
	gs_graphics_texture_request_update(cb, pf->ui_texture, &desc);
}

void pixel_frame_render(pixel_frame_t* pf, gs_command_buffer_t* cb, gs_camera_t* camera)
{
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());
	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());

	// Set up binds
	gs_mat4 proj_mtx = gs_camera_get_proj(camera, ws.x, ws.y);
	gs_mat4 view_mtx = gs_camera_get_view(camera);

    // Render pass action for clearing the screen
    gs_graphics_clear_desc_t clear = (gs_graphics_clear_desc_t){
        .actions = &(gs_graphics_clear_action_t){.color = {0.5f, 0.5f, 0.5f, 1.f}}
    };

    // Binding descriptor for vertex buffer
    gs_graphics_bind_desc_t binds = {
        .vertex_buffers = {.desc = &(gs_graphics_bind_vertex_buffer_desc_t){.buffer = pf->vbo}},
        .index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){.buffer = pf->ibo}},
        .uniforms = {
        	.desc = (gs_graphics_bind_uniform_desc_t[]) {
        		{.uniform = pf->u_proj, .data = &proj_mtx},	
        		{.uniform = pf->u_view, .data = &view_mtx}
        	}, 
        	.size = 2 * sizeof(gs_graphics_bind_uniform_desc_t)}
    };

    /* Render */
    gs_graphics_begin_render_pass(cb, GS_GRAPHICS_RENDER_PASS_DEFAULT);

    	gs_graphics_set_viewport(cb, 0, 0, (uint32_t)fbs.x, (uint32_t)fbs.y);

        gs_graphics_clear(cb, &clear);

        gs_graphics_bind_pipeline(cb, pf->pip);

        gs_graphics_apply_bindings(cb, &binds);

        // Render background texture 
        {
        	binds = (gs_graphics_bind_desc_t) {.uniforms = {.desc = &(gs_graphics_bind_uniform_desc_t){.uniform = pf->u_tex, .data = &pf->bg_texture, .binding = 0}}};
            gs_graphics_apply_bindings(cb, &binds);
	        gs_graphics_draw(cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 6});
        }

        // Render paint texture 
        {
        	binds = (gs_graphics_bind_desc_t) {.uniforms = {.desc = &(gs_graphics_bind_uniform_desc_t){.uniform = pf->u_tex, .data = &pf->texture, .binding = 0}}};
            gs_graphics_apply_bindings(cb, &binds);
	        gs_graphics_draw(cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 6});
        }

        // Render ui texture 
        {
        	binds = (gs_graphics_bind_desc_t) {.uniforms = {.desc = &(gs_graphics_bind_uniform_desc_t){.uniform = pf->u_tex, .data = &pf->ui_texture, .binding = 0}}};
            gs_graphics_apply_bindings(cb, &binds);
	        gs_graphics_draw(cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 6});
        }

    gs_graphics_end_render_pass(cb);
}

//================================================================

// Globals
gs_global gs_command_buffer_t g_cb = {0};
gs_global pixel_frame_t g_pixel_frame = {0};
gs_global gs_camera_t g_camera = {0};

// Forward Decls.
void app_init();		// Use to init your application
void app_update();		// Use to update your application
void app_shutdown();	// Use to shutdown your appliaction

typedef struct test_t
{
	u32 unsigned_val;
	s32 signed_val;
	f32 float_val;
	f64 double_val;
} test_t;

void print_test(test_t* t) {
	gs_println("test: { %zu, %d, %.2f, %.5f }", 
		t->unsigned_val, t->signed_val, t->float_val, t->double_val);
}

gs_app_desc_t gs_main(int argc, char** argv)
{
	return (gs_app_desc_t) {
		.init = app_init,
		.update = app_update,
		.shutdown = app_shutdown,
		.window_title = "Serialization",
		.window_width = 1200,
		.window_height = 600
	};
}

void byte_buffer_test()
{
	test_t t0 = {0};
	t0.unsigned_val = 5;
	t0.signed_val = 20;
	t0.float_val = -3.145f;
	t0.double_val = 2.37854;

	gs_byte_buffer_t buffer = gs_byte_buffer_new();

	// Serialize our test data structure into our buffer
    gs_byte_buffer_write(&buffer, test_t, t0);

	// Seek back to beginning
	gs_byte_buffer_seek_to_beg(&buffer);

	// Read values into our new test data
	test_t t1 = {0};
    gs_byte_buffer_read(&buffer, test_t, &t1);

	print_test(&t1); 

	// Write to file
	gs_byte_buffer_write_to_file(&buffer, "./test.bin");

	// Read back from file
	gs_byte_buffer_read_from_file(&buffer, "./test.bin");

	// Read back into new object
	test_t t2 = {0};

	// Read back entire object from memory
    gs_byte_buffer_read(&buffer, test_t, &t2);

	print_test(&t2);

	// Clean up byte buffer memory
	gs_byte_buffer_free(&buffer);
}

// Here, we'll initialize all of our application data, which in this case is our graphics resources
void app_init()
{
	byte_buffer_test();

	g_pixel_frame = pixel_frame_new();

	// Construct command buffer (the command buffer is used to allow for immediate drawing at any point in our program)
	g_cb = gs_command_buffer_new();

	// Construct camera parameters
    g_camera =  gs_camera_default();
	// g_camera.transform = gs_vqs_default();
	// g_camera.transform.position = (gs_vec3){-1.f, 0.f, -1.f};
	// g_camera.fov = 60.f;
	// g_camera.near_plane = 0.1f;
	// g_camera.far_plane = 1000.f;
	// g_camera.ortho_scale = 1.f;
	// g_camera.proj_type = GS_PROJECTION_TYPE_ORTHOGRAPHIC;

	nk_init_ui();
}

void app_update()
{
	// Main window size
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());
	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());

	// If we press the escape key, exit the application
	if (gs_platform_key_pressed(GS_KEYCODE_ESC)) {
		gs_engine_quit();
	}

	const f32 dt = gs_platform_delta_time();
	const f32 t = gs_platform_elapsed_time();

	/*=================
	// Camera controls
	==================*/

	if (gs_platform_key_down(GS_KEYCODE_Q)) {
		g_camera.ortho_scale += 0.1f;
	}
	if (gs_platform_key_down(GS_KEYCODE_E)) {
		g_camera.ortho_scale -= 0.1f;
	}

	if (gs_platform_key_down(GS_KEYCODE_A)) {
		g_camera.transform.position.x -= 0.1f;
	}
	if (gs_platform_key_down(GS_KEYCODE_D)) {
		g_camera.transform.position.x += 0.1f;
	}
	if (gs_platform_key_down(GS_KEYCODE_W)) {
		g_camera.transform.position.y += 0.1f;
	}
	if (gs_platform_key_down(GS_KEYCODE_S)) {
		g_camera.transform.position.y -= 0.1f;
	}
	if (gs_platform_mouse_down(GS_MOUSE_MBUTTON))
	{
		// Need to transform the camera's position by the transformation of the mouse in world space only...I think
		gs_vec2 md = gs_platform_mouse_deltav();
		gs_vec2 mc = gs_platform_mouse_positionv();
		gs_vec3 wc = (gs_vec3){mc.x, mc.y, 0.f};
		gs_vec3 pmp = gs_camera_screen_to_world(&g_camera, wc, (s32)ws.x, (s32)ws.y);
		gs_vec3 pmp_d = gs_camera_screen_to_world(&g_camera, (gs_vec3){mc.x + md.x, mc.y + md.y, 0.f}, (s32)ws.x, (s32)ws.y);
		gs_vec3 delta = gs_vec3_sub(pmp, pmp_d);

		g_camera.transform.position.x += delta.x;
		g_camera.transform.position.y += delta.y;
	}

	f32 mwx, mwy;
	gs_platform_mouse_wheel(&mwx, &mwy);
	g_camera.ortho_scale = gs_clamp(g_camera.ortho_scale - mwy * 0.2f, 0.2f, 10.f);

	/*===============
	// Update scene
	================*/

	pixel_frame_update(&g_cb, &g_pixel_frame, &g_camera);

	/*===============
	// Render scene
	================*/

	// Graphics api instance
	gs_command_buffer_t* cb = &g_cb;

	// Render to painting backbuffer
	pixel_frame_render(&g_pixel_frame, cb, &g_camera);

	// Handle ui
	nk_do_ui(cb, &g_pixel_frame);

	// Submit command buffer for rendering
	gs_graphics_submit_command_buffer(cb);
}

void app_shutdown()
{
	gs_println("Goodbye, Gunslinger.");
}

void pan_camera()
{
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());

	gs_vec2 md = gs_platform_mouse_deltav();
	gs_vec2 mc = gs_platform_mouse_positionv();
	gs_vec3 wc = (gs_vec3){mc.x, mc.y, 0.f};
	gs_vec3 pmp = gs_camera_screen_to_world(&g_camera, wc, ws.x, ws.y);
	gs_vec3 pmp_d = gs_camera_screen_to_world(&g_camera, (gs_vec3){mc.x + md.x, mc.y + md.y, 0.f}, ws.x, ws.y);
	gs_vec3 delta = gs_vec3_sub(pmp, pmp_d);

	g_camera.transform.position.x += delta.x;
	g_camera.transform.position.y += delta.y;
}

void nk_init_ui()
{
    // Initialize gs_nk context
	g_nk_color = (struct nk_colorf){0.f, 0.f, 0.f, 1.f};
    gs_nk_init(&gs_nk, gs_platform_main_window(), 0x00);
    // Default font (can add different ones in here)
    gs_nk_font_stash_begin(&gs_nk, NULL);
    gs_nk_font_stash_end(&gs_nk);
}

void nk_do_ui(gs_command_buffer_t* cb, struct pixel_frame_t* pf)
{
	gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());

	// Do the ui stuff
    gs_nk_new_frame(&gs_nk);

    struct nk_context* ctx = &gs_nk.nk_ctx;

    /* GUI */
    if (nk_begin(ctx, "Demo", nk_rect(0, 0, 250, ws.y), NK_WINDOW_BORDER))
    {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Color", NK_TEXT_LEFT);
        {
            nk_layout_row_dynamic(ctx, 120, 1);

            g_nk_color = nk_color_picker(ctx, g_nk_color, NK_RGBA);

            nk_layout_row_dynamic(ctx, 25, 1);
            g_nk_color.r = nk_propertyf(ctx, "#R:", 0, g_nk_color.r, 1.0f, 0.01f,0.005f);
            g_nk_color.g = nk_propertyf(ctx, "#G:", 0, g_nk_color.g, 1.0f, 0.01f,0.005f);
            g_nk_color.b = nk_propertyf(ctx, "#B:", 0, g_nk_color.b, 1.0f, 0.01f,0.005f);
            g_nk_color.a = nk_propertyf(ctx, "#A:", 0, g_nk_color.a, 1.0f, 0.01f,0.005f);
        }

        // nk_layout_row_begin(ctx, NK_STATIC, 2, 3);
        {
            g_pixel_frame.paint_radius = nk_propertyi(ctx, "Paint Radius", 0, g_pixel_frame.paint_radius, 20, 1, 0.8f);

	        // nk_layout_row_static(ctx, 30, 80, 1);
	        if (nk_button_label(ctx, "Save"))
	        {
	        	pixel_frame_save_image_to_disk(pf);
	        }

	        // nk_layout_row_static(ctx, 30, 80, 1);
	        if (nk_button_label(ctx, "Load"))
	        {
	        	pixel_frame_load_image_from_disk(pf);
	        }
        }
        // nk_layout_row_end(ct);
    }
    nk_end(ctx);

    // nk_glfw3_render(&gs_nk, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);

    // Render gs_nk commands into graphics command buffer
    gs_nk_render(&gs_nk, cb, NK_ANTI_ALIASING_ON, NULL);
}






















