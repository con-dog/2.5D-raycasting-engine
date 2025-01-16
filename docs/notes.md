## Goal

## Issues
### Window size relationship to FPS
- My current FPS is tied to screen size, and at full Macbook Pro M1 screen size, I get ~50-60 FPS
    - Not good! thats just for a single z-level
    - Can optimize this by doing ray calculations in a smaller window, and then scaling up the results to the screen size

### Raycasting multiple z-levels
- I want multiple Z levels (like minecraft, 100's)
  - Without optimizations, this means casting multiple vertical rays for every single horizontal ray
  - I currently cast 160 rays for a single z-level
  - This is a shitload of rays (160 x N), will cause a massive FPS drop
  - Can optimize this by having a vertical FOV half that of the horizontal FOV
    - Also if a wall has been hit on this z-level, we can start casting rays above the wall only (and similarly below)
    - This will reduce the number of rays casted by a lot
    - This will also allow for more z-levels to be rendered at once
  - An even better optimization is to use run-length encoding to store the walls in a z-level
    - This will allow for a constant time lookup of walls in a z-level
    - This will also allow for more z-levels to be rendered at once
    - This will also allow for more complex levels to be rendered

```c
// 1 represents walls, 0 represents empty spac
struct vertical_run {
    int start_z;
    int length;
    int block_types[MAX_RUN_LENGTH];  // texture ids for each block
};

// Each position can have multiple runs
struct map_position {
    int num_runs;  // how many vertical runs at this position
    struct vertical_run runs[MAX_RUNS];  // array of runs
};

struct map_position wall_map[5][5];

// Example: Setting up a wall that goes from z=0 to z=2 at position (0,0)
wall_map[0][0].num_runs = 1;
wall_map[0][0].runs[0].start_z = 0;
wall_map[0][0].runs[0].length = 3;
wall_map[0][0].runs[0].block_types[0] = BRICK_TEXTURE;
wall_map[0][0].runs[0].block_types[1] = STONE_TEXTURE;
wall_map[0][0].runs[0].block_types[2] = BRICK_TEXTURE;

/*
 * The advantage here is when your ray hits position (0,0), you immediately know there's a solid wall from z=0 to z=2, and exactly what * textures to use, without having to cast individual rays for each z-level.
 */

// Furthermore, if you clamp dimensions to a multiple of 8 (Byte) can use bit packing to save space eg:
// For a 2x2 map where each position can have 8 z-levels (1 byte per vertical column)
struct vertical_run {
    uint8_t start_z;          // using uint8_t since we only have 8 z-levels
    uint8_t length;
    uint8_t block_types[8];   // max possible length is 8
};

struct map_position {
    uint8_t wall_bits;        // each bit represents wall/no-wall for one z-level
    uint8_t num_runs;         // how many separate runs of walls exist
    struct vertical_run runs[8];  // worst case: alternating wall/no-wall = 8 runs
};

struct map_position wall_map[2][2];

// Example: Let's set up position (0,0) with walls at z=0, z=1, and z=5
wall_map[0][0].wall_bits = 0b00100011;  // 1's at positions 0,1,5
wall_map[0][0].num_runs = 2;            // two separate runs of walls

// First run (z=0 to z=1)
wall_map[0][0].runs[0].start_z = 0;
wall_map[0][0].runs[0].length = 2;
wall_map[0][0].runs[0].block_types[0] = 1;  // brick texture
wall_map[0][0].runs[0].block_types[1] = 2;  // stone texture

// Second run (z=5)
wall_map[0][0].runs[1].start_z = 5;
wall_map[0][0].runs[1].length = 1;
wall_map[0][0].runs[1].block_types[0] = 3;  // wood texture
```

Actually an even better solution is to use bit packing to represent the wall map:

```c
chunk [16][16] = {
  0b1110000011100001, ...
}

// coord 1,1,1
{1, 1, 1} = 0b 1011 0001 0001 0001; // [0-3] = 0000, [4-7] = 0001, [8-11] = 0001, [12-15] = 0001
// [0-3] =
// 0 -> is wall / is empty
// 1 -> texture-type ... regular vs special (animated, interactable, etc)
// 2 -> solid wall / passable wall
// 3 -> solid floor / passable floor

// [4-7] = x pos in chunk
// [8-11] = y pos in chunk
// [12-15] = z pos in chunk

// Then we use a hashmap to store entries
struct Entry {
    uint16_t coords;    // [4 unused][4 bits x][4 bits y][4 bits z]
    uint16_t texture;   // Texture ID
}

// Not siure about hashmap implementation yet.

Then we have Chunk structures:
struct Chunk {
    uint16_t coords;    // [8 unused][8 bits x][8 bits y][8 bits z]
    struct Entry *entries; // Array of entries, only store entries with walls
    size_t length;    // Number of entries
}

// Then we have a world structure
struct World {
    struct Chunk *chunks;  // Array of chunks, only store chunks with entries
    size_t length;         // Number of chunks
}

```

eg chunk[0][0] = 0b1110000011100001
  - 1's represent walls in the z-plane, 0's represent empty space
  - 16 bits represent 16 z-levels