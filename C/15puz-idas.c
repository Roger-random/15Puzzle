#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUZZLE_COLUMN 4
#define PUZZLE_ROW 4
#define PUZZLE_SIZE (PUZZLE_COLUMN * PUZZLE_ROW)
#define PUZZLE_MIN 0
#define PUZZLE_MAX PUZZLE_SIZE-1

/////////////////////////////////////////////////////////////////////////////
//
//  Given a lookup table of int[PUZZLE_SIZE][PUZZLE_SIZE], fill in values for
//  Manhattan Distance lookup. The format of the lookup table is:
//
//  First index: The tile number. 
//  Second index: The position of the tile.
//  Value: Manhattan Distance for that tile.

int GenerateManhattanDistanceLookup(int lookupTable[][PUZZLE_SIZE])
{
  int currentTile = 0;
  int currentPosition = 0;
  int currentColumn = 0;
  int currentRow = 0;
  int desiredPosition = 0;
  int desiredColumn = 0;
  int desiredRow = 0;
  int distance = 0;

  printf("\nGenerating Manhattan Distance lookup table\n");

  for (currentTile = 0; currentTile < PUZZLE_SIZE; currentTile++)
  {
    if (currentTile == 0)
    {
      desiredPosition = PUZZLE_SIZE-1;
    }
    else
    {
      desiredPosition = currentTile-1;
    }

    desiredColumn = desiredPosition % PUZZLE_COLUMN;
    desiredRow = desiredPosition / PUZZLE_COLUMN;

    for (currentPosition = 0; currentPosition < PUZZLE_SIZE; currentPosition++)
    {
      if (currentTile == 0)
      {
        distance = 0;
      }
      else
      {
        currentColumn = currentPosition % PUZZLE_COLUMN;
        currentRow = currentPosition / PUZZLE_COLUMN;

        distance = abs(desiredColumn - currentColumn) + abs(desiredRow - currentRow);
      }

      lookupTable[currentTile][currentPosition] = distance;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
//
//  Print the lookup table to stdout
//

void PrintLookupTable(int lookupTable[][PUZZLE_SIZE])
{
  int printColumn = 0,
      printRow = 0,
      tableColumn = 0,
      tableRow = 0,
      tileIndex = 0,
      tableIndex = 0;

  printf("\nThe lookup table is as follows:\n");

  for (printRow = 0; printRow < PUZZLE_ROW; printRow++)
  {
    for (tableRow = 0; tableRow < PUZZLE_ROW; tableRow++)
    {
      for (printColumn = 0; printColumn < PUZZLE_COLUMN; printColumn++)
      {
        tileIndex = (printRow*PUZZLE_COLUMN) + printColumn;
        for (tableColumn = 0; tableColumn < PUZZLE_COLUMN; tableColumn++)
        {
          tableIndex = (tableRow*PUZZLE_COLUMN) + tableColumn;
          printf("%2d", lookupTable[tileIndex][tableIndex]);
        }
        printf("  ");
      }
      printf("\n");
    }
    printf("\n\n");
  }
}

/////////////////////////////////////////////////////////////////////////////
//
//  Calculate value of given puzzle, using the given lookup table
int CalculateValue(int* puzzle, int lookupTable[][PUZZLE_SIZE])
{
  int sum = 0;

  for(int i = 0; i < PUZZLE_SIZE; i++)
  {
    sum += lookupTable[puzzle[i]][i];
  }

  return sum;
}

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
//  Validation stage 2: Verify puzzle is solvable via inversion count rules.

int InversionCountOf(int* puzzle)
{
  int inversionCount = 0;
  int currentTile = 0;
  int compareTile = 0;
  int i = 0;
  int j = 0;

  for (i = 0; i < PUZZLE_SIZE; i++)
  {
    currentTile = puzzle[i];

    if (currentTile > 0)
    {
      for( j = i; j < PUZZLE_SIZE; j++)
      {
        compareTile = puzzle[j];
        if (compareTile != 0 && compareTile < currentTile)
        {
          inversionCount++;
        }
      }
    }
  }

  return inversionCount;
}

int PuzzleIsSolvable(int* puzzle)
{
  int inversionCountIsEven = ((InversionCountOf(puzzle) % 2) == 0);
  int solvable = 0;

  if (PUZZLE_COLUMN % 2 == 0)
  {
    int indexBlank = -1;

    for(int i = 0; i < PUZZLE_SIZE && indexBlank == -1; i++)
    {
      if (puzzle[i] == 0)
      {
        indexBlank = i;
      }
    }

    int blankEvenRowFromDesired = (((PUZZLE_ROW - (indexBlank/PUZZLE_COLUMN)) % 2) == 1);

    if (blankEvenRowFromDesired)
    {
      solvable = inversionCountIsEven;
    }
    else
    {
      solvable = !inversionCountIsEven;
    }
  }
  else
  {
    solvable = inversionCountIsEven;
  }

  if (!solvable)
  {
    printf("Unsolvable puzzle configuration detected.\n\n");
  }

  return solvable;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Calls all the puzzle state validations in turn
//

int Valid(int* puzzle)
{
  return TilesAreUnique(puzzle) &&
         PuzzleIsSolvable(puzzle);
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
  int nodes = 0;
  int puzzle[PUZZLE_SIZE];
  int mdLookup[PUZZLE_SIZE][PUZZLE_SIZE];

  GenerateManhattanDistanceLookup(mdLookup);
  PrintLookupTable(mdLookup);
 
  ReadPuzzleFromInput(puzzle);

  printf("Initial Manhattan Distance value of %d\n\n", CalculateValue(puzzle, mdLookup));
 }