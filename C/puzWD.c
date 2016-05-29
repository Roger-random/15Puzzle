/////////////////////////////////////////////////////////////////////////////
//
//  Sliding tile puzzle solver using IDA* search with the Walking Distance
//  heuristic
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUZZLE_COLUMN 4
#define PUZZLE_ROW 4
#define PUZZLE_SIZE (PUZZLE_COLUMN * PUZZLE_ROW)
#define PUZZLE_MIN 0
#define PUZZLE_MAX PUZZLE_SIZE-1

#define FALSE 0
#define TRUE 1

/////////////////////////////////////////////////////////////////////////////
//
//  Adaptation of the Walking Distance algorithm by takaken (puz15wd.c)
//  
//  Goals of adaptation in priority order:
//  * Bring up to current C standard.
//  * More comments, and in English.
//  * Eliminate use of global variables.
//  
//  Originally I had the ambition to add flexibility for configuration other 
//  than 4x4 15-puzzle but that's going to be difficult. Problems are:
//  1. Current implementation depends on the puzzle being a square (with same
//     dimension on both sides) because it reuses the same lookup table for
//     both horizontal and vertical axis. The only way to do a non-square
//     puzzle (like 4x5 19-puzzle) would require two separate tables.
//  2. The lookup table size shoots up tremendously with size. When size is 4
//     the table can still fit in L3 cache. Table of 5 is going to dump into
//     main memory... would the memory latency kill us?
//
#define BOARD_WIDTH 4

#define WDTBL_SIZE 24964 // Don't understand where this value came from
#define IDTBL_SIZE 106

typedef unsigned long long u64; // MSVC 'unsigned __int64' now C99 'unsigned long long'

// Each element in TABLE is a count of the number of tiles mapping their 
// current row  against their desired row.
//
// Rephrase: TABLE[i][j] holds the count of tiles that are currently in row
//  i and need to be in row j for the puzzle's solved state. When i equals
//  j, that count is the number of tiles sitting in their correct final row.
//
// Example: TABLE representing the solved state, with all tiles in their
//  correct position (and therefore correct row) would look like this:
//             
//      j=0  1  2  3
//
// i = 0  4  0  0  0
// i = 1  0  4  0  0
// i = 2  0  0  4  0
// i = 3  0  0  0  3
int   TABLE[BOARD_WIDTH][BOARD_WIDTH];

// WDTOP and WDEND are used while generating the lookup table via breadth
// first search. WDTOP represents the end of the closed list, and WDEND 
// represents the end of the open list. Everything in between are nodes still
// still to be examined. Table generation ends when WDTOP catches up to WDEND
// (The values of which, when things work correctly, would be WDTBL_SIZE.)
int   WDTOP, WDEND;

// The value of a WDPTN element is a representation of a particular TABLE
// configuration. So given a TABLE, we can pack it into a pattern and perform
// a linear search of WDPTN until we find a match. The index value is used to 
// look at WDTBL to retrieve the Walking Distance corresponding to the TABLE.
u64   WDPTN[WDTBL_SIZE];
char  WDTBL[WDTBL_SIZE];

// WDLNK table stores transitions from one TABLE pattern to another. This
// allows the Walking Distance values to be updated without going through the
// steps of packing the TABLE and searching in WDPTN.
short WDLNK[WDTBL_SIZE][2][BOARD_WIDTH];

// Something to do with tile inversion distance...
char  IDTBL[IDTBL_SIZE];

// Walking Distance performs its calculations along one axis, then repeats
// the calculation along the other axis. The CONVersion table here is used
// to map tile positions across this axis flip, so that the same lookup
// tables can be used for both horizontal and vertical calculation.
int CONV[PUZZLE_SIZE] = {
  0,
  1, 5, 9,13,
  2, 6,10,14,
  3, 7,11,15,
  4, 8,12
};


void WriteTable(char count, int vect, int group)
{
  int i,j,k;
  u64 table;

  // GT: "Find the same pattern"
  table = 0;
  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      // TABLE is a 4x4 array like the game board.
      // TABLE[i][j] is something that can be represented in 3 bits (0-7) and
      // we're packing that into (16*3) = 48 bits, storing in a 64-bit value
      // 'table'
      table = (table <<3) | TABLE[i][j];
    }
  }

  // Scan values of the WDPTN table.
  // WDEND represents the index of the first unused entry in the table.
  for (i=0; i<WDEND; i++)
  {
    if (WDPTN[i] == table)
    {
      break;
    }
  }

  // At this point, if i < WDEND, we found an existing entry in the table
  // matching our TABLE[i][j] value. If i==WDEND we hit the end without
  // find it.

  // GT: "New pattern registration"
  if (i==WDEND)
  {
    WDPTN[WDEND] = table;
    WDTBL[WDEND] = count;
    WDEND++;
    for (j=0;j<2;j++)
    {
      for (k=0;k<4;k++)
      {
        WDLNK[i][j][k] = WDTBL_SIZE;
      }
    }
  }

  // GT: "To form a bidirection link"
  j = WDTOP - 1;
  WDLNK[j][vect  ][group] = (short)i;
  WDLNK[i][vect^1][group] = (short)j; // (vect) XOR (0x1) == flips the LSB on vect.
}

// GT: "Seek WalkingDistance in the breadth-first search"
void Simulation()
{
  int i,j,k,space=0,piece;
  char count;
  u64 table;

  // GT: "Make the initial surface"
  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      TABLE[i][j] = 0;
    }
  }
  TABLE[0][0] = TABLE[1][1] = TABLE[2][2] = 4;
  TABLE[3][3] = 3;
  table = 0;
  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      table = (table << 3) | TABLE[i][j];
    }
  }

  // GT: "Register the initial surface"
  WDPTN[0] = table;
  WDTBL[0] = 0;
  for (j=0; j<2; j++)
  {
    for (k=0; k<4; k++)
    {
      WDLNK[0][j][k] = WDTBL_SIZE;
    }
  }

  // GT: "Breadth-first search"
  WDTOP=0;
  WDEND=1;
  while (WDTOP < WDEND)
  {
    // GT: "TABLE[] call"
    table = WDPTN[WDTOP];
    count = WDTBL[WDTOP];
    WDTOP++;
    count++;

    // GT: "Reproduction" (Also: reappearance/return/revival)

    // Looks like taking the 'table' value and decoding it to the 4x4 TABLE
    //  Also sets 'space' to a value indicating [row?] of space.
    //  But what's going with 'piece'?
    for (i=3; i>=0; i--)
    {
      piece = 0;
      for (j=3; j>=0; j--)
      {
        TABLE[i][j] = (int)(table&7);
        table >>= 3;
        piece += TABLE[i][j];
      }
      if (piece==3)
      {
        space = i;
      }
    }

    // GT: "0: Move the piece to the top"
    if ((piece = space + 1) < 4)
    {
      for (i=0; i<4; i++)
      {
        if (TABLE[piece][i])
        {
          TABLE[piece][i]--;
          TABLE[space][i]++;
          WriteTable(count, 0, i);
          TABLE[piece][i]++;
          TABLE[space][i]--;
        }
      }
    }

    // GT: "1: Move the piece down"
    if ((piece = space - 1) >= 0)
    {
      for (i=0; i<4; i++)
      {
        if (TABLE[piece][i])
        {
          TABLE[piece][i]--;
          TABLE[space][i]++;
          WriteTable(count, 1, i);
          TABLE[piece][i]++;
          TABLE[space][i]--;
        }
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////
  for (i=0; i<IDTBL_SIZE; i++)
  {
    IDTBL[i] = (char)((i/3) + (i%3));
  }

  printf("Lookup table completed with WDTOP=%d, WDEND=%d\n", WDTOP, WDEND);
}

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
//  Calculate heuristic value of given puzzle

int CalculateIndicesFull(int* puzzle, int *pidx1, int *pidx2, int *pinv1, int *pinv2)
{
  int i, j, num1, num2, wd1, wd2, id1, id2;
  int idx1, idx2, inv1, inv2;
  int lowb1,lowb2;

  int work[PUZZLE_COLUMN];

  int convp[PUZZLE_SIZE] = { // Flips the board INDICES 90 degrees.
    0, 4, 8,12, 
    1, 5, 9,13, 
    2, 6,10,14, 
    3, 7,11,15
  };

  u64 table;

  // Calculate IDX1
  table = 0;
  for (i=0;i<PUZZLE_ROW;i++)
  {
    // Initialize work array
    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      work[j] = 0;
    }

    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      num1 = puzzle[i*PUZZLE_COLUMN + j];
      if (num1 == 0)
      {
        // Skip blank tile
        continue;
      }
      // Take the tile number, subtract one, then drop the least significant 
      // 2 bits (a.k.a. divide by four) gives us the desired row number for
      // that tile. Increment the desired row counter in the work array.
      work[(num1-1)>>2]++;
    }

    // Pack results of work array into table encoding
    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      table = (table<<3) | work[j];
    }
  }
  for (idx1=0; WDPTN[idx1] != table && idx1 < WDTBL_SIZE; idx1++);

  if(idx1 >= WDTBL_SIZE)
  {
    printf("WARNING: search for idx1 has ran off the rails. Debugger time!");
  }

  // Calculate IDX2
  table = 0;
  for (i=0; i<PUZZLE_ROW; i++)
  {
    // Initialize work array
    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      work[j] = 0;
    }

    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      num2 = CONV[puzzle[j*PUZZLE_ROW + i]];
      if (num2 == 0)
      {
        // Skip blank tile
        continue;
      }
      // Take the tile number, subtract one, then drop the least significant 
      // 2 bits (a.k.a. divide by four) gives us the desired row number for
      // that tile. Increment the desired row counter in the work array.
      work[(num2-1)>>2]++;
    }

    // Pack results of work array into table encoding
    for (j=0; j<PUZZLE_COLUMN; j++)
    {
      table = (table<<3) | work[j];
    }
  }
  for (idx2=0; WDPTN[idx2] != table && idx2 < WDTBL_SIZE; idx2++);

  if(idx2 >= WDTBL_SIZE)
  {
    printf("WARNING: search for idx2 has ran off the rails. Debugger time!");
  }

  // Calculate inv1
  inv1 = 0;
  for (i=0; i<PUZZLE_SIZE; i++)
  {
    num1 = puzzle[i];
    if (!num1)
    {
      // Skip blank
      continue;
    }

    for (j=i+1; j<PUZZLE_SIZE; j++)
    {
      num2 = puzzle[j];
      if (num2 && num2<num1)
      {
        inv1++;
      }
    }
  }

  // Calculate inv2
  inv2 = 0;
  for (i=0; i<PUZZLE_SIZE; i++)
  {
    num1 = CONV[puzzle[convp[i]]];
    if (!num1)
    {
      // Skip blank
      continue;
    }
    for (j=i+1; j<PUZZLE_SIZE; j++)
    {
      num2 = CONV[puzzle[convp[j]]];
      if (num2 && num2<num1)
      {
        inv2++;
      }
    }
  }

  // Put it all together for the walking distance
  wd1 = WDTBL[idx1];
  wd2 = WDTBL[idx2];
  id1 = IDTBL[inv1];
  id2 = IDTBL[inv2];
  lowb1 = (wd1>id1)? wd1:id1;
  lowb2 = (wd2>id2)? wd2:id2;
  // printf("(WD=%d/%d, ID=%d/%d) LowerBound=%d\n", wd1, wd2, id1, id2, lowb1+lowb2);

  *pidx1 = idx1;
  *pidx2 = idx2;
  *pinv1 = inv1;
  *pinv2 = inv2;

  return lowb1+lowb2;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Examine a node and recursively call self to search deeper in the tree

int ExamineNode(int puzzle[PUZZLE_SIZE],
  int currentBlankIndex, int prevBlankIndex,
  int idx1o, int idx2o, int inv1o, int inv2o,
  int currentLength, int limitLength, int *nextLimit, unsigned long long *nodeCounter)
{
  int val;
  int fallback = 0;

  (*nodeCounter)++;

  if (((*nodeCounter) % 1000000000) == 0)
  {
    // Status update every billion nodes
    printf("Limit: %d ongoing - with %llu nodes\n", limitLength, *nodeCounter);
  }  

  if(puzzle[currentBlankIndex]!=0)
  {
    printf("ERROR: Blank index is not blank.\n");
  }

  int idx1 = idx1o;
  int idx2 = idx2o;
  int inv1 = inv1o;
  int inv2 = inv2o;

  int wd1 = WDTBL[idx1];
  int wd2 = WDTBL[idx2];
  int id1 = IDTBL[inv1];
  int id2 = IDTBL[inv2];
  int lowb1 = (wd1 > id1) ? wd1 : id1;
  int lowb2 = (wd2 > id2) ? wd2 : id2;

  val = lowb1 + lowb2;

  if (val == 0)
  {
    // Problem solved!
    printf("\nTile movements to arrive in this state:\n");
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
      idx1 = idx1o;
      idx2 = idx2o;
      inv1 = inv1o;
      inv2 = inv2o;

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

          for (int j = childBlankIndex+1; j < currentBlankIndex; j++)
          {
            if (puzzle[j] > puzzle[childBlankIndex])
            {
              inv1++;
            }
            else
            {
              inv1--;
            }
          }

          idx1 = WDLNK[idx1][1][(puzzle[childBlankIndex]-1)>>2];
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

          // Scan the tiles between space and tile to update inversion count
          for (int j = currentBlankIndex+1; j < childBlankIndex; j++)
          {
            if (puzzle[j] > puzzle[childBlankIndex])
            {
              inv1--;
            }
            else
            {
              inv1++;
            }
          }

          // Use the link table to update the index corresponding to the moved tile.
          idx1 = WDLNK[idx1][0][(puzzle[childBlankIndex]-1)>>2];
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

          int convTile = CONV[puzzle[childBlankIndex]];

          for (int j = childBlankIndex + PUZZLE_COLUMN; j < PUZZLE_SIZE; j+= PUZZLE_COLUMN)
          {
            if (CONV[puzzle[j]] > convTile)
            {
              inv2++;
            }
            else
            {
              inv2--;
            }
          }

          for (int j = currentBlankIndex - PUZZLE_COLUMN; j >= 0; j -= PUZZLE_COLUMN)
          {
            if (CONV[puzzle[j]] > convTile)
            {
              inv2++;
            }
            else
            {
              inv2--;
            }
          }

          idx2 = WDLNK[idx2][1][(convTile-1)>>2];
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

          int convTile = CONV[puzzle[childBlankIndex]];

          for (int j = currentBlankIndex+PUZZLE_COLUMN; j < PUZZLE_SIZE; j += PUZZLE_COLUMN)
          {
            if (CONV[puzzle[j]] > convTile)
            {
              inv2--;
            }
            else
            {
              inv2++;
            }
          }

          for (int j = childBlankIndex- PUZZLE_COLUMN; j >= 0; j -= PUZZLE_COLUMN)
          {
            if (CONV[puzzle[j]] > convTile)
            {
              inv2--;
            }
            else
            {
              inv2++;
            }
          }

          idx2 = WDLNK[idx2][0][(convTile-1) >> 2];
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
      ret = ExamineNode(puzzle, 
        childBlankIndex, currentBlankIndex,
        idx1, idx2, inv1, inv2,
        currentLength+1, limitLength, nextLimit, nodeCounter);

      // Revert the swap
      puzzle[childBlankIndex] = puzzle[currentBlankIndex];
      puzzle[currentBlankIndex] = 0;

      // Did the child find anything?
      if (ret != 0)
      {
        printf(" %d", puzzle[childBlankIndex]);
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
int IDAStar(int puzzle[PUZZLE_SIZE])
{
  unsigned long long nodesTotal=0;
  unsigned long long nodesAtLimit = 0;
  int length = 0;
  int idx1, idx2, inv1, inv2;
  int limit = CalculateIndicesFull(puzzle, &idx1, &idx2, &inv1, &inv2);
  int nextLimit = 999;

  int blankIndex = GetBlankPosition(puzzle);

  if (limit > 0)
  {
    while(0 == (length = ExamineNode(puzzle,
                      blankIndex, -1 /* prevBlankIndex */, 
                      idx1, idx2, inv1, inv2,
                      0 /* Starting length */, limit, 
                      &nextLimit, &nodesAtLimit)))
    {
      printf("Limit: %d completed with %llu nodes\n", limit, nodesAtLimit);      
      nodesTotal += nodesAtLimit;
      length = 0;
      nodesAtLimit = 0;
      limit = nextLimit;
      nextLimit = 999;
    }
    printf("\n\nLimit: %d halted at %llu nodes\n", limit, nodesAtLimit);      

    nodesTotal += nodesAtLimit;
  }

  printf("\n\nSolution of length %d found after searching %llu nodes\n", length, nodesTotal);
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
  int idx1, idx2, inv1, inv2;

  Simulation();

  ReadPuzzleFromInput(puzzle);

  printf("Initial heuristic value of %d\n\n", CalculateIndicesFull(puzzle, &idx1, &idx2, &inv1, &inv2));

  IDAStar(puzzle);
 }