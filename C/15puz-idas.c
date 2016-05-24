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
//  Given a position, decode it into row and column.
//
void GetColumnRow(int position, int *column, int *row)
{
  *column = position % PUZZLE_COLUMN;
  *row = position / PUZZLE_COLUMN;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Simple linear search to find the location of the blank (zero) tile
//
int GetBlankPosition(int puzzle[PUZZLE_SIZE])
{
    int indexBlank = -1;

    for(int i = 0; i < PUZZLE_SIZE && indexBlank == -1; i++)
    {
      if (puzzle[i] == 0)
      {
        indexBlank = i;
      }
    }

    if (indexBlank==-1)
    {
      printf("ERROR: Blank tile not found\n");
    }

    return indexBlank;
}

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

    GetColumnRow(desiredPosition, &desiredColumn, &desiredRow);

    for (currentPosition = 0; currentPosition < PUZZLE_SIZE; currentPosition++)
    {
      if (currentTile == 0)
      {
        distance = 0;
      }
      else
      {
        GetColumnRow(currentPosition, &currentColumn, &currentRow);

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
//  Print the puzzle state to stdout
void PrintPuzzle(int puzzle[PUZZLE_SIZE])
{
    for(int i = 0; i < PUZZLE_ROW; i++) 
    {
      for (int j = 0; j < PUZZLE_COLUMN; j++)
      {
        printf("%3d", puzzle[(i*PUZZLE_COLUMN) + j]);
      }
      printf("\n");
    }

    printf("\n");
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
//  Examine a node and recursively call self to search deeper in the tree
int ExamineNode(int puzzle[PUZZLE_SIZE], int lookupTable[][PUZZLE_SIZE],
  int currentBlankIndex, int prevBlankIndex,
  int currentLength, int limitLength, int *nextLimit, unsigned long long *nodeCounter)
{
  int val = CalculateValue(puzzle, lookupTable);

  (*nodeCounter)++;

  if(puzzle[currentBlankIndex]!=0)
  {
    printf("ERROR: Blank index is not blank.\n");
  }

  // START Debug dump
  // printf("%d: blank at %d, prev %d. Length %d + Value %d against Limit %d\n",
  //   *nodeCounter, currentBlankIndex, prevBlankIndex, currentLength, val, limitLength);
  // PrintPuzzle(puzzle);
  // END Debug dump

  if (val == 0)
  {
    // Problem solved!
    return currentLength;
  }
  else if (currentLength + val > limitLength)
  {
    // Exceeded limit
    if (*nextLimit > currentLength+val)
    {
      // Nominate our length+heuristic value as next highest limit
      *nextLimit = currentLength+val;
    }
    return 0;
  }
  else
  {
    // Not terminating, so let's dig deeper
    int row=0, col=0, ret=0, childBlankIndex=0;

    GetColumnRow(currentBlankIndex, &col, &row);

    for (int i = 0; i < 4; i++)
    {
      if (i==0)
      {
        // Try moving the blank up
        if (row==0)
        {
          // Can't move up - it's already on the top row.
          continue;
        }
        else
        {
          childBlankIndex = currentBlankIndex - PUZZLE_COLUMN;
        }
      }
      else if (i == 1)
      {
        // Try moving the blank down
        if (row==PUZZLE_ROW-1)
        {
          // Can't move down - already on the bottom row.
          continue;
        }
        else
        {
          childBlankIndex = currentBlankIndex + PUZZLE_COLUMN;
        }
      }
      else if (i == 2)
      {
        // Try moving the blank left
        if (col == 0)
        {
          // Can't move left - already on leftmost column.
          continue;
        }
        else
        {
          childBlankIndex = currentBlankIndex - 1;
        }
      }
      else if (i == 3)
      {
        // Try moving the blank right
        if (col == PUZZLE_COLUMN-1)
        {
          // Can't move right - already on the rightmost column.
          continue;
        }
        else
        {
          childBlankIndex = currentBlankIndex + 1;
        }
      }

      if(childBlankIndex == prevBlankIndex)
      {
        // This retracts the move our parent just did, no point.
        continue;
      }

      // Perform the swap
      puzzle[currentBlankIndex] = puzzle[childBlankIndex];
      puzzle[childBlankIndex] = 0;

      // Recursive call to look at the next node
      ret = ExamineNode(puzzle, lookupTable, 
        childBlankIndex, currentBlankIndex,
        currentLength+1, limitLength, nextLimit, nodeCounter);

      // Revert the swap
      puzzle[childBlankIndex] = puzzle[currentBlankIndex];
      puzzle[currentBlankIndex] = 0;

      // Did the child find anything?
      if (ret != 0)
      {
        return ret;
      }
    }

    // None of the four directions proved fruitful
    return 0;
  }
}


/////////////////////////////////////////////////////////////////////////////
//
//  Execute the IDA* algorithm on the given puzzle state with given lookup table
//  for calculating heuristic.
//
int IDAStar(int puzzle[PUZZLE_SIZE], int lookupTable[][PUZZLE_SIZE])
{
  unsigned long long nodesTotal=0;
  unsigned long long nodesAtLimit = 0;
  int length = 0;
  int limit = CalculateValue(puzzle, lookupTable);
  int nextLimit = 999;

  int blankIndex = GetBlankPosition(puzzle);

  printf("Limit: %d ", limit);
  while(0 == (length = ExamineNode(puzzle, lookupTable,
                    blankIndex, -1 /* prevBlankIndex */, 
                    0 /* Starting length */, limit, 
                    &nextLimit, &nodesAtLimit)))
  {
    printf(" %llu nodes\n", nodesAtLimit);
    nodesTotal += nodesAtLimit;
    length = 0;
    nodesAtLimit = 0;
    limit = nextLimit;
    nextLimit = 999;
    printf("Limit: %d ", limit);
  }
  printf(" %llu nodes\n", nodesAtLimit);

  nodesTotal += nodesAtLimit;

  printf("Solution of length %d found after searching %llu nodes\n", length, nodesTotal);
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
    int indexBlank = GetBlankPosition(puzzle);
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

  // printf("15-Puzzle solver\n\n");
  // printf("Here are the tile position indices:\n\n");
  // printf("  0  1  2  3\n");
  // printf("  4  5  6  7\n");
  // printf("  8  9 10 11\n");
  // printf(" 12 13 14 15\n\n");
  // printf("The solution state, with the blank in the lower-right (position index 15), is represented by the sequence\n\n");
  // printf("1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 0\n\n");

  memset(puzzle, 0, sizeof(int)*PUZZLE_SIZE);

  do
  {
    printf("Enter the starting configuration for the puzzle:\n");

    for(i = 0; i < PUZZLE_SIZE; i++)
    {
      scanf("%d", &puzzle[i]);
    }

    printf("\nThe input received were as follows:\n\n");

    PrintPuzzle(puzzle);
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
  int mdLookup[PUZZLE_SIZE][PUZZLE_SIZE];

  GenerateManhattanDistanceLookup(mdLookup);
  // PrintLookupTable(mdLookup);
 
  ReadPuzzleFromInput(puzzle);

  printf("Initial Manhattan Distance value of %d\n\n", CalculateValue(puzzle, mdLookup));

  IDAStar(puzzle, mdLookup);
 }