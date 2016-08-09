#include "ui_elements.h"
#include "TEMP_GeneralFunctions.cpp"
#include "SDL_keycode.h"
#include "SDL_render.h"
#include "SDL_mouse.h"
#include "new_tile_map_panel.h"
#include "tile_set_panel.h"
#include "tile_map_panel.h"

#define MIN_WIDTH 800
#define MIN_HEIGHT 670

#define PADDING 15

#define SCROLL_BAR_WIDTH 25
#define SCROLL_BAR_BIG_COLOUR 0xFFFFFFFF
#define SCROLL_BAR_SMALL_COLOUR 0xFFAAAAAA

static const enum ToolType
{
    PAINT_TOOL,
    FILL_TOOL,
    MOVE_TOOL
};

static SDL_Renderer *_renderer;
static UIPanel _panel;

static SDL_Rect _tileMapArea;
static TexturedRect _defaultTile;

static bool _selectionVisible;
static Vector2 _startSelectPos;
static TexturedRect _selectionBox;

static Button _createNewButton;
static Button _paintToolIcon;
static Button _fillToolIcon;
static Button _moveToolIcon;
static TexturedRect _selectedToolIcon;
static TexturedRect _hoveringToolIcon;

static bool _hoverToolIconVisible;
static Vector2 _lastFramePos;

static ToolType _currentTool;
static ToolType _previousTool;

static TileMap _tileMaps[5];
static uint32 _numTileMaps;
static uint32 _selectedTileMap;
static ScrollBar _verticalBar;
static ScrollBar _horizontalBar;

static SDL_Cursor *_arrowCursor;
static SDL_Cursor *_handCursor;

static TileMap createNewTileMap(int startX, int startY,
				int32 *outWidth, int32 *outHeight)
{
    TileMap newTileMap = {};
    newTileMap.offset.x = startX;
    newTileMap.offset.y = startY;
    
    NewTileMapPanelData* data = newTileMapPanelGetData();

    newTileMap.name = data->tileMapName;
    newTileMap.widthInTiles = data->widthInTiles;
    newTileMap.heightInTiles = data->heightInTiles;
    newTileMap.tileSize = data->tileSize;

    *outWidth = newTileMap.widthInTiles*newTileMap.tileSize;
    *outHeight = newTileMap.heightInTiles*newTileMap.tileSize;
    
    int memorySize = sizeof(Tile)*newTileMap.widthInTiles*newTileMap.heightInTiles;
    
    HANDLE heapHandle = GetProcessHeap();
    newTileMap.tiles = (Tile*) HeapAlloc(heapHandle, HEAP_ZERO_MEMORY, memorySize);

    if (newTileMap.tiles)
    {	
	Tile *row = newTileMap.tiles;
	for (int i = 0; i < newTileMap.heightInTiles; ++i)
	{
	    Tile *element = row;
	    for (int j = 0; j < newTileMap.widthInTiles; ++j)
	    {
		SDL_Rect rect;
		
		rect.h = newTileMap.tileSize;
		rect.w = newTileMap.tileSize;
		rect.x = newTileMap.offset.x + j*newTileMap.tileSize;
		rect.y = newTileMap.offset.y + i*newTileMap.tileSize;
		
		element->pos = rect;
		element->sheetPos.w = element->sheetPos.h = newTileMap.tileSize;
		
		++element;
	    }
	    row += newTileMap.widthInTiles;
	}
    }

    return newTileMap;
}

static void changeCurrentTool(ToolType *currentTool, ToolType newType,
			      Button *paintToolIcon, Button *fillToolIcon,
			      Button *moveToolIcon, TexturedRect *selectedToolIcon,
			      bool *selectionVisible)
{
    if (newType == PAINT_TOOL)
    {
	selectedToolIcon->pos.x = paintToolIcon->background.pos.x;
	selectedToolIcon->pos.y = paintToolIcon->background.pos.y;
	paintToolIcon->startedClick = false;
    }
    else if (newType == FILL_TOOL)
    {
	selectedToolIcon->pos.x = fillToolIcon->background.pos.x;
	selectedToolIcon->pos.y = fillToolIcon->background.pos.y;
					
	fillToolIcon->startedClick = false;
    }
    else if (newType == MOVE_TOOL)
    {
	selectedToolIcon->pos.x = moveToolIcon->background.pos.x;
	selectedToolIcon->pos.y = moveToolIcon->background.pos.y;
					
	moveToolIcon->startedClick = false;
	*selectionVisible = false;
    }
    
    *currentTool = newType;
}

static inline void clipSelectionBoxToBoundary(TexturedRect *selectionBox,
					      SDL_Rect bounds)
{
    if (selectionBox->pos.x < bounds.x)
    {
	selectionBox->pos.w -= bounds.x - selectionBox->pos.x;
	selectionBox->pos.x = bounds.x;
    }
    if (selectionBox->pos.x + selectionBox->pos.w > bounds.x + bounds.w)
    {
	selectionBox->pos.w = (bounds.x + bounds.w) - selectionBox->pos.x;
    }

    if (selectionBox->pos.y < bounds.y)
    {
	selectionBox->pos.h -= bounds.y - selectionBox->pos.y;
	selectionBox->pos.y = bounds.y;
    }
    if (selectionBox->pos.y + selectionBox->pos.h > bounds.y + bounds.h)
    {
	selectionBox->pos.h = (bounds.y + bounds.h) - selectionBox->pos.y;
    }
}

static inline Vector2 convertTilePosToScreenPos(uint32 tileSize, Vector2 tileMapOffset,
						Vector2 scrollOffset, Vector2 tilePos)
{
    Vector2 screenPos = tileMapOffset;
    
    screenPos.x +=
	(tilePos.x - scrollOffset.x/tileSize)*tileSize - scrollOffset.x%tileSize;
    screenPos.y +=
	(tilePos.y - scrollOffset.y/tileSize)*tileSize - scrollOffset.y%tileSize;
    
    return screenPos;
}

static Vector2 convertScreenPosToTilePos(int32 tileSize, Vector2 tileMapOffset,
					 Vector2 scrollOffset, Vector2 screenPos)
{
    Vector2 tilePos = scrollOffset/tileSize;
    
    int32 firstTileWidth = tileSize - scrollOffset.x%tileSize;
    if (firstTileWidth < tileSize)
    {
        tileMapOffset.x += firstTileWidth;

	if (screenPos.x >= tileMapOffset.x)
	    ++tilePos.x;
    }
    tilePos.x += (screenPos.x - tileMapOffset.x)/tileSize;
					        
    int32 firstTileHeight = tileSize - scrollOffset.y%tileSize;
    if (firstTileHeight < tileSize)
    {
	tileMapOffset.y += firstTileHeight;

	if (screenPos.y >= tileMapOffset.y)
	    ++tilePos.y;
    }
    tilePos.y += (screenPos.y - tileMapOffset.y)/tileSize;

    return tilePos;
}

static void paintSelectedTile(TileMap *tileMap, SDL_Rect tileMapArea,
			      Vector2 scrollOffset, Vector2 mousePos)
{
    int32 tileSize = tileMap->tileSize;
    Vector2 offset = {tileMapArea.x, tileMapArea.y};
    Vector2 tilePos = convertScreenPosToTilePos(tileSize, offset,
						scrollOffset, mousePos);
    
    Tile *clicked = tileMap->tiles + tilePos.x + tilePos.y*tileMap->widthInTiles;

    if (tileSetPanelGetSelectedTile().sheetPos.w != 0 &&
	tileSetPanelGetSelectedTile().sheetPos.h != 0)
    {
	clicked->sheetPos = tileSetPanelGetSelectedTile().sheetPos;
    }
}

static void moveSelectionInScrolledMap(TexturedRect *selectionBox, SDL_Rect tileMapArea,
				       Vector2 scrollOffset, Vector2 point, int32 tileSize)
{
    Vector2 offset = {tileMapArea.x, tileMapArea.y};
    Vector2 selectedTile = point - offset;
    
    int32 firstTileWidth = tileSize - scrollOffset.x%tileSize;
    if (selectedTile.x > firstTileWidth)
    {
	selectedTile.x -= firstTileWidth;

	selectionBox->pos.x = (selectedTile.x/tileSize)*tileSize + offset.x + firstTileWidth;
	selectionBox->pos.w = MIN(tileSize, tileMapArea.x + tileMapArea.w - selectionBox->pos.x);
    }
    else
    {
	selectionBox->pos.w = firstTileWidth;
	selectionBox->pos.x = offset.x;
    }

    int32 firstTileHeight = tileSize - scrollOffset.y%tileSize;
    if (selectedTile.y > firstTileHeight)
    {
	selectedTile.y -= firstTileHeight;

	selectionBox->pos.y = (selectedTile.y/tileSize)*tileSize + offset.y + firstTileHeight;
	selectionBox->pos.h = MIN(tileSize, tileMapArea.y + tileMapArea.h - selectionBox->pos.y);
    }
    else
    {
	selectionBox->pos.h = firstTileHeight;
	selectionBox->pos.y = offset.y;
    }
}

static void scrollTileMap(ScrollBar *scrollBar, bool isVertical, Vector2 mousePos,
			  TileMap *currentMap)
{
    TexturedRect *smallRect = &scrollBar->scrollingRect;
    TexturedRect *backgroundRect = &scrollBar->backgroundRect;
    int32 tileSize = currentMap->tileSize;
    
    int32 mouseCoord = 0;
    int32 *drawOffsetCoord = 0;
    int32 tileMapDim = 0;
    int32 tileMapVisibleDim = 0;
    
    int32 *smallRectCoord = 0;
    int32 smallRectDim = 0;
    int32 backgroundRectCoord = 0;
    int32 backgroundRectDim = 0;
    
    if (isVertical)
    {
	mouseCoord = mousePos.y;
	drawOffsetCoord = &currentMap->drawOffset.y;
	tileMapDim = currentMap->heightInTiles;
	tileMapVisibleDim = currentMap->visibleArea.h;
	
	smallRectCoord = &smallRect->pos.y;
	smallRectDim = smallRect->pos.h;
	backgroundRectCoord = backgroundRect->pos.y;
	backgroundRectDim = backgroundRect->pos.h;
    }
    else
    {
	mouseCoord = mousePos.x;
        drawOffsetCoord = &currentMap->drawOffset.x;
	tileMapDim = currentMap->widthInTiles;
	tileMapVisibleDim = currentMap->visibleArea.w;
	
	smallRectCoord = &smallRect->pos.x;
	smallRectDim = smallRect->pos.w;
	backgroundRectCoord = backgroundRect->pos.x;
	backgroundRectDim = backgroundRect->pos.w;
    }
    
    if (scrollBar->scrolling)
    {
	*smallRectCoord = mouseCoord - scrollBar->scrollClickDelta;

	if (*smallRectCoord < backgroundRectCoord)
	{
	    *smallRectCoord = backgroundRectCoord;
	}
	else if (*smallRectCoord + smallRectDim >
		 backgroundRectCoord + backgroundRectDim)
	{
	    *smallRectCoord = backgroundRectCoord + backgroundRectDim - smallRectDim;
	}

	real32 percentMoved = (real32)(*smallRectCoord - backgroundRectCoord)/(real32)(backgroundRectDim - smallRectDim);
	*drawOffsetCoord = (int32)((tileMapDim*tileSize - tileMapVisibleDim)*percentMoved);
    }
}

void tileMapPanelCreateNew(SDL_Renderer *renderer, uint32 x, uint32 y,
			   uint32 width, uint32 height)
{
    _renderer = renderer;

    width = MAX(MIN_WIDTH, width);
    height = MAX(MIN_HEIGHT, height);

    _defaultTile = loadImage(_renderer, "default_tile.png");
    
    {
	uint32 colour = 0xFFEE2288;
	_panel = ui_createPanel(x, y, width, height, colour);
    }

    _createNewButton = ui_createTextButton("Create New Tile Map", 0xFFFFFFFF, 0, 0,
					   0xFFFF0000);
    {
	int x = _panel.panel.pos.x + _panel.getWidth()/2 - _createNewButton.getWidth()/2;
	int y = _panel.panel.pos.y + _panel.getHeight()/2 - _createNewButton.getHeight()/2;
	_createNewButton.setPosition({x, y});
    }
	    
    _previousTool = PAINT_TOOL;
    _currentTool = PAINT_TOOL;
	    
    _paintToolIcon = ui_createImageButton("paint-brush-icon-32.png");
    _fillToolIcon = ui_createImageButton("paint-can-icon-32.png");
    _moveToolIcon = ui_createImageButton("cursor-icon-32.png");
    
    _selectedToolIcon = loadImage(renderer, "selected-icon-32.png");
    _hoveringToolIcon = loadImage(renderer, "hovering-icon-32.png");
	    
    {
	int x = _panel.panel.pos.x + _panel.getWidth() - _paintToolIcon.getWidth() - PADDING;
	int y = _panel.panel.pos.y + PADDING;
	_paintToolIcon.setPosition({x, y});
	_selectedToolIcon.pos.x = x;
	_selectedToolIcon.pos.y = y;
		    
	y += _paintToolIcon.getHeight() + 5;
	_fillToolIcon.setPosition({x, y});

	y += _fillToolIcon.getHeight() + 5;
	_moveToolIcon.setPosition({x, y});

	_tileMapArea.x = _panel.panel.pos.x + PADDING;
	_tileMapArea.y = _panel.panel.pos.y + PADDING;
	_tileMapArea.w = x - _tileMapArea.x - PADDING - SCROLL_BAR_WIDTH;
	_tileMapArea.h = _panel.panel.pos.h - PADDING - SCROLL_BAR_WIDTH;
    }
    ui_addToPanel(&_paintToolIcon, &_panel);
    ui_addToPanel(&_fillToolIcon, &_panel);
    ui_addToPanel(&_moveToolIcon, &_panel);
    ui_addToPanel(&_selectedToolIcon, &_panel);

    _arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    _handCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
}

void tileMapPanelDraw()
{
    if (_panel.visible)
    {
	ui_draw(&_panel);

	TileMap *currentMap = &_tileMaps[_selectedTileMap];
	int32 tileSize = currentMap->tileSize;
    
	if (_hoverToolIconVisible)
	{
	    ui_draw(&_hoveringToolIcon);
	}
    
	if (!currentMap->tiles)
	{
	    ui_draw(&_createNewButton);
	}
	else
	{
	    if (currentMap->tiles && currentMap->widthInTiles != 0 &&
		currentMap->heightInTiles != 0)
	    {
		int32 startTileX = (currentMap->drawOffset.x)/tileSize;
		int32 startTileY = (currentMap->drawOffset.y)/tileSize;

		//TODO(denis): test this out and replace the current for loop bounds
		int32 endTileX = currentMap->visibleArea.w/tileSize + currentMap->drawOffset.x/tileSize;
		int32 endTileY = currentMap->visibleArea.h/tileSize + currentMap->drawOffset.y/tileSize;
			    
		for (int32 i = startTileY; i < currentMap->heightInTiles; ++i)
		{
		    for (int32 j = startTileX; j < currentMap->widthInTiles; ++j)
		    {
			Tile *element = currentMap->tiles + i*currentMap->widthInTiles + j;

			SDL_Texture *tileSet = tileSetPanelGetCurrentTileSet();

			bool usingDefault = false;
			if (!tileSet)
			{
			    tileSet = _defaultTile.image;
			    usingDefault = true;
			}
		    
			SDL_Rect drawRectSheet = element->sheetPos;
			SDL_Rect drawRectScreen = element->pos;

			if (drawRectScreen.x - currentMap->drawOffset.x
			    < currentMap->visibleArea.x)
			{
			    drawRectScreen.x = currentMap->visibleArea.x;
			    drawRectScreen.w -= currentMap->drawOffset.x % tileSize;
			    drawRectSheet.w = drawRectScreen.w;

			    if (usingDefault)
			    {
				real32 ratio = (real32)_defaultTile.pos.w/(real32)tileSize;
				drawRectSheet.x += (int32)((currentMap->drawOffset.x % tileSize)*ratio);
			    }
			    else
				drawRectSheet.x += currentMap->drawOffset.x % tileSize;
			}
			else
			{
			    drawRectScreen.x -= currentMap->drawOffset.x;
					
			    if (drawRectScreen.x+drawRectScreen.w >
				currentMap->visibleArea.x+currentMap->visibleArea.w)
			    {
				drawRectSheet.w = currentMap->drawOffset.x % tileSize;
				drawRectScreen.w = drawRectSheet.w;
			    }
			}

			if (drawRectScreen.y - currentMap->drawOffset.y <
			    currentMap->visibleArea.y)
			{
			    drawRectScreen.y = currentMap->visibleArea.y;
			    drawRectScreen.h -= currentMap->drawOffset.y % tileSize;
			    drawRectSheet.h = drawRectScreen.h;

			    if (usingDefault)
			    {
				real32 ratio = (real32)_defaultTile.pos.h/(real32)tileSize;
				drawRectSheet.y += (int32)((currentMap->drawOffset.y % tileSize)*ratio);
			    }
			    else
				drawRectSheet.y += currentMap->drawOffset.y % tileSize;
			}
			else
			{
			    drawRectScreen.y -= currentMap->drawOffset.y;

			    if (drawRectScreen.y + drawRectScreen.h >
				currentMap->visibleArea.y + currentMap->visibleArea.h)
			    {
				drawRectSheet.h = currentMap->drawOffset.y % tileSize;
				drawRectScreen.h = drawRectSheet.h;
			    }
			}
		    
			//TODO(denis): if new bounds works, remove this if statement
			if (drawRectScreen.x+drawRectScreen.w >= currentMap->visibleArea.x
			    && drawRectScreen.x <= currentMap->visibleArea.x + currentMap->visibleArea.w &&
			    drawRectScreen.y + drawRectScreen.h >= currentMap->visibleArea.y &&
			    drawRectScreen.y <= currentMap->visibleArea.y + currentMap->visibleArea.h)
			{
			    SDL_RenderCopy(_renderer, tileSet, &drawRectSheet, &drawRectScreen);
			}
		    }
		}

		//TODO(denis): put more conditions on the drawing of the scroll bars
		// only draw them on tile maps that need them
		ui_draw(&_verticalBar);
		ui_draw(&_horizontalBar);
	    }	
	}

	if (_selectionVisible)
	    SDL_RenderCopy(_renderer, _selectionBox.image, NULL, &_selectionBox.pos);
    }
}

void tileMapPanelOnMouseMove(Vector2 mousePos, int32 leftClickFlag)
{
    SDL_SetCursor(_arrowCursor);

    TileMap *currentMap = &_tileMaps[_selectedTileMap];
    int32 tileSize = currentMap->tileSize;
    
    if (_horizontalBar.scrolling)
    {
	scrollTileMap(&_horizontalBar, false, mousePos, currentMap);
    }
    else if (_verticalBar.scrolling)
    {
	scrollTileMap(&_verticalBar, true, mousePos, currentMap);
    }
    else if (_currentTool == PAINT_TOOL && _panel.visible && currentMap->tiles)
    {
	if (_selectionBox.pos.w != 0 && _selectionBox.pos.h != 0)
	{
	    if (pointInRect(mousePos, currentMap->visibleArea))
	    {
		_selectionVisible = true;
				        
		moveSelectionInScrolledMap(&_selectionBox, currentMap->visibleArea, currentMap->drawOffset, mousePos, tileSize);
	    }
	    else
	    {
		_selectionVisible = false;
	    }
	}
				
	if (leftClickFlag)
	{
	    if (pointInRect(mousePos, currentMap->visibleArea))
	    {
		paintSelectedTile(currentMap, currentMap->visibleArea,
				  currentMap->drawOffset, mousePos);
	    }
	}
    }
    else if (_currentTool == FILL_TOOL && _panel.visible && currentMap->tiles)
    {
	if (leftClickFlag && _startSelectPos != Vector2{0,0})
	{
	    _selectionVisible = true;

	    Vector2 offset = {_tileMapArea.x, _tileMapArea.y};

	    Vector2 tilePos =
		convertScreenPosToTilePos(tileSize, offset, currentMap->drawOffset, mousePos);
	    
	    Vector2 startedTilePos =
		convertScreenPosToTilePos(tileSize, offset, currentMap->drawOffset, _startSelectPos);

	    //TODO(denis): if the user is holding the mouse
	    // near an edge that can be scrolled more
	    // do it

	    //TODO(denis): do I still need these bounds checking
	    // if statements?
	    if (tilePos.x < 0)
		tilePos.x = 0;
	    else if (tilePos.x >= currentMap->widthInTiles)
		tilePos.x = currentMap->widthInTiles-1;

	    if (tilePos.y < 0)
		tilePos.y = 0;
	    else if (tilePos.y >= currentMap->heightInTiles)
		tilePos.y = currentMap->heightInTiles-1;
	    
	    
	    if (startedTilePos.x < tilePos.x)
	    {
		_selectionBox.pos.x =
		    convertTilePosToScreenPos(tileSize, offset, currentMap->drawOffset, startedTilePos).x;
	    }	
	    else
	    {
		_selectionBox.pos.x =
		    convertTilePosToScreenPos(tileSize, offset, currentMap->drawOffset, tilePos).x;
	    }

	    _selectionBox.pos.w = tileSize + absValue(tilePos.x - startedTilePos.x)*tileSize;
	    
	    if (startedTilePos.y < tilePos.y)
	    {
		_selectionBox.pos.y =
		    convertTilePosToScreenPos(tileSize, offset, currentMap->drawOffset, startedTilePos).y;
	    }
	    else
	    {
		_selectionBox.pos.y =
		    convertTilePosToScreenPos(tileSize, offset, currentMap->drawOffset, tilePos).y;
	    }
				    
	    _selectionBox.pos.h = tileSize + absValue(tilePos.y - startedTilePos.y)*tileSize;

	    clipSelectionBoxToBoundary(&_selectionBox, currentMap->visibleArea);
	}
	else
	{
	    _selectionVisible = pointInRect(mousePos, currentMap->visibleArea);
	    if (_selectionVisible)
		moveSelectionInScrolledMap(&_selectionBox, currentMap->visibleArea, currentMap->drawOffset, mousePos, tileSize);
	}
    }
    else if(_currentTool == MOVE_TOOL && _panel.visible && currentMap->tiles)
    {
	if (pointInRect(mousePos, currentMap->visibleArea))
	{
	    SDL_SetCursor(_handCursor);
	    
	    if (leftClickFlag && (_horizontalBar.scrollingRect.image || _verticalBar.scrollingRect.image))
	    {
		currentMap->drawOffset.x += _lastFramePos.x - mousePos.x;
	        currentMap->drawOffset.y += _lastFramePos.y - mousePos.y;

		if (currentMap->drawOffset.x < 0)
		{
		    currentMap->drawOffset.x = 0;
		}
		else if (currentMap->drawOffset.x > currentMap->widthInTiles*tileSize - currentMap->visibleArea.w)
		{
		    currentMap->drawOffset.x = currentMap->widthInTiles*tileSize - currentMap->visibleArea.w;
		}

		if (currentMap->drawOffset.y < 0)
		{
		    currentMap->drawOffset.y = 0;
		}
		else if (currentMap->drawOffset.y > currentMap->heightInTiles*tileSize - currentMap->visibleArea.h)
		{
		    currentMap->drawOffset.y = currentMap->heightInTiles*tileSize - currentMap->visibleArea.h;
		}

		TexturedRect *scrollingBarY = &_verticalBar.scrollingRect;
		TexturedRect *backgroundBarY = &_verticalBar.backgroundRect;
		TexturedRect *scrollingBarX = &_horizontalBar.scrollingRect;
		TexturedRect *backgroundBarX = &_horizontalBar.backgroundRect;
		
		scrollingBarY->pos.y = (int32)((real32)currentMap->drawOffset.y / (currentMap->heightInTiles*tileSize - currentMap->visibleArea.h) * (backgroundBarY->pos.h - scrollingBarY->pos.h) + backgroundBarY->pos.y);
		scrollingBarX->pos.x = (int32)((real32)currentMap->drawOffset.x / (currentMap->widthInTiles*tileSize - currentMap->visibleArea.w) * (backgroundBarX->pos.w - scrollingBarX->pos.w) + backgroundBarX->pos.x);
					
		if (scrollingBarX->pos.x < backgroundBarX->pos.x)
		{
		    scrollingBarX->pos.x = backgroundBarX->pos.x;
		}
		else if (scrollingBarX->pos.x > backgroundBarX->pos.x + backgroundBarX->pos.w - scrollingBarX->pos.w)
		{
		    scrollingBarX->pos.x = backgroundBarX->pos.x + backgroundBarX->pos.w - scrollingBarX->pos.w;
		}

		if (scrollingBarY->pos.y < backgroundBarY->pos.y)
		{
		    scrollingBarY->pos.y = backgroundBarY->pos.y;
		}
		else if (scrollingBarY->pos.y > backgroundBarY->pos.y + backgroundBarY->pos.h - scrollingBarY->pos.h)
		{
		    scrollingBarY->pos.y = backgroundBarY->pos.y + backgroundBarY->pos.h - scrollingBarY->pos.h;
		}

		_lastFramePos = mousePos;
	    }
	}
    }

    _hoverToolIconVisible = true;
    if (pointInRect(mousePos, _paintToolIcon.background.pos))
    {
	_hoveringToolIcon.pos.x = _paintToolIcon.background.pos.x;
	_hoveringToolIcon.pos.y = _paintToolIcon.background.pos.y;
    }
    else if (pointInRect(mousePos, _fillToolIcon.background.pos))
    {
	_hoveringToolIcon.pos.x = _fillToolIcon.background.pos.x;
	_hoveringToolIcon.pos.y = _fillToolIcon.background.pos.y;
    }
    else if (pointInRect(mousePos, _moveToolIcon.background.pos))
    {
	_hoveringToolIcon.pos.x = _moveToolIcon.background.pos.x;
	_hoveringToolIcon.pos.y = _moveToolIcon.background.pos.y;
    }
    else
    {
	_hoverToolIconVisible = false;
    }
}

void tileMapPanelOnMouseDown(Vector2 mousePos, uint8 mouseButton)
{
    TileMap *currentMap = &_tileMaps[_selectedTileMap];
    int32 tileSize = currentMap->tileSize;
    
    if (pointInRect(mousePos, _horizontalBar.scrollingRect.pos))
    {
        _horizontalBar.scrolling = true;
	_horizontalBar.scrollClickDelta = mousePos.x - _horizontalBar.scrollingRect.pos.x;
    }
    else if (pointInRect(mousePos, _verticalBar.scrollingRect.pos))
    {
	_verticalBar.scrolling = true;
	_verticalBar.scrollClickDelta = mousePos.y - _verticalBar.scrollingRect.pos.y;
    }
				    
    ui_processMouseDown(&_panel, mousePos, mouseButton);
				    
    _createNewButton.startedClick = pointInRect(mousePos, _createNewButton.background.pos);

    if (_currentTool == PAINT_TOOL && currentMap->tiles)
    {
	if (mouseButton == SDL_BUTTON_LEFT)
	{
	    if (pointInRect(mousePos, currentMap->visibleArea))
	    {	
		paintSelectedTile(currentMap, currentMap->visibleArea,
				  currentMap->drawOffset, mousePos);
	    }
	}
    }
    else if (_currentTool == FILL_TOOL && currentMap->tiles)
    {
	if (mouseButton == SDL_BUTTON_LEFT)
	{
	    if (pointInRect(mousePos, currentMap->visibleArea))
	    {
		Vector2 offset = {currentMap->visibleArea.x,
				  currentMap->visibleArea.y};

		//TODO(denis): this seems redudant and silly
		Vector2 tilePos =
		    convertScreenPosToTilePos(tileSize, offset,
					      currentMap->drawOffset, mousePos);

		_startSelectPos =
		    convertTilePosToScreenPos(tileSize, offset,
					      currentMap->drawOffset, tilePos);

		if (_startSelectPos.x < offset.x)
		{
		    _selectionBox.pos.x = offset.x;
		    _selectionBox.pos.w = tileSize - currentMap->drawOffset.x%tileSize;
		}
		else
		{
		    _selectionBox.pos.x = _startSelectPos.x;   
		    _selectionBox.pos.w = tileSize;
		}

		if (_startSelectPos.y < offset.y)
		{
		    _selectionBox.pos.y = offset.y;
		    _selectionBox.pos.h = tileSize - currentMap->drawOffset.y%tileSize;
		}
		else
		{
		    _selectionBox.pos.y = _startSelectPos.y;
		    _selectionBox.pos.h = tileSize;
		}

		clipSelectionBoxToBoundary(&_selectionBox, currentMap->visibleArea);
	    }
	    else
	    {
		_startSelectPos = {0, 0};
	    }
	}
    }
    else if (_currentTool == MOVE_TOOL && currentMap->tiles)
    {
	if ((_horizontalBar.backgroundRect.image || _verticalBar.backgroundRect.image) &&
	    mouseButton == SDL_BUTTON_LEFT)
	{
	    if (pointInRect(mousePos, currentMap->visibleArea))
	    {
		_lastFramePos = mousePos;
	    }
	}
    }
}

void tileMapPanelOnMouseUp(Vector2 mousePos, uint8 mouseButton)
{
    TileMap *currentMap = &_tileMaps[_selectedTileMap];
    int32 tileSize = currentMap->tileSize;
    
    _verticalBar.scrolling = false;
    _horizontalBar.scrolling = false;

    //TODO(denis): implement a proper "radio button" type
    // situation
    if (ui_wasClicked(_paintToolIcon, mousePos))
    {	
	changeCurrentTool(&_currentTool, PAINT_TOOL,
			  &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			  &_selectedToolIcon, &_selectionVisible);
    }
    else if (ui_wasClicked(_fillToolIcon, mousePos))
    {
	changeCurrentTool(&_currentTool, FILL_TOOL,
			  &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			  &_selectedToolIcon, &_selectionVisible);
    }
    else if (ui_wasClicked(_moveToolIcon, mousePos))
    {
	changeCurrentTool(&_currentTool, MOVE_TOOL,
			  &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			  &_selectedToolIcon, &_selectionVisible);
    }

    if (!currentMap->tiles)
    {
	if (ui_wasClicked(_createNewButton, mousePos))
	{
	    newTileMapPanelSetVisible(true);
	    int32 tileSetTileSize = tileSetPanelGetCurrentTileSize();
	    if (tileSetTileSize != 0)
	    {
		newTileMapPanelSetTileSize(tileSetTileSize);
	    }
	    
	    _createNewButton.startedClick = false;
	}
    }

    //NOTE(denis): tool behaviour
    if (currentMap->tiles)
    {
	if (_currentTool == FILL_TOOL)
	{
	    if (_selectionVisible && _startSelectPos != Vector2{0,0})
	    {
		Vector2 topLeft = {_selectionBox.pos.x, _selectionBox.pos.y};
				    
		Vector2 botRight = {_selectionBox.pos.x + _selectionBox.pos.w-1,
				    _selectionBox.pos.y + _selectionBox.pos.h-1};

		Vector2 offset = {currentMap->visibleArea.x, currentMap->visibleArea.y};
		Vector2 startTile =
		    convertScreenPosToTilePos(tileSize, offset, currentMap->drawOffset, topLeft);
		Vector2 endTile =
		    convertScreenPosToTilePos(tileSize, offset, currentMap->drawOffset, botRight);

		if (endTile.x > currentMap->widthInTiles)
		{
		    endTile.x = currentMap->widthInTiles;
		}
		if (endTile.y > currentMap->heightInTiles)
		{
		    endTile.y = currentMap->heightInTiles;
		}
				    
		for (int i = startTile.y; i <= endTile.y; ++i)
		{
		    for (int j = startTile.x; j <= endTile.x; ++j)
		    {
			if (tileSetPanelGetSelectedTile().sheetPos.w != 0)
			{
			    (currentMap->tiles + i*currentMap->widthInTiles + j)->sheetPos = tileSetPanelGetSelectedTile().sheetPos;
			}
		    }
		}
	    }

	    _selectionVisible = pointInRect(mousePos, currentMap->visibleArea);
	    if (_selectionVisible)
	    {
		moveSelectionInScrolledMap(&_selectionBox, currentMap->visibleArea, currentMap->drawOffset, mousePos, tileSize);
	    }
	}
    }
}

void tileMapPanelOnKeyPressed(SDL_Keycode key)
{
    if (_tileMaps[_selectedTileMap].tiles)
    {
	if (_currentTool != MOVE_TOOL)
	    _previousTool = _currentTool;
    
	changeCurrentTool(&_currentTool, MOVE_TOOL,
			  &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			  &_selectedToolIcon, &_selectionVisible);
    }
}

void tileMapPanelOnKeyReleased(SDL_Keycode key)
{
    if (_tileMaps[_selectedTileMap].tiles)
    {
	if (key == SDLK_SPACE)
	{
	    changeCurrentTool(&_currentTool, _previousTool,
			      &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			      &_selectedToolIcon, &_selectionVisible);
	}
	else if (key == SDLK_m)
	{
	    changeCurrentTool(&_currentTool, MOVE_TOOL,
			      &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			      &_selectedToolIcon, &_selectionVisible);
	}
	else if (key == SDLK_f)
	{
	    changeCurrentTool(&_currentTool, FILL_TOOL,
			      &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			      &_selectedToolIcon, &_selectionVisible);
	}
	else if (key == SDLK_p)
	{
	    changeCurrentTool(&_currentTool, PAINT_TOOL,
			      &_paintToolIcon, &_fillToolIcon, &_moveToolIcon,
			      &_selectedToolIcon, &_selectionVisible);
	}
    }
}

TileMap* tileMapPanelAddNewTileMap()
{
    TileMap *result = 0;
    
    int32 x = _tileMapArea.x;
    int32 y = _tileMapArea.y;
    int32 tileMapWidth = 0;
    int32 tileMapHeight = 0;

    int32 tileSize = 0;
			
    _tileMaps[_numTileMaps] = createNewTileMap(x, y, &tileMapWidth, &tileMapHeight);
    result = &_tileMaps[_numTileMaps];
    tileSize = result->tileSize;
    
    _selectedTileMap = _numTileMaps;
    ++_numTileMaps;
			
    result->visibleArea = _tileMapArea;
    //TODO(denis): these don't have to be restricted to only
    // showing a multiple of tile sizes
    result->visibleArea.w = MIN(tileMapWidth, (_tileMapArea.w/tileSize)*tileSize);
    result->visibleArea.h = MIN(tileMapHeight, (_tileMapArea.h/tileSize)*tileSize);
			
    if (tileMapWidth > _tileMapArea.w)
    {
	int32 backgroundWidth = (_tileMapArea.w/tileSize)*tileSize;
	result->visibleArea.w = backgroundWidth;

	real32 sizeRatio = (real32)_tileMapArea.w/(real32)tileMapWidth;
	int32 bigBarWidth = backgroundWidth;
	int32 smallBarWidth = (int32)(_tileMapArea.w*sizeRatio);
	int32 barX = _tileMapArea.x;
	int32 barY = result->offset.y + result->visibleArea.h + 4;
	
	_horizontalBar =
	    ui_createScrollBar(barX, barY, bigBarWidth, smallBarWidth,
			       SCROLL_BAR_WIDTH, SCROLL_BAR_BIG_COLOUR, SCROLL_BAR_SMALL_COLOUR,
			       false);
    }
    if (tileMapHeight > _tileMapArea.h)
    {
	int32 backgroundHeight = (_tileMapArea.h/tileSize)*tileSize;
	result->visibleArea.h = backgroundHeight;

	real32 sizeRatio = (real32)_tileMapArea.h/(real32)tileMapHeight;
	int32 barX = result->offset.x + result->visibleArea.w + 4;
	int32 barY = _tileMapArea.y;
	int32 smallBarWidth = (int32)(_tileMapArea.h*sizeRatio);
	
	_verticalBar =
	    ui_createScrollBar(barX, barY, backgroundHeight, smallBarWidth,
			       SCROLL_BAR_WIDTH, SCROLL_BAR_BIG_COLOUR, SCROLL_BAR_SMALL_COLOUR,
			       true);
    }

    newTileMapPanelResetData();
    newTileMapPanelSetVisible(false);

    if (_selectionBox.pos.w == 0 && _selectionBox.pos.h == 0)
    {
	initializeSelectionBox(_renderer, &_selectionBox, tileSize);
    }

    return result;
}

bool tileMapPanelVisible()
{
    return _panel.visible;
}

void tileMapPanelSetVisible(bool newValue)
{
    _panel.visible = newValue;
}

bool tileMapPanelTileMapIsValid()
{
    //TODO(denis): actually check things here
    return _tileMaps[_selectedTileMap].name != 0;
}

TileMap* tileMapPanelGetCurrentTileMap()
{
    return &_tileMaps[_selectedTileMap];
}

void tileMapPanelSelectTileMap(uint32 newSelection)
{
    if (newSelection < _numTileMaps)
    {
	_selectedTileMap = newSelection;
    }
}