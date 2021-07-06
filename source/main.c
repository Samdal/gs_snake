#define GS_IMPL
#include <gs/gs.h>

// Vertex data
float v_data[] = {
    -1.0f, -1.0f,  0.0f, 0.0f,  // Top Left
     1.0f, -1.0f,  1.0f, 0.0f,  // Top Right
    -1.0f,  1.0f,  0.0f, 1.0f,  // Bottom Left
     1.0f,  1.0f,  1.0f, 1.0f   // Bottom Right
};
// Index data for quad
uint32_t i_data[] = {
    0, 3, 2,    // First Triangle
    0, 1, 3     // Second Triangle
};

gs_command_buffer_t                      cb      = {0};
gs_handle(gs_graphics_vertex_buffer_t)   vbo     = {0};
gs_handle(gs_graphics_index_buffer_t)    ibo     = {0};
gs_handle(gs_graphics_pipeline_t)        pip     = {0};
gs_handle(gs_graphics_shader_t)          shader  = {0};
gs_handle(gs_graphics_uniform_t)         u_resolution = {0};
gs_handle(gs_graphics_uniform_t)         u_food   = {0};
gs_handle(gs_graphics_uniform_t)         u_map   = {0};

char map_names[32][256] = {0};
uint32_t map_buffer[32] = {0};

// snake linked list
typedef struct snake {
        gs_vec2 pos;
        void* next;
} snake_t;
snake_t* tail = &(snake_t){0};

gs_vec2 food = {16.0f, 16.0f};

gs_vec2 prev_move = {0};
gs_vec2 move = {0};
uint32_t prev_move_time = 0;

snake_t* eat_food(snake_t* head)
{
        // move food
        map_buffer[(int)food.y] |= (1 << (int)food.x);
        do {
                food = (gs_vec2){rand() % 32, rand() % 32};
        } while (map_buffer[(int)food.y] & (1 << (int)food.x));

        // Grow snake
        head->next = (snake_t*)gs_malloc(sizeof(snake_t));
        gs_assert(head->next);
        gs_vec2 pos = head->pos;
        head = head->next;
        head->pos = pos;
        head->next = NULL;
        return head;
}

void free_snake()
{
        while (tail->next) {
                snake_t* head = tail;
                for (;;) {
                        snake_t* next = head->next;
                        if (!next->next)
                                break;
                        head = next;
                }
                gs_free(head->next);
                head->next = NULL;
        }
}

void new_game()
{
        // reinitialise buffers
        for (uint32_t i = 0; i < 32; i++)
                map_buffer[i] = 0;
        prev_move_time = gs_platform_elapsed_time() * 0.01f;
        move = (gs_vec2){1.0f, 0.0f};
        prev_move = move;
        free_snake();

        // init tail piece
        snake_t* body = tail;
        body->pos = (gs_vec2){16.0f, 16.0f};

        // place tail piece in map_buffer
        map_buffer[(int)body->pos.y] |= (1 << (int)body->pos.x);

        for (uint32_t i = 1; i <= 3; i++) {
                // create snake piece
                body->next = (snake_t*)gs_malloc(sizeof(snake_t));
                gs_assert(body->next);
                body = body->next;

                // init snake piece
                body->pos = (gs_vec2){16.0f + (float)i, 16.0f};
                body->next = NULL;

                // place snake piece in map_buffer
                map_buffer[(int)body->pos.y] |= (1 << (int)body->pos.x);
        }

        // place food at random position
        do {
                food = (gs_vec2){rand() % 32, rand() % 32};
        } while (map_buffer[(int)food.y] & (1 << (int)food.x));
}

void move_snake()
{
        prev_move = move;
        snake_t* body = tail;

        // get new head pos
        while (body->next)
                body = body->next;
        gs_vec2 new_pos = body->pos;
        new_pos.x += move.x;
        new_pos.y += move.y;
        // wrap snake
        if (new_pos.x > 31.0f)
                new_pos.x = 0.0f;
        else if (new_pos.x < 0.0f)
                new_pos.x = 31.0f;
        else if (new_pos.y > 31.0f)
                new_pos.y = 0.0f;
        else if (new_pos.y < 0.0f)
                new_pos.y = 31.0f;

        // if new pos is food
        if (new_pos.x == food.x && new_pos.y == food.y) {
                body = eat_food(body);
                body->pos = new_pos;
        } else {
                // remove tail from buffer
                body = tail;
                map_buffer[(int)body->pos.y] &= ~(1 << (int)body->pos.x);

                // move each snake piece to the next ones position
                while (body->next) {
                        snake_t* next = body->next;
                        body->pos = next->pos;
                        body = next;
                }

                body->pos = new_pos;

                // check for collision
                if (map_buffer[(int)body->pos.y] & (1 << (int)body->pos.x)) {
                        new_game();
                        return;
                }
                // add head to buffer
                map_buffer[(int)body->pos.y] |= (1 << (int)body->pos.x);
        }
}

void init()
{
        // Construct new command buffer
        cb = gs_command_buffer_new();

        // Construct vertex buffer
        vbo = gs_graphics_vertex_buffer_create(&(gs_graphics_vertex_buffer_desc_t) {
                .data = v_data,
                .size = sizeof(v_data)
        });

        // Construct index buffer
        ibo = gs_graphics_index_buffer_create(&(gs_graphics_index_buffer_desc_t) {
                .data = i_data,
                .size = sizeof(i_data)
        });

        // shader sources
        char* f_src = gs_platform_read_file_contents("./source/frag.glsl", "r", NULL);
        char* v_src = gs_platform_read_file_contents("./source/vertex.glsl", "r", NULL);

        // Create shader
        shader = gs_graphics_shader_create (&(gs_graphics_shader_desc_t) {
                .sources = (gs_graphics_shader_source_desc_t[]){
                        {.type = GS_GRAPHICS_SHADER_STAGE_VERTEX, .source = v_src},
                        {.type = GS_GRAPHICS_SHADER_STAGE_FRAGMENT, .source = f_src}
                },
                .size = 2 * sizeof(gs_graphics_shader_source_desc_t),
                .name = "quad"
        });

        // Construct uniforms

        // Construct layout fields for sub elements of array
        gs_graphics_uniform_layout_desc_t layout[32] = {0};
        for (uint32_t i = 0; i < 32; ++i) {
                gs_snprintf(map_names[i], sizeof(map_names[i]), "[%d]", i);
                layout[i].type = GS_GRAPHICS_UNIFORM_INT;
                layout[i].fname = map_names[i];
        }
        u_map = gs_graphics_uniform_create(&(gs_graphics_uniform_desc_t) {
                .name = "u_map",
                .layout = layout,
                .layout_size = 32 * sizeof(gs_graphics_uniform_layout_desc_t)
        });
        u_resolution = gs_graphics_uniform_create(&(gs_graphics_uniform_desc_t) {
                .name = "u_resolution",
                .layout = &(gs_graphics_uniform_layout_desc_t) {
                        .type = GS_GRAPHICS_UNIFORM_VEC2
                }
        });
        u_food = gs_graphics_uniform_create(&(gs_graphics_uniform_desc_t) {
                .name = "u_food",
                .layout = &(gs_graphics_uniform_layout_desc_t) {
                        .type = GS_GRAPHICS_UNIFORM_VEC2
                }
        });

        // init pipeline
        pip = gs_graphics_pipeline_create (&(gs_graphics_pipeline_desc_t) {
                .raster = {
                        .shader = shader,
                        .index_buffer_element_size = sizeof(uint32_t)
                },
                .layout = {
                        .attrs = (gs_graphics_vertex_attribute_desc_t[]){
                                {.format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "a_pos"},
                                {.format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "a_uv"}
                        },
                        .size = 2 * sizeof(gs_graphics_vertex_attribute_desc_t)
                }
        });

        // set random seed
        srand((uint32_t)time(NULL));

        // start game
        new_game();
}

void update()
{
        // inputs
        if (gs_platform_key_pressed(GS_KEYCODE_ESC)) gs_engine_quit();
        if (gs_platform_key_pressed(GS_KEYCODE_RIGHT)     && prev_move.y != 0.0f) move = (gs_vec2){1.0f,0.0f};
        else if (gs_platform_key_pressed(GS_KEYCODE_LEFT) && prev_move.y != 0.0f) move = (gs_vec2){-1.0f,0.0f};
        else if (gs_platform_key_pressed(GS_KEYCODE_UP)   && prev_move.x != 0.0f) move = (gs_vec2){0.0f,1.0f};
        else if (gs_platform_key_pressed(GS_KEYCODE_DOWN) && prev_move.x != 0.0f) move = (gs_vec2){0.0f,-1.0f};

        // move snake every 0.2 seconds
        uint32_t time_now = gs_platform_elapsed_time() * 0.01f;
        if (time_now - prev_move_time >= 2) {
                move_snake();
                prev_move_time = time_now;
        }

        // set uniforms
        gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
        gs_graphics_bind_uniform_desc_t uniforms[] = {
                (gs_graphics_bind_uniform_desc_t) {.uniform = u_map, .data = &map_buffer},
                (gs_graphics_bind_uniform_desc_t) {.uniform = u_resolution, .data = &fbs},
                (gs_graphics_bind_uniform_desc_t) {.uniform = u_food, .data = &food},
        };

        // Bindings for all buffers: vertex, index, uniforms
        gs_graphics_bind_desc_t binds = {
                .vertex_buffers = {.desc = &(gs_graphics_bind_vertex_buffer_desc_t){.buffer = vbo}},
                .index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){.buffer = ibo}},
                .uniforms = {.desc = uniforms, .size = sizeof(uniforms)}
        };

        /* Render */
        gs_graphics_begin_render_pass(&cb, GS_GRAPHICS_RENDER_PASS_DEFAULT);
        gs_graphics_set_viewport(&cb, 0, 0, (int32_t)fbs.x, (int32_t)fbs.y);
        gs_graphics_bind_pipeline(&cb, pip);
        gs_graphics_apply_bindings(&cb, &binds);
        gs_graphics_draw(&cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 6});
        gs_graphics_end_render_pass(&cb);

        // Submit command buffer (syncs to GPU, MUST be done on main thread where you have your GPU context created)
        gs_graphics_submit_command_buffer(&cb);
}

gs_app_desc_t gs_main(int32_t argc, char** argv)
{
        return (gs_app_desc_t){
                .window_title = "Gunslinger Snake",
                .init = init,
                .update = update,
                .shutdown = free_snake,
        };
}
