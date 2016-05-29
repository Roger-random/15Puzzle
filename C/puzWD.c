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

// Inversion Distance is another heuristic employed here. It tracks the number
// of tiles that are out-of-place relative to tiles with a lower number.
// In the solved state, all tile numbers are increasing and the inversion
// distance is zero. IDTBL maps an inversion count to the minimum number of 
// moves required to put tiles back in order.
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

/////////////////////////////////////////////////////////////////////////////
//
// Pack the TABLE array (each element represented by 3 bits) into a 48-bit 
// representation.

u64 PackTable()
{
  u64 packedValue = 0;

  for (int i=0; i<BOARD_WIDTH; i++)
  {
    for (int j=0; j<BOARD_WIDTH; j++)
    {
      packedValue = (packedValue << 3) | TABLE[i][j];
    }
  }

  return packedValue;
}

/////////////////////////////////////////////////////////////////////////////
//
// Pack the given puzzle array (each element is the tile number at that space)
// into the 48-bit representation of their walking distance displacement.
// Can calculate either vertical or horizontal depending on flipAxis parameter.

u64 PackPuzzle(int* puzzle, int flipAxis)
{
  int i,j;
  int tileNum;

  int work[BOARD_WIDTH];  // Temporary work space

  u64 packedPuzzle = 0;

  for (i=0;i<BOARD_WIDTH;i++)
  {
    // Zero out work array
    for (j=0; j<BOARD_WIDTH; j++)
    {
      work[j] = 0;
    }

    // Look at each of the tiles in this row.
    for (j=0; j<BOARD_WIDTH; j++)
    {
      tileNum = flipAxis? CONV[puzzle[j*BOARD_WIDTH + i]] : puzzle[i*BOARD_WIDTH + j];

      if (tileNum == 0)
      {
        // Skip blank tile
        continue;
      }
      // Take the tile number, subtract one, then drop the least significant 
      // 2 bits (a.k.a. divide by four) gives us the desired row number for
      // that tile. Increment the desired row counter in the work array.
      work[(tileNum-1)>>2]++;
    }

    // Pack results of work array into lookup table encoding
    for (j=0; j<BOARD_WIDTH; j++)
    {
      packedPuzzle = (packedPuzzle<<3) | work[j];
    }
  }

  return packedPuzzle;
}

/////////////////////////////////////////////////////////////////////////////
//
// Initialize the link table entry at the given index to the starting value
// of WDTBL_SIZE

void InitializeLink(int linkIndex)
{
  for (int j=0;j<2;j++)
  {
    for (int k=0;k<4;k++)
    {
      WDLNK[linkIndex][j][k] = WDTBL_SIZE;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// 
//  Given a tile index and a space index, explore all the possible swaps
//  between those and record valid states into the lookup tables.

void SwapAndWrite(int tileIndex, int spaceIndex, int walkingDistance, int direction)
{
  u64 packedTable;
  int tableIndex;

  for (int group=0; group<4; group++)
  {
    // Check if there's even a tile of the appropriate class to swap with
    if (TABLE[tileIndex][group])
    {
      // Swap that tile with the space
      TABLE[tileIndex][group]--;
      TABLE[spaceIndex][group]++;

      packedTable = PackTable();

      // Examine the WDPTN table to see if this TABLE configuration is already
      // represented.
      for (tableIndex=0; tableIndex<WDEND; tableIndex++)
      {
        if (WDPTN[tableIndex] == packedTable)
        {
          break;
        }
      }

      // At this point, if i < WDEND, we found an existing entry in the table
      // matching our packed table value. If i==WDEND we hit the end without
      // find it and would need to add it to the table.
      if (tableIndex==WDEND)
      {
        WDPTN[WDEND] = packedTable; // Representing a TABLE configuration.
        WDTBL[WDEND] = walkingDistance; // The Walking Distance for this TABLE configuration.
        WDEND++;

        // When a new entry is added, we also initialize the corresponding
        // transition lookup entry for this TABLE configuration.
        InitializeLink(tableIndex);
      }

      // Fill in the transition lookup table entry for transition between the
      // currently examined node (WDTOP-1) and this new node. First fill in 
      // the given direction, then flip the direction with the ^ (XOR) operator
      // and fill in the other way.
      WDLNK[WDTOP - 1 ][direction  ][group] = (short)tableIndex;
      WDLNK[tableIndex][direction^1][group] = (short)WDTOP-1; 

      // Revert the swap so we can look at the next candidate.
      TABLE[tileIndex][group]++;
      TABLE[spaceIndex][group]--;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// Breadth-first walk through the Walking Distance space to generate all the
// lookup data used in the heuristic search later on.

void GenerateWalkingDistanceLookup()
{
  int i,j,k,space=0,piece;
  char walkingDistance;
  u64 packedTable;

  // The breadth-first search begins with the solved puzzle state and expands
  // from there to the full Walking Distance table configurations.

  // Create TABLE representing the solved state. 
  // First initialize to zero.
  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      TABLE[i][j] = 0;
    }
  }
  // Then fill in the diagonals representing tiles in their proper places.
  TABLE[0][0] = TABLE[1][1] = TABLE[2][2] = 4; // 4 tiles in correct row positions
  TABLE[3][3] = 3; // 3 tiles in correct row position for final row.

  // The solved state and its representation sits at the beginning of the
  // Walking Distance lookup table.
  WDPTN[0] = PackTable(); // Representing the solved TABLE configuration
  WDTBL[0] = 0;           // Solved state has walking distance of zero.

  // Initialize the transition lookup entry for the solved state.
  InitializeLink(0);

  // With the start state initialized to the solved configuration, it is
  // time to explore all the possible changes from that point.
  WDTOP=0; // Index of the node currently being expanded (the solved state)
  WDEND=1; // End of the open list where we append new nodes.
  while (WDTOP < WDEND)
  {
    // Retrieve the TABLE representation pattern and the Walking Distance
    // count for the node to be expanded.
    packedTable = WDPTN[WDTOP];
    walkingDistance = WDTBL[WDTOP] + 1;
    WDTOP++;

    // Unpack the representation back into the TABLE array so we can 
    // use it to explore valid states.
    for (i=3; i>=0; i--)
    {
      piece = 0; // This tracks the number of tile pieces on this row
      for (j=3; j>=0; j--)
      {
        TABLE[i][j] = (int)(packedTable&7);
        packedTable >>= 3;
        piece += TABLE[i][j];
      }
      if (piece==3)
      {
        // Only three tiles live on this row - so the blank space is here.
        space = i;
      }
    }

    // If the space is not on the bottom-most row, explore the states that
    // involve moving a tile up into the space.
    if ((piece = space + 1) < 4)
    {
      SwapAndWrite(piece, space, walkingDistance, 0);
    }

    // If the space is not on the top-most row, explore the states that
    // involve moving a tile down into the space.
    if ((piece = space - 1) >= 0)
    {
      SwapAndWrite(piece, space, walkingDistance, 1);
    }
  }

  ///////////////////////////////////////////////////////////////////////////

  // The inversion count table maps the count of inversion along an axis (i)
  // to the minimum number of moves that would be needed to fix the inversion
  // so the tiles are in order.
  for (i=0; i<IDTBL_SIZE; i++)
  {
    IDTBL[i] = (char)((i/3) + (i%3));
  }
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
//  Calculates the inversion count used for lookup into the inversion distance
//  heuristic. Can flip the axis of calculation via flipAxis parameter.

int InversionCount(int* puzzle, int flipAxis)
{
  int i,j;
  int inversionCount = 0;
  int currentTile;
  int compareTile;

  // For tile position index i, convp[i] will be the index of the position
  // of the flipped-axis array.
  int convp[PUZZLE_SIZE] = { 
    0, 4, 8,12, 
    1, 5, 9,13, 
    2, 6,10,14, 
    3, 7,11,15
  };

  for (i=0; i<PUZZLE_SIZE; i++)
  {
    currentTile = flipAxis ? CONV[puzzle[convp[i]]] : puzzle[i];
  
    if (currentTile) // Skip blank
    {
      for (j=i+1; j<PUZZLE_SIZE; j++)
      {
        compareTile = flipAxis ? CONV[puzzle[convp[j]]] : puzzle[j];

        if (compareTile && compareTile<currentTile) 
        {
          // Not blank && Inverted ordering
          inversionCount++;
        }
      }
    }
  }

  return inversionCount;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Given the full set of indices, calculate the lower bound value to use
//  as heuristic for the IDA* search.

int HeuristicValue(int idxV, int idxH, int invV, int invH)
{
  int wdV = WDTBL[idxV]; // Walking Distance for vertical tile movements.
  int wdH = WDTBL[idxH]; // Walking Distance for horizontal tile movements.
  int idV = IDTBL[invV]; // Inversion Distance for vertical tile movements.
  int idH = IDTBL[invH]; // Inversion Distance for horizontal tile movements.
  int lowbV = (wdV>idV)? wdV:idV; // Maximum of WD or ID is the lower bound.
  int lowbH = (wdH>idH)? wdH:idH; // Maximum of WD or ID is the lower bound.

  return lowbV + lowbH;
}

/////////////////////////////////////////////////////////////////////////////
//
//  The full set of Walking Distance calculations. This is required for the
//  initial state of the search. After startup, as we search the problem tree,
//  the WDLNK array can take care of updating the walking distance from step
//  to step with far less computation involved.
//
//  Debug note: If we suspect the WDLNK update mechanism is broken, we can 
//  fall back to performing this full calculation. But it is not recommended,
//  it is extremely computationally expensive to do this for each tree node.

int HeuristicLookupIndices(int* puzzle, int *pidx1, int *pidx2, int *pinv1, int *pinv2)
{
  int idx1, idx2, inv1, inv2;
  u64 packedPuzzle;

  // Calculate IDX1 - index into the Walking Distance table corresponding to
  // the minimum number of tile movements across rows. (Vertical tile moves.)
  packedPuzzle = PackPuzzle(puzzle, FALSE);

  // Look for index of the WDPTN entry corresponding to this pattern.
  for (idx1=0; WDPTN[idx1] != packedPuzzle; idx1++);

  // Calculate IDX2 - repeat the calculation made for IDX1, but this time
  // for movement across columns. (Horizontal tile moves.) 
  packedPuzzle = PackPuzzle(puzzle, TRUE);

  // Look for index of the WDPTN entry corresponding to this pattern.
  for (idx2=0; WDPTN[idx2] != packedPuzzle; idx2++);

  // Calculate inv1 - the number of tile inversions along the horizontal axis
  inv1 = InversionCount(puzzle, FALSE);

  // Calculate inv1 - the number of tile inversions along the vertical axis
  inv2 = InversionCount(puzzle, TRUE);

  // Copy values to outparams.
  *pidx1 = idx1;
  *pidx2 = idx2;
  *pinv1 = inv1;
  *pinv2 = inv2;

  return HeuristicValue(idx1, idx2, inv1, inv2);
}

/////////////////////////////////////////////////////////////////////////////
//
//  Examine a node and recursively call self to search deeper in the tree

int ExamineNode(int puzzle[PUZZLE_SIZE],
  int currentBlankIndex, int prevBlankIndex,
  int idx1o, int idx2o, int inv1o, int inv2o,
  int currentLength, int limitLength, unsigned long long *nodeCounter)
{
  int val = HeuristicValue(idx1o, idx2o, inv1o, inv2o);;

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

  if (val == 0)
  {
    // Problem solved!
    printf("\nTile movements to arrive in this state:\n");
    return currentLength;
  }
  else if (currentLength + val > limitLength)
  {
    // Exceeded limit
    return 0;
  }
  else
  {
    // Not terminating, so let's dig deeper
    int row=0, col=0, ret=0, childBlankIndex=0;
    int idx1, idx2, inv1, inv2;

    GetColumnRow(currentBlankIndex, &col, &row);

    for (int i = 0; i < 4; i++)
    {
      // Reset indices between iterations
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

          // Update inversion count for this move.
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

          // Look up the new Walking Distance index for this move.
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

          // Update inversion count for this move.
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

          // Look up the new Walking Distance index for this move.
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

          // Update inversion count for this move.
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

          // Look up the new Walking Distance index for this move.
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

          // Update inversion count for this move.
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

          // Look up the new Walking Distance index for this move.
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
        currentLength+1, limitLength, nodeCounter);

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
  int limit = HeuristicLookupIndices(puzzle, &idx1, &idx2, &inv1, &inv2);

  int blankIndex = GetBlankPosition(puzzle);

  if (limit > 0)
  {
    while(0 == (length = ExamineNode(puzzle,
                      blankIndex, -1 /* prevBlankIndex */, 
                      idx1, idx2, inv1, inv2,
                      0 /* Starting length */, limit, 
                      &nodesAtLimit)))
    {
      printf("Limit: %d completed with %llu nodes\n", limit, nodesAtLimit);      
      nodesTotal += nodesAtLimit;
      length = 0;
      nodesAtLimit = 0;
      limit += 2;
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

int PuzzleIsSolvable(int* puzzle)
{
  int inversionCountIsEven = ((InversionCount(puzzle,FALSE) % 2) == 0);
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
  int i, j;

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
      j = scanf("%d", &puzzle[i]);
      if(j==EOF)
      {
        break;
      }
      else if (j != 1)
      {
        printf("ERROR: Expected 1 input, got %d\n", j);
      }
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

  GenerateWalkingDistanceLookup();

  ReadPuzzleFromInput(puzzle);

  printf("Initial heuristic value of %d\n\n", HeuristicLookupIndices(puzzle, &idx1, &idx2, &inv1, &inv2));

  IDAStar(puzzle);
 }