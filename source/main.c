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

char map_names[32][32] = {0};
uint32_t map_buffer[32] = {0};
typedef union {
        int32_t xy[2];
        struct {
                int32_t x, y;
        };
} ivec;

ivec snake[32 * 32] = {0};
uint32_t head = 3;

gs_vec2 food = {16.0f, 16.0f};

ivec prev_move = {0};
ivec move = {0};
uint32_t prev_move_time = 0;

void new_game()
{
        // reinitialise buffers
        for (uint32_t i = 0; i < 32; i++)
                map_buffer[i] = 0;
        for (uint32_t i = 0; i < 32 * 32; i++)
                snake[i] = (ivec){0};
        prev_move_time = gs_platform_elapsed_time() * 0.01f;
        move = (ivec){1, 0};
        prev_move = move;
        head = 3;

        // place snake in map_buffer
        for (uint32_t i = 0; i <= 3; i++) {
                snake[i] = (ivec){16+i,16};
                map_buffer[snake[i].y] |= (1 << snake[i].x);
        }

        // place food at random position
        do {
                food = (gs_vec2){rand() % 32, rand() % 32};
        } while (map_buffer[(int)food.y] & (1 << (int)food.x));
}

void move_snake()
{
        gs_printf("FPS: %f\n", gs_engine_subsystem(platform)->time.frame);
        prev_move = move;
        ivec new_pos = {snake[head].x + move.x, snake[head].y + move.y};

        // wrap snake
        if (new_pos.x > 31)      new_pos.x = 0;
        else if (new_pos.x < 0)  new_pos.x = 31;
        else if (new_pos.y > 31) new_pos.y = 0;
        else if (new_pos.y < 0)  new_pos.y = 31;

        // if new pos is food
        if (new_pos.x == (int)food.x && new_pos.y == (int)food.y) {
                // add head to buffer
                head++;
                snake[head] = new_pos;
                map_buffer[new_pos.y] |= (1 << new_pos.x);

                // place food in a random place
                do {
                        food = (gs_vec2){rand() % 32, rand() % 32};
                } while (map_buffer[(int)food.y] & (1 << (int)food.x));

        } else {
                // remove tail from buffer
                map_buffer[snake[0].y] &= ~(1 << snake[0].x);

                // move each snake piece to the next ones position
                for (uint32_t i = 0; i < head; i++)
                        snake[i] = snake[i+1];

                // check for collision
                if (map_buffer[new_pos.y] & (1 << new_pos.x)) {
                        new_game();
                        return;
                }
                // add head to buffer
                snake[head] = new_pos;
                map_buffer[snake[head].y] |= (1 << snake[head].x);
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

        // Bindings for all buffers: vertex, index, uniforms
        gs_graphics_bind_desc_t binds = {
                .vertex_buffers = {.desc = &(gs_graphics_bind_vertex_buffer_desc_t){.buffer = vbo}},
                .index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){.buffer = ibo}}
        };

        gs_graphics_begin_render_pass(&cb, GS_GRAPHICS_RENDER_PASS_DEFAULT);
        gs_graphics_bind_pipeline(&cb, pip);
        gs_graphics_apply_bindings(&cb, &binds);
}

void update()
{
        // inputs
        if (gs_platform_key_pressed(GS_KEYCODE_ESC)) gs_engine_quit();
        else if (gs_platform_key_pressed(GS_KEYCODE_UP)   && prev_move.x != 0) move = (ivec){0,1};
        else if (gs_platform_key_pressed(GS_KEYCODE_DOWN) && prev_move.x != 0) move = (ivec){0,-1};
        else if (gs_platform_key_pressed(GS_KEYCODE_RIGHT)     && prev_move.y != 0) move = (ivec){1,0};
        else if (gs_platform_key_pressed(GS_KEYCODE_LEFT) && prev_move.y != 0) move = (ivec){-1,0};

        // move snake every 0.2 seconds
        const uint32_t time_now = gs_platform_elapsed_time() * 0.01f;
        const bool has_moved = time_now - prev_move_time >= 2;
        if (has_moved) {
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
                .uniforms = {.desc = uniforms, .size = sizeof(uniforms)}
        };

        /* Render */
        gs_graphics_set_viewport(&cb, 0, 0, (int32_t)fbs.x, (int32_t)fbs.y);
        if (has_moved)
                gs_graphics_apply_bindings(&cb, &binds);
        gs_graphics_draw(&cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 6});

        // Submit command buffer (syncs to GPU, MUST be done on main thread where you have your GPU context created)
        gs_graphics_submit_command_buffer(&cb);
}

gs_app_desc_t gs_main(int32_t argc, char** argv)
{
        return (gs_app_desc_t){
                .window_title = "Gunslinger Snake",
                .init = init,
                .update = update,
        };
}
