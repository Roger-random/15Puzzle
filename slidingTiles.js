// Array tracks the position of tiles.
// Index in array = position on the board.
// Value in the array = number on the tile.
// -1 represents blank.
var tilePosition = [1, 2, 3, 4, 
                    5, 6, 7, 8, 
                    9, 10, 11, 12, 
                    13, 14, 15, -1]

// How long to take to animate a tile
var tileSlideTime = 25;

// How much space to put between tiles
var tileSpace = 5;

// Gets the length of a tile's side.
// The item #tileBoard is told to lay itself out at 100% of available width.
// We also query for the visible window's inner width and height.
// The minimum of all of those is the maximum possible dimension to be entirely visible.
// We divided that by four to correspond to four rows and four columns.
// Further subtract by however much space we want to put between tiles.
var getTileDim = function() {
  var tileBoard = $("#tileBoard");

  var minDim = Math.min(window.innerWidth, window.innerHeight, tileBoard.innerWidth());

  return (minDim / 4) - tileSpace;
}

// Given a tile number, return its index in the array.
// Returns -1 if not found.
var indexOfTile = function(tileNum) {
  for (var i = 0; i < tilePosition.length; i++) {
    if (tilePosition[i] == tileNum) {
      return i;
    }
  }

  return -1;
}

// Given a tile number, starts an animation that moves the tile to its
// corresponding location on screen. Call this after the tile has been
// moved in the tilePosition[] array.
var updatePositionOfTile = function(tileNum) {
  var tileIndex = indexOfTile(tileNum);

  var tileRow = Math.floor(tileIndex / 4);
  var tileColumn = tileIndex % 4;
  var tileDim = getTileDim() + (tileSpace/2);

  $("#" + tileNum).animate({
    "left": tileColumn * tileDim,
    "top": tileRow * tileDim
  }, tileSlideTime);
}

// When the viewport is resized, update size of board accordingly.
var resizeTiles = function() {
  var boxes = $(".box");
  var boxLabels = $(".boxLabel");
  var tileDim = getTileDim();

  boxes.css({
    "width": tileDim,
    "height": tileDim,
    "border-radius": (tileDim * 0.15)
  });

  boxLabels.css({
    "font-size": tileDim / 2 + "px",
    "padding": tileDim / 4 + "px"
  });

  for (var i = 1; i <= 15; i++) {
    updatePositionOfTile(i);
  }

  $("#tileBoard").css("height", (tileDim + 2) * 4);
}

// Initial setup of game board. Take the HTML for ".box" under the
// tileTemplateHost" and clone it 15 times for the game tile. 
// For each tile, the tile text is updated and the ID set to the tile number.
var setupTiles = function() {
  var tileBoard = $("#tileBoard");
  var tileTemplate = $("#tileTemplateHost .box");

  for (var i = 1; i < 16; i++) {
    var newTile = tileTemplate.clone(false, false);
    newTile.attr("id", i);
    newTile.children(".boxLabel").text(i);
    tileBoard.append(newTile);
  }

  resizeTiles();
}

// Checks to see if the two given tile indices can be legally swapped.
// If so, return true.
var isValidSwap = function(indexClick, indexBlank) {
  if (indexClick + 1 == indexBlank &&
    indexClick % 4 < 3) {
    return true; // Move right
  } else if (indexClick - 1 == indexBlank &&
    indexClick % 4 > 0) {
    return true; // Move left
  } else if (indexBlank < 16 &&
    indexClick + 4 == indexBlank) {
    return true; // Move down
  } else if (indexBlank >= 0 &&
    indexClick - 4 == indexBlank) {
    return true; // Move up
  }
  return false;
}

// Click event handler for .box delegated on the #tileBoard. $(this) is the 
// tile that got clicked on. Retrieve its index in the tilePosition array and
// the index of the blank to determine if they are adjacent. If so, it is a 
// valid move, and perform the swap.
var tileClicked = function(event) {
  var tileClick = $(this).attr("id");

  var indexClick = indexOfTile(tileClick);
  var indexBlank = indexOfTile(-1);

  if (isValidSwap(indexClick, indexBlank)) {
    tilePosition[indexBlank] = tileClick;
    tilePosition[indexClick] = -1;
    updatePositionOfTile(tileClick);
  };
}

// Game board setup: generate tiles, size them correctly, and wait for the
// user to click.
$(document).ready(function() {
  setupTiles();
  $(window).resize(resizeTiles);
  $("#tileBoard").on("click", ".box", tileClicked);
})
