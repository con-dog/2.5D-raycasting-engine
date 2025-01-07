#include "./level-io.h"

static void process_row(char *line, Jagged_Row *row);

// Main function to read and create the grid
extern Jagged_Grid *read_grid_csv_file(const char *filename)
{
  FILE *file = fopen(filename, "r");
  if (!file)
  {
    fprintf(stderr, "Could not open file %s\n", filename);
    return NULL;
  }

  // Create the grid structure
  Jagged_Grid *grid = malloc(sizeof(Jagged_Grid));
  if (!grid)
  {
    fclose(file);
    return NULL;
  }

  // First pass: count the number of rows
  grid->num_rows = 0;
  char c;
  while ((c = fgetc(file)) != EOF)
  {
    if (c == '\n')
      grid->num_rows++;
  }
  if (c != '\n' && grid->num_rows > 0)
    grid->num_rows++; // Handle last line without newline

  // Allocate rows array
  grid->rows = malloc(grid->num_rows * sizeof(Jagged_Row));
  if (!grid->rows)
  {
    free(grid);
    fclose(file);
    return NULL;
  }

  // Reset file position
  rewind(file);

  // Buffer for reading lines
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  int row_index = 0;

  // Read and process each line
  while ((read = getline(&line, &len, file)) != -1 && row_index < grid->num_rows)
  {
    process_row(line, &grid->rows[row_index]);
    row_index++;
  }

  free(line);
  fclose(file);
  return grid;
}

// Cleanup function
extern void free_jagged_grid(Jagged_Grid *grid)
{
  if (!grid)
    return;

  for (int i = 0; i < grid->num_rows; i++)
  {
    if (grid->rows[i].elements)
    {
      free(grid->rows[i].elements);
    }
  }

  free(grid->rows);
  free(grid);
}

// Debug printing function
extern void print_jagged_grid(const Jagged_Grid *grid)
{
  if (!grid)
  {
    printf("Grid is NULL\n");
    return;
  }

  printf("Grid has %d rows:\n", grid->num_rows);
  for (int i = 0; i < grid->num_rows; i++)
  {
    printf("Row %d:  length=%d, elements=",
           i, grid->rows[i].length);

    // Print elements if they exist
    if (grid->rows[i].elements)
    {
      for (int j = 0; j < grid->rows[i].length; j++)
      {
        printf("%c", grid->rows[i].elements[j]);
        if (j < grid->rows[i].length - 1)
        {
          printf(",");
        }
      }
    }
    printf("\n");
  }
}

static void process_row(char *line, Jagged_Row *row)
{
  // First pass: find last letter position and check if row has any content
  int last_letter = -1;
  int has_content = 0;
  int current_pos = 0;

  // Create a copy of the line for first pass
  char *line_copy = strdup(line);
  char *pos = line_copy;
  char *token;

  // Custom tokenization to handle empty cells
  while ((token = strsep(&pos, ",")) != NULL)
  {
    // Strip whitespace and newline
    while (*token && (isspace(*token) || *token == '\n'))
      token++;

    // Check if it's a non-empty cell
    if (*token)
    { // If there's any character after whitespace
      has_content = 1;
      last_letter = current_pos;
    }
    current_pos++;
  }
  free(line_copy);

  // If the row has content, start from position 0
  // If no content, create empty row
  if (!has_content)
  {
    row->length = 0;
    row->elements = NULL;
    return;
  }

  // Always start from position 0 if we have content
  row->length = last_letter + 1; // +1 because last_letter is 0-based

  // Allocate array
  row->elements = malloc(row->length * sizeof(Wall_Type));
  if (!row->elements)
  {
    row->length = 0;
    return;
  }

  // Second pass: fill the array
  line_copy = strdup(line);
  pos = line_copy;
  current_pos = 0;

  while ((token = strsep(&pos, ",")) != NULL && current_pos < row->length)
  {
    // Strip whitespace and newline
    while (*token && (isspace(*token) || *token == '\n'))
      token++;

    if (!*token)
    { // Empty cell
      row->elements[current_pos] = 'z';
    }
    else
    {
      row->elements[current_pos] = *token;
    }
    current_pos++;
  }

  free(line_copy);
}