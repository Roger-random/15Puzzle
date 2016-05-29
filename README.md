
A collection of 15-puzzle exercises.
======

This repository is a collection of various programming exercises. The problem space is the 15-tile sliding tile puzzle, and I'm using it to learn about different things.

## Directory: ./

The root directory (initial exercise) is based on HTML and JavaScript. It is a UI building exercise on the fundamentals of HTML, JavaScript, and using the jQuery library. At the time of writing, it's just a set of 15 tiles that can moved around via a click or tap. There's no scrambling ability and no puzzle solving.

## Directory: /C/

This directory holds the exercises written in straight C. The code in this directory has no UI to speak of, they are about finding the optimal solution to solving the 15 sliding tile puzzle. By working in straight C, the goal is for maximum portability and bare-to-the-metal speed. 

#### 15puz-idas.c
This applies the iterative-deepening A\* (IDA\*) algorithm to solving the puzzle, using the Manhattan Distance heuristic.

**Advantage**: Minimal setup required, and runs extremely fast because everything stays within CPU L2 cache (possibly even L1, depending on CPU.) 

**Disadvantage**: The Manhattan Distance heuristic is not as effective at culling nodes as Walking Distance, so it results in a lot of duplicate nodes searched.

#### puzWD.c

This applies the same IDA\* algorithm to the puzzle, but using the Walking Distance heuristic (as described by Ken'ichiro Takahashi and adapted from his/her code) supplemented by the Inversion Distance heuristic. 

**Advantage**: Compared to Manhattan Distance, it culls the number of nodes examined by as much as 98%. 

**Disadvantage**: Compared to Manhattan Distance, the full Walking Disance heuristic is much slower in execution. Part of the problem is that the lookup tables are too big for L2 cache and must go into L3 or even overflow into main memory. 

**HOWEVER** - Takahashi also devised a system to update the heuristic value from one state to another without performing the full recalculation. This vastly reduces the average computation cost per node to only about 1.5 times for Manhattan Distance.

#### directionLookup.c

Calculating valid moves from a specific sliding tile position isn't very computationally intensive, but I was curious if doing it in the form of a lookup table would have a measurable performance impact. Empirical tests show almost 20% increase in time spent per search tree node, which was far more drastic of an impact than I had expected.

## Directory: /Test/

This directory holds test cases that can be piped in as input to the various different implementations of 15-puzzle solver programs.
