// Array tracks the position of tiles.
// Index in array = position on the board.
// Value in the array = number on the tile.
// -1 represents blank.
var tilePosition = [1, 2, 3, 4, 
                    5, 6, 7, 8, 
                    9, 10, 11, 12, 
                    13, 14, 15, -1]

// Gets the length of a tile's side.
var getTileDim = function() {
  var tileBoard = $("#tileBoard");

  var minDim = Math.min(window.innerWidth, window.innerHeight, tileBoard.innerWidth());

  return (minDim / 4) - 5;
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

// Given a tile number, updates the position of that tile on screen
var updatePositionOfTile = function(tileNum) {
  var tileIndex = indexOfTile(tileNum);

  var tileRow = Math.floor(tileIndex / 4);
  var tileColumn = tileIndex % 4;
  var tileDim = getTileDim() + 2;

  $("#" + tileNum).animate({
    "left": tileColumn * tileDim,
    "top": tileRow * tileDim
  }, 25);
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

// Initial setup of game board. Take the HTML for "tileTemplateHost"
// and clone it 15 times for the game tile. For each tile, the tile
// text is updated and the ID set to the tile number.
var setupTiles = function() {
  var err = $("errorMessage");
  var tileBoard = $("#tileBoard");
  var tileTemplateHost = $("#tileTemplateHost");
  var tileTemplate;

  if (tileTemplateHost.length != 1) {
    err.text("Failed to find template host");
  }
  if (tileTemplateHost.children().length != 1) {
    err.text("Expected only one DIV for template");
  }
  tileTemplate = tileTemplateHost.children().first();

  for (var i = 1; i < 16; i++) {
    var newTile = tileTemplate.clone(false, false);
    newTile.attr("id", i);
    newTile.children(".boxLabel").text(i);
    tileBoard.append(newTile);
  }

  resizeTiles();
}

// Checks to see if the two given tile indices can be legally
// swapped. If so, return true.
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

// Click event handler maps click coordinates to a tile index
// and, if it is adjacent to a blank tile, perform the swap.
var tileClicked = function(event) {
  var tileBoard = $("#tileBoard");
  var boardX = event.pageX - tileBoard.offset().left;
  var boardY = event.pageY - tileBoard.offset().top;
  var tileDim = getTileDim();

  var column = Math.floor(boardX / tileDim);
  var row = Math.floor(boardY / tileDim);
  
  if (column > 3 || row > 3) {
    return;
  }
  
  var indexClick = (row * 4) + column;
  var tileClick = tilePosition[indexClick];

  var indexBlank = indexOfTile(-1);

  if (isValidSwap(indexClick, indexBlank)) {
    tilePosition[indexBlank] = tileClick;
    tilePosition[indexClick] = -1;
    updatePositionOfTile(tileClick);
  };
}

$(document).ready(function() {
  setupTiles();
  $(window).resize(resizeTiles);
  $("#tileBoard").on("click", tileClicked);
})
