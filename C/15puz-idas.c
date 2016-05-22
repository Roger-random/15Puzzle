#include <stdio.h>
#include <string.h>

#define PUZZLE_SIZE 16
#define PUZZLE_MIN 0
#define PUZZLE_MAX PUZZLE_SIZE-1

/////////////////////////////////////////////////////////////////////////////
//
//  Validation stage 1: Verify array has only integers 0 through 15, and only
//    one of each.

int TilesAreUnique(int* puzzle)
{
  int unique = 1;
  int i;

  int seenTile[PUZZLE_SIZE];

  memset(seenTile, 0, sizeof(int)*PUZZLE_SIZE);

  for(i = 0; i < PUZZLE_SIZE; i++)
  {
    if(puzzle[i] < 0 || puzzle[i] > 15) 
    {
      printf("Out of range tile %d detected.\n\n", puzzle[i]);
      unique = 0;
    }
    else if(seenTile[puzzle[i]] != 0)
    {
      printf("Duplicate tile %d detected.\n\n", puzzle[i]);
      unique = 0;
    }
    else
    {
      seenTile[puzzle[i]] = 1;
    }

  }

  return unique;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Calls all the puzzle state validations in turn
//

int Valid(int* puzzle)
{
  return TilesAreUnique(puzzle);
}

/////////////////////////////////////////////////////////////////////////////
//
//  Read in puzzle state from user input
//

void ReadPuzzleFromInput(int* puzzle)
{
  int i;

  printf("15-Puzzle solver\n\n");
  printf("Here are the tile position indices:\n\n");
  printf("  0  1  2  3\n");
  printf("  4  5  6  7\n");
  printf("  8  9 10 11\n");
  printf(" 12 13 14 15\n\n");
  printf("The solution state, with the blank in the lower-right (position index 15), is represented by the sequence\n\n");
  printf("1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 0\n\n");

  memset(puzzle, 0, sizeof(int)*PUZZLE_SIZE);

  do
  {
    printf("Enter the starting configuration for the puzzle:\n\n");

    for(i = 0; i < PUZZLE_SIZE; i++)
    {
      scanf("%d", &puzzle[i]);
    }

    printf("\n\nThe input received were as follows:\n\n");

    for(i = 0; i < PUZZLE_SIZE; i++) 
    {
      printf("%d ", puzzle[i]);
    }

    printf("\n\n");
  }
  while(!Valid(puzzle));
}

/////////////////////////////////////////////////////////////////////////////
//
//  Main
//

int main()
{
  int puzzle[PUZZLE_SIZE];

  ReadPuzzleFromInput(puzzle);
}