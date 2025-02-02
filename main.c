#define ANGLE_TO_LUT_INDEX (1.0f / 0.3f)
#define TOTAL_LUT_ANGLES 1200

#include <time.h>

#include "main.h"

/* ******************
 * GLOBALS (START)
 ****************** */
SDL_Window *window;
SDL_Renderer *renderer;
World_Objects_Container *world_objects_container;
Jagged_Grid *floor_grid;
Jagged_Grid *wall_grid;
Player player;
SDL_Texture *rod;
const bool *keyboard_state;
static float cos_lut[TOTAL_LUT_ANGLES];
static float sin_lut[TOTAL_LUT_ANGLES];
/* ******************
 * GLOBALS (END)
 ****************** */

void init_trig_luts(void)
{
  for (int i = 0; i < TOTAL_LUT_ANGLES; i++)
  {
    float angle_rad = (i * 0.3f) * (M_PI / 180.0f);
    cos_lut[i] = cosf(angle_rad);
    sin_lut[i] = sinf(angle_rad);
  }
}

int get_angle_index(float angle_deg)
{
  int index = (int)(angle_deg * ANGLE_TO_LUT_INDEX) % TOTAL_LUT_ANGLES;
  return (index < 0) ? index + TOTAL_LUT_ANGLES : index;
}

static void player_init(void)
{
  player.rect.x = 72.0f;
  player.rect.y = 72.0f;
  player.rect.w = PLAYER_W;
  player.rect.h = PLAYER_H;
  player.angle = 0.0f;

  Radians radians = convert_deg_to_rads(player.angle);
  player.delta.x = cos(radians) * PLAYER_MOTION_DELTA_MULTIPLIER;
  player.delta.y = sin(radians) * PLAYER_MOTION_DELTA_MULTIPLIER;

  // ROD
  SDL_Surface *temp_surface = IMG_Load("./assets/sprites/rod/rod.png");
  if (!temp_surface)
  {
    fprintf(stderr, "Failed to load image\n");
  }

  SDL_Texture *temp_texture =
      SDL_CreateTextureFromSurface(renderer, temp_surface);
  SDL_DestroySurface(temp_surface);

  if (!temp_texture)
  {
    fprintf(stderr, "Failed to create texture\n");
  }

  if (!SDL_SetTextureScaleMode(temp_texture, SDL_SCALEMODE_NEAREST))
  {
    fprintf(stderr, "Failed to set texture scale mode: %s\n",
            SDL_GetError());
    SDL_DestroyTexture(temp_texture);
  }

  rod = temp_texture;
}

static void create_2D_line_from_start_point(Line_2D *out_line, Degrees degrees,
                                            float length)
{
  Radians radians = convert_deg_to_rads(degrees);
  out_line->end.x = out_line->start.x + length * cos(radians);
  out_line->end.y = out_line->start.y + length * sin(radians);
}

static void draw_player_direction(void)
{
  float length = 30.0f;
  Line_2D line = {
      .start.x = player.rect.x + PLAYER_W / 2,
      .start.y = player.rect.y + PLAYER_H / 2,
  };
  create_2D_line_from_start_point(&line, player.angle, length);
  SDL_RenderLine(renderer, line.start.x, line.start.y, line.end.x, line.end.y);
}

static void draw_player_rect(void) { SDL_RenderRect(renderer, &player.rect); }

static Scalar calculate_ray_perpendicular_distance(Line_2D *ray,
                                                   int lut_index)
{
  Scalar ray_length = sqrt(pow(ray->start.x - ray->end.x, 2) +
                           pow(ray->start.y - ray->end.y, 2));
  return ray_length * cos_lut[lut_index];
}

static void cast_rays_from_player(void)
{
  Degrees start_angle_deg = player.angle - PLAYER_FOV_DEG / 2;
  Degrees end_angle_deg = player.angle + PLAYER_FOV_DEG / 2;

  SDL_Texture *current_batch_texture = NULL;
  float batch_start_x = 0;
  float batch_width = 0;
  float batch_y = 0;
  float batch_height = 0;
  SDL_FRect wall_src_rect;

  for (Degrees curr_angle_deg = start_angle_deg;
       curr_angle_deg <= end_angle_deg; curr_angle_deg += PLAYER_FOV_DEG_INC)
  {
    /*
     * Ray Setup logic
     */

    clock_t grid_traversal_start = clock();

    Line_2D ray;
    Jagged_Row *curr_floor_grid_row = NULL;
    Jagged_Row *curr_wall_grid_row = NULL;

    // Radians curr_angle_rads = convert_deg_to_rads(curr_angle_deg);
    // Radians theta = convert_deg_to_rads(curr_angle_deg - player.angle);
    int curr_lut_index = get_angle_index(curr_angle_deg);
    int theta_lut_index = get_angle_index(curr_angle_deg - player.angle);

    ray.start.x = player.rect.x + (PLAYER_W / 2);
    ray.start.y = player.rect.y + (PLAYER_H / 2);

    IPoint_1D grid_x = floorf(ray.start.x / GRID_CELL_SIZE);
    Point_1D norm_x = ray.start.x / GRID_CELL_SIZE;
    // Vector_1D x_dir = cos(curr_angle_rads);
    Vector_1D x_dir = cos_lut[curr_lut_index];
    IVector_1D step_x = (x_dir >= 0) ? 1 : -1;
    Vector_1D delta_x = fabs(1.0f / x_dir);
    Vector_1D norm_x_dist_cell_edge = (x_dir < 0)
                                          ? (norm_x - grid_x) * delta_x
                                          : (grid_x + 1 - norm_x) * delta_x;

    IPoint_1D grid_y = floorf(ray.start.y / GRID_CELL_SIZE);
    Point_1D norm_y = ray.start.y / GRID_CELL_SIZE;
    Vector_1D y_dir = sin_lut[curr_lut_index];
    // Vector_1D y_dir = sin(curr_angle_rads);
    IVector_1D step_y = (y_dir >= 0) ? 1 : -1;
    Vector_1D delta_y = fabs(1.0f / y_dir);
    Vector_1D norm_y_dist_cell_edge = (y_dir < 0)
                                          ? (norm_y - grid_y) * delta_y
                                          : (grid_y + 1 - norm_y) * delta_y;

    Point_1D wall_x_intersection;
    Point_1D wall_y_intersection;
    Wall_Surface surface_hit;

    /*
     * Wall collision and step logic
     */
    bool is_wall_hit = false;
    while (!is_wall_hit)
    {
      if (norm_x_dist_cell_edge < norm_y_dist_cell_edge)
      {
        wall_x_intersection = (x_dir < 0) ? grid_x * GRID_CELL_SIZE
                                          : (grid_x + 1) * GRID_CELL_SIZE;
        wall_y_intersection =
            ray.start.y + (wall_x_intersection - ray.start.x) * y_dir / x_dir;
        norm_x_dist_cell_edge += delta_x;
        grid_x += step_x;
        surface_hit = WS_VERTICAL;
      }
      else
      {
        wall_y_intersection = (y_dir < 0) ? grid_y * GRID_CELL_SIZE
                                          : (grid_y + 1) * GRID_CELL_SIZE;
        wall_x_intersection =
            ray.start.x + (wall_y_intersection - ray.start.y) * x_dir / y_dir;
        norm_y_dist_cell_edge += delta_y;
        grid_y += step_y;
        surface_hit = WS_HORIZONTAL;
      }

      ray.end.x = wall_x_intersection;
      ray.end.y = wall_y_intersection;

      /*
       * Collision check for non EMPTY cell
       */
      if (grid_y < wall_grid->length)
      {
        curr_wall_grid_row = &wall_grid->rows[grid_y];
      }
      if (curr_wall_grid_row && grid_x < curr_wall_grid_row->length)
      {
        // Ocurr_grid_cell_value
        Object_Name current_grid_cell_value =
            curr_wall_grid_row->world_object_names[grid_x];

        if (strcmp(current_grid_cell_value, EMPTY_GRID_CELL_VALUE) != 0)
        {
          is_wall_hit = 1;
          break;
        }
      }
    }
    clock_t grid_traversal_end = clock();

    clock_t screen_calc_start = clock();
    /*
     * Screen calculations
     */
    Scalar perp_distance = calculate_ray_perpendicular_distance(&ray, theta_lut_index);
    Point_1D scr_x =
        ((curr_angle_deg - start_angle_deg) / PLAYER_FOV_DEG) * (WINDOW_W / 2) +
        WINDOW_W / 4;
    Scalar wall_strip_h = (GRID_CELL_SIZE * WINDOW_H) / perp_distance;
    Scalar scr_strip_w = (WINDOW_W / 2) / ((end_angle_deg - start_angle_deg) /
                                           PLAYER_FOV_DEG_INC);
    Scalar scr_offset_y = (WINDOW_H - wall_strip_h) / 2;
    clock_t screen_calc_end = clock();

    clock_t draw_floors_start = clock();
    Scalar floor_start_y = scr_offset_y + wall_strip_h;

    SDL_Texture *floor_batch_texture = NULL;
    float floor_batch_start_x = scr_x;
    float floor_batch_width = 0;
    float floor_batch_y = 0;
    float floor_batch_height = 1; // Floor strips are always 1 pixel high

    SDL_FRect floor_src_rect;

    for (int scr_y = floor_start_y; scr_y < WINDOW_H; scr_y++)
    {
      Scalar distance =
          ((WINDOW_H / 2.0f) / (scr_y - WINDOW_H / 2.0f)) * GRID_CELL_SIZE;
      Point_1D floor_world_x =
          (player.rect.x) + (x_dir / cos_lut[theta_lut_index]) * distance;
      Point_1D floor_world_y =
          (player.rect.y) + (y_dir / cos_lut[theta_lut_index]) * distance;

      IPoint_1D floor_grid_y = floorf(floor_world_y / GRID_CELL_SIZE);
      IPoint_1D floor_grid_x = floorf(floor_world_x / GRID_CELL_SIZE);

      Point_1D texture_x = (int)(floor_world_x) % TEXTURE_PIXEL_W;
      Point_1D texture_y = (int)(floor_world_y) % TEXTURE_PIXEL_H;

      SDL_Texture *current_strip_texture = NULL;
      floor_src_rect.x = texture_x;
      floor_src_rect.y = texture_y;
      floor_src_rect.w = 1;
      floor_src_rect.h = 1;

      // bounds check
      if (floor_grid_y < floor_grid->length &&
          floor_grid_x < floor_grid->rows[floor_grid_y].length)
      {
        curr_floor_grid_row = &floor_grid->rows[floor_grid_y];
        if (floor_grid_x < curr_floor_grid_row->length &&
            curr_floor_grid_row->world_object_names)
        {
          Object_Name curr_floor_grid_cell_value =
              curr_floor_grid_row->world_object_names[floor_grid_x];

          // Check if the cell value is valid
          if (curr_floor_grid_cell_value != NULL)
          {
            for (size_t i = 0; i < world_objects_container->length; i++)
            {
              // Check if the object and its name exist
              if (world_objects_container->data[i] == NULL ||
                  world_objects_container->data[i]->name == NULL)
              {
                continue;
              }

              if (strcmp(curr_floor_grid_cell_value,
                         world_objects_container->data[i]->name) == 0)
              {
                int current_frame_index = world_objects_container->data[i]
                                              ->animation_state.current_frame_index;

                // Check if textures array is valid and index is in bounds
                if (world_objects_container->data[i]->textures.data == NULL ||
                    current_frame_index < 0 ||
                    current_frame_index >= world_objects_container->data[i]->textures.length)
                {
                  continue;
                }

                current_strip_texture = world_objects_container->data[i]
                                            ->textures.data[current_frame_index];
                break;
              }
            }
          }
        }
      }

      // If this is the start of a new batch or texture changed
      if (floor_batch_texture == NULL ||
          floor_batch_texture != current_strip_texture ||
          floor_batch_y != scr_y)
      {
        // Render previous batch if it exists
        if (floor_batch_texture != NULL && floor_batch_width > 0)
        {
          SDL_FRect batch_floor_rect = {
              .x = floor_batch_start_x,
              .y = floor_batch_y,
              .w = floor_batch_width,
              .h = floor_batch_height};
          SDL_RenderTexture(renderer, floor_batch_texture, &floor_src_rect, &batch_floor_rect);
        }

        // Start new batch
        floor_batch_texture = current_strip_texture;
        floor_batch_start_x = scr_x;
        floor_batch_width = scr_strip_w;
        floor_batch_y = scr_y;
      }
      else
      {
        // Add to current batch
        floor_batch_width += scr_strip_w;
      }
    }

    // Handle final batch
    if (floor_batch_texture != NULL && floor_batch_width > 0)
    {
      SDL_FRect final_batch_rect = {
          .x = floor_batch_start_x,
          .y = floor_batch_y,
          .w = floor_batch_width,
          .h = floor_batch_height};
      SDL_RenderTexture(renderer, floor_batch_texture, &floor_src_rect, &final_batch_rect);
    }
    clock_t draw_floors_end = clock();

    clock_t draw_walls_start = clock();

    /*
     * Draw Walls
     */
    current_batch_texture = NULL;
    batch_start_x = scr_x;
    batch_width = 0;
    batch_y = scr_offset_y;
    batch_height = wall_strip_h;
    float current_texture_x = 0;

    Point_1D wall_x;
    if (surface_hit == WS_VERTICAL)
    {
      wall_x = wall_y_intersection;
    }
    else
    {
      wall_x = wall_x_intersection;
    }
    Point_1D wall_x_normalized = wall_x / GRID_CELL_SIZE;
    Point_1D wall_x_offset_normalized = wall_x_normalized - floorf(wall_x_normalized);
    Point_1D texture_x = roundf(wall_x_offset_normalized * TEXTURE_PIXEL_W);

    wall_src_rect.x = texture_x;
    wall_src_rect.y = 0;
    wall_src_rect.w = 1;
    wall_src_rect.h = TEXTURE_PIXEL_H;

    // Find the texture for current strip
    SDL_Texture *current_strip_texture = NULL;
    for (size_t i = 0; i < world_objects_container->length; i++)
    {
      if (curr_wall_grid_row->world_object_names &&
          grid_x < curr_wall_grid_row->length &&
          grid_x >= 0)
      {
        if (strcmp(curr_wall_grid_row->world_object_names[grid_x],
                   world_objects_container->data[i]->name) == 0)
        {
          int current_frame_index = world_objects_container->data[i]
                                        ->animation_state.current_frame_index;
          current_strip_texture = world_objects_container->data[i]
                                      ->textures.data[current_frame_index];
          break;
        }
      }
    }

    // If this is the start of a new batch or texture changed
    if (current_batch_texture == NULL || current_batch_texture != current_strip_texture)
    {
      // Render previous batch if it exists
      if (current_batch_texture != NULL && batch_width > 0)
      {
        SDL_FRect batch_wall_rect = {
            .x = batch_start_x,
            .y = batch_y,
            .w = batch_width,
            .h = batch_height};
        SDL_RenderTexture(renderer, current_batch_texture, &wall_src_rect, &batch_wall_rect);
      }

      // Start new batch
      current_batch_texture = current_strip_texture;
      batch_start_x = scr_x;
      batch_width = scr_strip_w;
      batch_y = scr_offset_y;
      batch_height = wall_strip_h;
    }
    else
    {
      // Add to current batch
      batch_width += scr_strip_w;
    }

    if (current_batch_texture != NULL && batch_width > 0)
    {
      SDL_FRect final_batch_rect = {
          .x = batch_start_x,
          .y = batch_y,
          .w = batch_width,
          .h = batch_height};
      SDL_RenderTexture(renderer, current_batch_texture, &wall_src_rect, &final_batch_rect);
    }

    clock_t draw_walls_end = clock();

    // printf("Traversal time: %f\n", ((double)(grid_traversal_end - grid_traversal_start)) / CLOCKS_PER_SEC);
    // printf("Screen calcs time: %f\n", ((double)(screen_calc_end - screen_calc_start)) / CLOCKS_PER_SEC);
    // // printf("Draw floors time: %f\n", ((double)(draw_floors_end - draw_floors_start)) / CLOCKS_PER_SEC);
    // printf("Draw walls time: %f\n", ((double)(draw_walls_end - draw_walls_start)) / CLOCKS_PER_SEC);
  }
  // exit(EXIT_SUCCESS);

  // Need to handle the last batch at the end of ray casting loop
  // Add this after your ray casting loop:
}

void draw_player(void)
{
  SDL_SetRenderDrawColor(renderer, 100, 0, 255, 255);
  draw_player_rect();
  draw_player_direction();
}

static void draw_jagged_grid(void)
{
  for (size_t i = 0; i < wall_grid->length; i++)
  {

    Jagged_Row *current_row = &wall_grid->rows[i];
    SDL_FRect black_rects[current_row->length];
    SDL_FRect white_rects[current_row->length];
    int black_count = 0;
    int white_count = 0;
    float offset = 0.1f;
    for (size_t j = 0; j < current_row->length; j++)
    {
      SDL_FRect rect;
      rect.h = GRID_CELL_SIZE * (1.0f - offset);
      rect.w = GRID_CELL_SIZE * (1.0f - offset);
      rect.x = (j * GRID_CELL_SIZE) + (GRID_CELL_SIZE * offset / 2);
      rect.y = (i * GRID_CELL_SIZE) + (GRID_CELL_SIZE * offset / 2);
      if (strcmp(wall_grid->rows[i].world_object_names[j],
                 EMPTY_GRID_CELL_VALUE) != 0)
      {
        black_rects[black_count++] = rect;
      }
      else
      {
        white_rects[white_count++] = rect;
      }
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRects(renderer, white_rects, white_count);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRects(renderer, black_rects, black_count);
  }
}

void rotate_player(Rotation_Type rotation, float delta_time)
{
  player.angle = player.angle + (rotation * PLAYER_ROTATION_STEP *
                                 PLAYER_ROTATION_SPEED * delta_time);
  player.angle = (player.angle < 0)     ? 360
                 : (player.angle > 360) ? 0
                                        : player.angle;
  Radians radians = convert_deg_to_rads(player.angle);
  player.delta.x = cos(radians) * PLAYER_MOTION_DELTA_MULTIPLIER;
  player.delta.y = sin(radians) * PLAYER_MOTION_DELTA_MULTIPLIER;
}

Grid_Hit_Box convert_world_position_to_grid_position(Point_2D *world_point,
                                                     float offset)
{
  Hit_Box player_hit_box_world = {// Top-left
                                  .tl.x = world_point->x - offset,
                                  .tl.y = world_point->y - offset,
                                  // Top-right
                                  .tr.x = world_point->x + PLAYER_W + offset,
                                  .tr.y = world_point->y - offset,
                                  // Bottom-left
                                  .bl.x = world_point->x - offset,
                                  .bl.y = world_point->y + PLAYER_H + offset,
                                  // Bottom-right
                                  .br.x = world_point->x + PLAYER_W + offset,
                                  .br.y = world_point->y + PLAYER_H + offset};

  Grid_Hit_Box player_hit_box_grid = {
      // Top-left
      .tl.x = floor(player_hit_box_world.tl.x / GRID_CELL_SIZE),
      .tl.y = floor(player_hit_box_world.tl.y / GRID_CELL_SIZE),
      // Top-right
      .tr.x = floor(player_hit_box_world.tr.x / GRID_CELL_SIZE),
      .tr.y = floor(player_hit_box_world.tr.y / GRID_CELL_SIZE),
      // Bottom-left
      .bl.x = floor(player_hit_box_world.bl.x / GRID_CELL_SIZE),
      .bl.y = floor(player_hit_box_world.bl.y / GRID_CELL_SIZE),
      // Bottom-right
      .br.x = floor(player_hit_box_world.br.x / GRID_CELL_SIZE),
      .br.y = floor(player_hit_box_world.br.y / GRID_CELL_SIZE),
  };

  return player_hit_box_grid;
}

void move_player(float direction, bool is_sprinting, float delta_time)
{
  Point_2D new_pos = {
      .x = player.rect.x +
           (direction * player.delta.x *
            (PLAYER_SPEED + (is_sprinting ? SPRINT_SPEED_INCREASE : 0)) *
            delta_time),
      .y = player.rect.y +
           (direction * player.delta.y *
            (PLAYER_SPEED + (is_sprinting ? SPRINT_SPEED_INCREASE : 0)) *
            delta_time),
  };

  Grid_Hit_Box player_hit_box_grid = convert_world_position_to_grid_position(
      &new_pos, PLAYER_INTERACTION_DISTANCE);

  /*
   * Process wall collisions
   */
  Jagged_Row *tl_wall_cell_row = &wall_grid->rows[player_hit_box_grid.tl.y];
  Jagged_Row *tr_wall_cell_row = &wall_grid->rows[player_hit_box_grid.tr.y];
  Jagged_Row *bl_wall_cell_row = &wall_grid->rows[player_hit_box_grid.bl.y];
  Jagged_Row *br_wall_cell_row = &wall_grid->rows[player_hit_box_grid.br.y];

  Jagged_Row *tl_floor_cell_row = &floor_grid->rows[player_hit_box_grid.tl.y];
  Jagged_Row *tr_floor_cell_row = &floor_grid->rows[player_hit_box_grid.tr.y];
  Jagged_Row *bl_floor_cell_row = &floor_grid->rows[player_hit_box_grid.bl.y];
  Jagged_Row *br_floor_cell_row = &floor_grid->rows[player_hit_box_grid.br.y];

  const Object_Name tl_wall_value =
      tl_wall_cell_row->world_object_names[player_hit_box_grid.tl.x];
  const Object_Name tr_wall_value =
      tr_wall_cell_row->world_object_names[player_hit_box_grid.tr.x];
  const Object_Name bl_wall_value =
      bl_wall_cell_row->world_object_names[player_hit_box_grid.bl.x];
  const Object_Name br_wall_value =
      br_wall_cell_row->world_object_names[player_hit_box_grid.br.x];

  const Object_Name tl_floor_value = tl_floor_cell_row->world_object_names[player_hit_box_grid.tl.x];
  const Object_Name tr_floor_value = tr_floor_cell_row->world_object_names[player_hit_box_grid.tr.x];
  const Object_Name bl_floor_value = bl_floor_cell_row->world_object_names[player_hit_box_grid.bl.x];
  const Object_Name br_floor_value = br_floor_cell_row->world_object_names[player_hit_box_grid.br.x];

  const Object_Name wall_obj_names[4] = {
      tl_wall_value,
      tr_wall_value,
      bl_wall_value,
      br_wall_value,
  };

  const Object_Name floor_obj_names[4] = {
      tl_floor_value,
      tr_floor_value,
      bl_floor_value,
      br_floor_value};

  bool can_move = true;
  for (size_t i = 0; i < 4; i++)
  {
    for (size_t j = 0; j < world_objects_container->length; j++)
    {
      if (strcmp(wall_obj_names[i], world_objects_container->data[j]->name) == 0)
      {
        switch (world_objects_container->data[j]->collision_mode)
        {
        case 0b010:
        {
          can_move = false;
          break;
        }
        case 0b011:
        {
          can_move = false;
          break;
        }
        case 0b111:
        {
          can_move = false;
        }
        }
        if (can_move == false)
        {
          break;
        }
      }
    }
  }

  for (size_t i = 0; i < 4; i++)
  {
    for (size_t j = 0; j < world_objects_container->length; j++)
    {
      if (strcmp(floor_obj_names[i], world_objects_container->data[j]->name) == 0)
      {
        switch (world_objects_container->data[j]->collision_mode)
        {
        case 0b001:
        {
          can_move = false;
          break;
        }
        case 0b011:
        {
          can_move = false;
          break;
        }
        case 0b111:
        {
          can_move = false;
        }
        }
        if (can_move == false)
        {
          break;
        }
      }
    }
  }

  if (can_move)
  {
    player.rect.x = new_pos.x;
    player.rect.y = new_pos.y;
  }
}

uint8_t get_kb_arrow_input_state(void)
{
  uint8_t state = 0b0;
  if (keyboard_state[SDL_SCANCODE_UP])
    state |= KEY_UP;
  if (keyboard_state[SDL_SCANCODE_DOWN])
    state |= KEY_DOWN;
  if (keyboard_state[SDL_SCANCODE_LEFT])
    state |= KEY_LEFT;
  if (keyboard_state[SDL_SCANCODE_RIGHT])
    state |= KEY_RIGHT;
  return state;
}

void handle_player_movement(float delta_time)
{
  uint8_t arrows_state = get_kb_arrow_input_state();
  bool is_sprinting = false;
  if (keyboard_state[SDL_SCANCODE_LSHIFT] ||
      keyboard_state[SDL_SCANCODE_RSHIFT])
  {
    is_sprinting = true;
  }
  if (arrows_state & KEY_LEFT)
  {
    rotate_player(ANTI_CLOCKWISE, delta_time);
  }
  if (arrows_state & KEY_RIGHT)
  {
    rotate_player(CLOCKWISE, delta_time);
  }
  if (arrows_state & KEY_UP)
  {
    move_player(FORWARDS, is_sprinting, delta_time);
  }
  if (arrows_state & KEY_DOWN)
  {
    move_player(BACKWARDS, is_sprinting, delta_time);
  }
}

void update_display(void)
{
  SDL_SetRenderDrawColor(renderer, 30, 0, 30, 255);
  SDL_RenderClear(renderer);
  cast_rays_from_player();

  SDL_FRect dest_rect = {
      .h = 300,
      .w = 400,
      .x = WINDOW_W / 2 - 200,
      .y = WINDOW_H - 300,
  };

  SDL_RenderTexture(renderer, rod, NULL, &dest_rect);

  SDL_RenderPresent(renderer);
}

void process_texture_animations(float delta_time)
{
  for (size_t i = 0; i < world_objects_container->length; i++)
  {
    // check if is animated first
    if (!world_objects_container->data[i]->animation_state.is_animated)
    {
      continue;
    }

    // increment elapsed time
    world_objects_container->data[i]->animation_state.frame_elapsed_time +=
        delta_time;

    if (world_objects_container->data[i]->animation_state.frame_elapsed_time >
        world_objects_container->data[i]->animation_state.frame_duration)
    {
      // reset elapsed time
      world_objects_container->data[i]->animation_state.frame_elapsed_time = 0;
      // check whether to increment or reset index
      world_objects_container->data[i]
          ->animation_state.current_frame_index =
          world_objects_container->data[i]
                      ->animation_state.current_frame_index ==
                  world_objects_container->data[i]
                      ->animation_state.max_frame_index
              ? 0
              : world_objects_container->data[i]
                        ->animation_state.current_frame_index +
                    1;
    }
  }
}

void run_game_loop(void)
{
  uint32_t frame_count = 0;
  uint32_t fps_last_time = SDL_GetTicks();
  uint32_t current_fps = 0;
  bool loopShouldStop = false;
  uint64_t previous_time = SDL_GetTicks();

  while (!loopShouldStop)
  {
    uint64_t current_time = SDL_GetTicks();
    float delta_time =
        (current_time - previous_time) / 1000.0f; // Convert to seconds
    previous_time = current_time;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_EVENT_QUIT)
      {
        loopShouldStop = true;
      }
    }

    clock_t anim_start = clock();
    process_texture_animations(delta_time);
    clock_t anim_end = clock();

    clock_t move_start = clock();
    handle_player_movement(delta_time);
    clock_t move_end = clock();

    clock_t display_start = clock();
    update_display();
    frame_count++;
    uint32_t current_time_fps = SDL_GetTicks();
    clock_t display_end = clock();

    if (current_time_fps - fps_last_time >= 1000)
    { // Every second
      current_fps = frame_count;
      frame_count = 0;
      fps_last_time = current_time_fps;
      printf("Current FPS: %u\n", current_fps);
    }

    // printf("Animation time: %f\n", ((double)(anim_end - anim_start)) / CLOCKS_PER_SEC);
    // printf("Movement time: %f\n", ((double)(move_end - move_start)) / CLOCKS_PER_SEC);
    // printf("Display time: %f\n", ((double)(display_end - display_start)) / CLOCKS_PER_SEC);
    // exit(EXIT_SUCCESS);
  }
}

int main()
{
  const char *title = "2.5D Raycasting Game Engine";
  setup_sdl(title, WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE, &window,
            &renderer);

  init_trig_luts();

  world_objects_container =
      setup_engine_textures(renderer, "./manifests/texture_manifest.json");
  floor_grid = read_grid_csv_file("./assets/levels/3/f.csv");
  wall_grid = read_grid_csv_file("./assets/levels/3/w.csv");

  player_init();
  keyboard_state = SDL_GetKeyboardState(NULL);
  run_game_loop();

  free_jagged_grid(wall_grid);
  free_jagged_grid(floor_grid);
  cleanup_world_objects(world_objects_container);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}