/* TODO(denis):
 *
 * CTRL + N -> create new tile map
 * CTRL + O -> open tile map
 * CTRL + S -> save current tile map
 * etc...
 *
 * DISABLE top menu bar when popup is visible
 *
 * add a keyboard shortcut to change to next tool and change to previous tool
 *
 * add tool tips
 *
 * when typing in the panels, disable the shortcut keys
 *
 * when using the fill tool, make right click cancel the selection entirely
 *
 * add the ability to close maps and tile sets
 *
 * closing tile maps is a bit weird when there are no more tile maps
 *
 * fix the "everything is a different font" issue
 *
 */

//IMPORTANT(denis): note to self, design everything expecting at least a 1280 x 720
// window

#include <SDL.h>
#include "SDL_ttf.h"
#include "SDL_image.h"
#include "windows.h"
#include <math.h>

#include "ui_elements.h"
#include "main.h"
#include "file_saving_loading.h"
#include "denis_math.h"
#include "new_tile_map_panel.h"
#include "tile_set_panel.h"
#include "tile_map_panel.h"
#include "import_tile_set_panel.h"
#include "TEMP_GeneralFunctions.cpp"

#define TITLE "Tile Map Editor"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define BACKGROUND_COLOUR 60,67,69,255

static inline void openNewTileMapPanel()
{
    newTileMapPanelResetData();
    newTileMapPanelSetVisible(true);

    int32 tileSize = tileSetPanelGetCurrentTileSize();
    if (tileSize != 0)
    {
	newTileMapPanelSetTileSize(tileSize);
    }
}

static inline void openImportTileSetPanel()
{
    importTileSetPanelSetVisible(true);

    int32 tileSize = newTileMapPanelGetTileSize();
    if (tileSize != 0)
    {
	importTileSetPanelSetTileSize(tileSize);
    }
}

static inline void addTileMapToMenuBar(DropDownMenu *menu, char *tileMapName)
{
    if (menu->itemCount == 2)
    {
        menu->addItem("Close Tile Map", menu->itemCount-1);
    }
    menu->addItem(tileMapName, menu->itemCount-2);
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_Window *window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED,
					  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
					  WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_SetWindowMinimumSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    char *defaultFontName= "LiberationMono-Regular.ttf";
    int defaultFontSize = 16;

    char *menuFontName = "LiberationMono-Regular.ttf";
    int menuFontSize = 14;
    
    if (window)
    {
	uint32 renderFlags = SDL_RENDERER_PRESENTVSYNC;
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, renderFlags);

	if (renderer && ui_init(renderer, defaultFontName, defaultFontSize)
	    && IMG_Init(IMG_INIT_PNG) != 0)
	{   
	    bool running = true;

	    //NOTE(denis): setting up the top bar
	    ui_setFont(menuFontName, menuFontSize);

	    MenuBar topMenuBar = ui_createMenuBar(0, 0, WINDOW_WIDTH, 20,
						  0xFF667378, 0xFF000000);
	    
	    char *items[] = {"File", "Create New Tile Map", "Open Tile Map...",
			     "Save Tile Map...", "Import Tile Sheet...", "Exit"};	     
	    topMenuBar.addMenu(items, 6, 225);
	    
	    items[0] = "Tile Maps";
	    items[1] = "Create New";
	    topMenuBar.addMenu(items, 2, 225);
	    
	    //NOTE(denis): create new tile map panel
	    ui_setFont(defaultFontName, defaultFontSize);
	    
	    {
		createNewTileMapPanel(0, 0, 0, 0);
		int centreX = WINDOW_WIDTH/2 - newTileMapPanelGetWidth()/2;
		int centreY = WINDOW_HEIGHT/2 - newTileMapPanelGetHeight()/2;
		newTileMapPanelSetPosition({centreX, centreY});
		newTileMapPanelSetVisible(false);
	    }

	    //NOTE(denis): import tile sheet panel
	    {
		int x = WINDOW_WIDTH/2 - 900/2;
		int y = WINDOW_HEIGHT/2 - 150/2;
		importTileSetPanelCreateNew(renderer, x, y, 0, 0);
	    }
	    
	    //NOTE(denis): tile set panel
	    {
		int width = WINDOW_WIDTH/3;
		int height = WINDOW_HEIGHT - 
		    (topMenuBar.botRight.y - topMenuBar.topLeft.y) - 30;
		int x = WINDOW_WIDTH - width - 15;
		int y = topMenuBar.botRight.y + 15;

		tileSetPanelCreateNew(renderer, x, y, width, height);
	    }
	    
	    //NOTE(denis): tile map panel
	    {
		uint32 padding = 15;
		uint32 x = padding;
		uint32 y = topMenuBar.botRight.y + padding;
		uint32 width = 800;
		uint32 height = WINDOW_HEIGHT - y - padding;
		
		tileMapPanelCreateNew(renderer, x, y, width, height);
	    }

	    //NOTE(denis): automatic tile sheet opening panel
	    UIPanel openTileSheetPanel = {};
	    TexturedRect titleText = {};
	    Button openButton = {};
	    Button cancelButton = {};
	    TextBox tileSheetNameText = {};
	    Button browseButton = {};
	    TextBox lastModifiedText = {};
	    {
		int32 x, y, width, height;
		width = WINDOW_WIDTH/2;
		height = WINDOW_HEIGHT/2;
		x = WINDOW_WIDTH/2 - width/2;
		y = WINDOW_HEIGHT/2 - height/2;
		uint32 colour = 0xFF888800;
		openTileSheetPanel = ui_createPanel(x, y, width, height, colour);

		//TODO(denis): set font for title
		x += 15;
		y += 15;
		colour = 0xFFFFFFFF;
		titleText = ui_createTextField("Open this tile sheet?", x, y, colour);
		ui_addToPanel(&titleText, &openTileSheetPanel);
		
		//TODO(denis): set font for other text
		uint32 textColour = 0xFFFFFFFF;
		colour = 0xFF888888;
		openButton = ui_createTextButton("Open", textColour, 0, 0, colour);
		y = openTileSheetPanel.panel.pos.y + openTileSheetPanel.panel.pos.h - openButton.getHeight() - 15;
		openButton.setPosition({x, y});
		ui_addToPanel(&openButton, &openTileSheetPanel);
		
		cancelButton = ui_createTextButton("Cancel", textColour, 0, 0, colour);
		x = openTileSheetPanel.panel.pos.x + openTileSheetPanel.panel.pos.w - cancelButton.getWidth() - 15;
		y = openButton.background.pos.y;
		cancelButton.setPosition({x, y});
		ui_addToPanel(&cancelButton, &openTileSheetPanel);
		
		textColour = 0xFF000000;
		colour = 0xFFFFFFFF;
		width = openTileSheetPanel.getWidth() - 30;
		tileSheetNameText = ui_createTextBox("No tile sheet found", width, 0,
						     textColour, colour);
		x = titleText.pos.x;
		y = titleText.pos.y + titleText.pos.h + 15;
		tileSheetNameText.setPosition({x, y});
		ui_addToPanel(&tileSheetNameText, &openTileSheetPanel);
		
		lastModifiedText = ui_createTextBox("last modified: 00:00 00/00/0000", width, 0,
						    textColour, colour);
		x = titleText.pos.x;
		y = tileSheetNameText.pos.y + tileSheetNameText.getHeight() + 15;
		lastModifiedText.setPosition({x, y});
		ui_addToPanel(&lastModifiedText, &openTileSheetPanel);

		textColour = 0xFFFFFFFF;
		colour = 0xFF888888;
		browseButton = ui_createTextButton("Browse for Tile Sheet", textColour, 0, 0, colour);
		x = titleText.pos.x;
		y = lastModifiedText.pos.y + lastModifiedText.getHeight() + 15;
		browseButton.setPosition({x, y});
		ui_addToPanel(&browseButton, &openTileSheetPanel);
		
		openTileSheetPanel.visible = false;
	    }
	    
	    char *programPathName = getProgramPathName();
	    char *tileSheetDirectory = concatStrings(programPathName, TILE_SHEET_FOLDER);
	    
	    if (CreateDirectory(tileSheetDirectory, NULL) == 0)
	    {
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
		    //TODO(denis): probably want to try again or log the error or
		    // something
		}
	    }

	    HEAP_FREE(programPathName);
	    programPathName = 0;
	    
	    while (running)
	    {
		static uint32 topMenuOpenDelay = 0;
                #define DELAY_THRESHOLD 20
		
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
		    switch(event.type)
		    {
			case SDL_QUIT:
			{
			    running = false;
			} break;

			case SDL_WINDOWEVENT:
			{
			    if (event.window.event == SDL_WINDOWEVENT_RESIZED)
			    {
				int windowHeight = 0;
				int windowWidth = 0;
				SDL_GetWindowSize(window, &windowWidth, &windowHeight);

			        topMenuBar.botRight.x =
				    topMenuBar.topLeft.x + windowWidth;

				//TODO(denis): resize all the panels here

				
				//TODO(denis): might want to resize the new tile map
				// panel here
				if (newTileMapPanelVisible())
				{
				    Vector2 newPos = {};
				    newPos.x = windowWidth/2 - newTileMapPanelGetWidth()/2;
				    newPos.y = windowHeight/2 - newTileMapPanelGetHeight()/2;
				    newTileMapPanelSetPosition(newPos);
				}
			    }
			    
			} break;

			case SDL_MOUSEMOTION:
			{
			    //TODO(denis): also handle when the user has focus on our
			    // window but has moved off of it
			    
			    Vector2 mouse = {event.motion.x, event.motion.y};
			    int32 leftClickFlag = event.motion.state & SDL_BUTTON_LMASK;
			    
			    topMenuBar.onMouseMove(mouse);
			    if (!topMenuBar.isOpen())
			    {
				if (tileSetPanelVisible())
				{
				    tileSetPanelOnMouseMove(mouse);
				}

				if (tileMapPanelVisible())
				{
				    tileMapPanelOnMouseMove(mouse, leftClickFlag);
				}
			    }
			    
			} break;

			case SDL_MOUSEBUTTONDOWN:
			{
			    Vector2 mouse = {event.button.x, event.button.y};
			    uint8 mouseButton = event.button.button;

			    if (importTileSetPanelVisible())
			    {
				importTileSetPanelOnMouseDown(mouse, mouseButton);
			    }
			    else if (newTileMapPanelVisible())
			    {
				newTileMapPanelOnMouseDown(mouse, mouseButton);
			    }
			    else if (tileSetPanelVisible() || tileMapPanelVisible())
			    {
				if (!topMenuBar.isOpen() &&
				    topMenuOpenDelay > DELAY_THRESHOLD)
				{
				    if (tileSetPanelVisible())
				    {
					tileSetPanelOnMouseDown(mouse, mouseButton);
				    }

				    if (tileMapPanelVisible())
				    {
					tileMapPanelOnMouseDown(mouse, mouseButton);
				    }
				}
			    }
			   
			    topMenuBar.onMouseDown(mouse, event.button.button);
			    
			} break;
			
			case SDL_MOUSEBUTTONUP:
			{   
			    Vector2 mouse = {event.button.x, event.button.y};
			    uint8 mouseButton = event.button.button;
			    
			    if (importTileSetPanelVisible())
			    {
				importTileSetPanelOnMouseUp(mouse, mouseButton);
			    }
			    else if (newTileMapPanelVisible())
			    {
				newTileMapPanelOnMouseUp(mouse, mouseButton);
			    }
			    else if (topMenuBar.onMouseUp(mouse, event.button.button))
			    {
				topMenuOpenDelay = 0;
				
				//TODO(denis): need better solution
				tileSetPanelOnMouseUp({0,0}, mouseButton);
				
				if (pointInRect(mouse, topMenuBar.menus[0].getRect()))
				{
				    //NOTE(denis): clicked on the file menu

				    topMenuBar.menus[0].isOpen = false;
				    
				    int selectionY = (mouse.y - topMenuBar.menus[0].getRect().y)/topMenuBar.menus[0].items[0].pos.h;
				    if (selectionY == 1)
				    {
					//NOTE(denis): 1 == "create new tile map"
				        openNewTileMapPanel();
				    }
				    else if (selectionY == 2)
				    {
					//NOTE(denis): 2 == "open tile map file"
					LoadTileMapResult data = {};
					data = loadTileMapFromFile();

					SDL_Surface *tileSet = 0;
					//TODO(denis): need to free
					char *tileSheetFullPath = 0;
					
					if (tileSheetDirectory)
					{
					    tileSheetFullPath = concatStrings(tileSheetDirectory, data.tileSheetFileName);
					    tileSet = loadImageAsSurface(tileSheetFullPath);
					}

					openTileSheetPanel.visible = true;
					if (tileSet)
					{
					    //TODO(denis): 
					    // the prompt should have the name of the
					    // tile sheet, the last date it was modified,
					    // and maybe a small picture of it?
					    // or maybe a full sized scrollable picture?

					    //TODO(denis): if the user opens a new tile sheet
					    // that meddles with the positions of the tiles
					    // I want my program to recognize where the tiles
					    // have moved and update the tile map tile sheet positions
					    // as necessary so that it still draws properly
					    //tileSetPanelInitializeNewTileSet(data.tileSheetFileName, tileSet, data.tileSize);

					    WIN32_FILE_ATTRIBUTE_DATA fileInformation = {};
					    SYSTEMTIME lastWriteTimeUniversal = {};
					    SYSTEMTIME lastWriteTimeLocal = {};
					    
					    if (GetFileAttributesEx(tileSheetFullPath, GetFileExInfoStandard, &fileInformation) != 0)
					    {
						if (FileTimeToSystemTime(&fileInformation.ftLastWriteTime, &lastWriteTimeUniversal) != 0 &&
						    SystemTimeToTzSpecificLocalTime(NULL, &lastWriteTimeUniversal, &lastWriteTimeLocal) != 0)
						{
						    char *year = convertIntToString(lastWriteTimeLocal.wYear);
						    char *month = convertIntToString(lastWriteTimeLocal.wMonth);
						    char *day = convertIntToString(lastWriteTimeLocal.wDay);
						    char *hour = convertIntToString(lastWriteTimeLocal.wHour);
						    char *minute = convertIntToString(lastWriteTimeLocal.wMinute);

						    //TODO(denis): put these into a singular string
						    // and set the text of lastModifiedText
						}
					    }

					    ui_setText(&tileSheetNameText, data.tileSheetFileName);
					    tileSheetNameText.setPosition(tileSheetNameText.getPosition());
					}
					else
					{
					    //TODO(denis): use the same prompt but
					    // the tile sheet is empty and the
					    // user must open the correct one
					    // if the tilesheet name is different than
					    // data.tileSheetFileName then save the
					    // new name into the tile map
					}

					//TODO(denis): if it was cancelled, don't do all this
					
					TileMapTile *tileMapTiles = (TileMapTile*)HEAP_ALLOC(data.tileMapWidth*data.tileMapHeight*sizeof(TileMapTile));

					assert(tileMapTiles);
					for (uint32 i = 0; i < data.tileMapHeight; ++i)
					{
					    for (uint32 j = 0; j < data.tileMapWidth; ++j)
					    {
						(tileMapTiles + i*data.tileMapWidth + j)->tile = *(data.tiles + i*data.tileMapWidth + j);
						(tileMapTiles + i*data.tileMapWidth + j)->initialized = true;
					    }
					}

					HEAP_FREE(data.tiles);
					
					TileMap *tileMap = tileMapPanelAddTileMap(tileMapTiles, data.tileMapName, data.tileMapWidth, data.tileMapHeight, data.tileSize,
										  data.tileSheetFileName);
					addTileMapToMenuBar(&topMenuBar.menus[1], tileMap->name);
				    }
				    else if (selectionY == 3)
				    {
					//NOTE(denis): 3 == "save tile map to file"
					if (tileMapPanelTileMapIsValid())
					{
					    TileMap *current = tileMapPanelGetCurrentTileMap();
					    char *fileName = current->name;

					    saveTileMapToFile(current, fileName);
					}
				    }
				    else if (selectionY == 4)
				    {
					//NOTE(denis): 4 = "import tile sheet"
					openImportTileSetPanel();
				    }
				    else if (selectionY == 0)
				    {
					topMenuBar.menus[0].isOpen = true;
				    }
				    
				}
				else if (pointInRect(mouse, topMenuBar.menus[1].getRect()))
				{
				    //NOTE(denis): clicked on the tile map menu
				    topMenuBar.menus[1].isOpen = false;
				    
				    uint32 selectionY = (mouse.y - topMenuBar.menus[1].getRect().y)/topMenuBar.menus[1].items[0].pos.h;
				    
				    if (selectionY == (uint32)(topMenuBar.menus[1].itemCount-1))
				    {
					openNewTileMapPanel();
				    }
				    else if (selectionY == (uint32)(topMenuBar.menus[1].itemCount-2) &&
					     topMenuBar.menus[1].itemCount > 2)
				    {
					//NOTE(denis): close tile map
					uint32 selectedTileMap = tileMapPanelGetCurrentTileMapIndex();
					topMenuBar.menus[1].removeItem(selectedTileMap+1);

					tileMapPanelRemoveTileMap(selectedTileMap);

					if (!tileMapPanelGetCurrentTileMap()->tiles)
					{
					    //NOTE(denis): remove "close tile map" from the menu
					    topMenuBar.menus[1].removeItem(1);
					}
				    }
				    else if (selectionY != 0)
				    {
					tileMapPanelSelectTileMap(selectionY-1);
				    }
				    else if (selectionY == 0)
				    {
					topMenuBar.menus[1].isOpen = true;
				    }
				}
			    }
			    else if (tileSetPanelVisible() || tileMapPanelVisible())
			    {
				if (!topMenuBar.isOpen() &&
				    topMenuOpenDelay > DELAY_THRESHOLD)
				{
				    if (tileSetPanelVisible())
				    {
					tileSetPanelOnMouseUp(mouse, mouseButton);

					if (tileSetPanelImportTileSetPressed())
					    openImportTileSetPanel();
				    }
			    
				    if (tileMapPanelVisible())
				    {
					tileMapPanelOnMouseUp(mouse, mouseButton);
				    }
				}
			    }
			} break;

			case SDL_TEXTINPUT:
			{
			    char* theText = event.text.text;

			    if (newTileMapPanelVisible())
			    {
				newTileMapPanelCharInput(theText[0]);
			    }
			    else if(importTileSetPanelVisible())
			    {
				importTileSetPanelCharInput(theText[0]);
			    }
			    
			} break;

			case SDL_KEYDOWN:
			{
			    if (event.key.keysym.sym == SDLK_BACKSPACE)
			    {
				if (newTileMapPanelVisible())
				{
				    newTileMapPanelCharDeleted();
				}
				else if (importTileSetPanelVisible())
				{
				    importTileSetPanelCharDeleted();
				}  
			    }
			    
			    if (event.key.keysym.sym == SDLK_SPACE &&
				tileMapPanelVisible())
			    {
				tileMapPanelOnKeyPressed(SDLK_SPACE);
			    }
			    
			} break;

			case SDL_KEYUP:
			{
			    if (event.key.keysym.sym == SDLK_TAB)
			    {
				if (newTileMapPanelVisible())
				{
				    SDL_Keymod mod = SDL_GetModState();

				    if ((mod & KMOD_LSHIFT) || (mod & KMOD_RSHIFT))
					newTileMapPanelSelectPrevious();
				    else
					newTileMapPanelSelectNext();
				}
			    }
			    else if (event.key.keysym.sym == SDLK_RETURN ||
				     event.key.keysym.sym == SDLK_KP_ENTER)
			    {
				if (newTileMapPanelVisible())
				    newTileMapPanelEnterPressed();
			    }

			    tileMapPanelOnKeyReleased(event.key.keysym.sym);
			} break;
		    }
		}

		if (topMenuOpenDelay <= DELAY_THRESHOLD)
		    ++topMenuOpenDelay;
		
		SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOUR);
		SDL_RenderClear(renderer);

		if (newTileMapPanelVisible())
		{   
		    if (newTileMapPanelDataReady())
		    {
			TileMap *tileMap = tileMapPanelCreateNewTileMap();

			addTileMapToMenuBar(&topMenuBar.menus[1], tileMap->name);
		    }
		}

		tileSetPanelDraw();
		tileMapPanelDraw();    

		newTileMapPanelDraw();
		importTileSetPanelDraw();

		ui_draw(&openTileSheetPanel);
		
		ui_draw(&topMenuBar);
	
		SDL_RenderPresent(renderer);
	    }

	    IMG_Quit();
	}
	
	ui_destroy();
	if (renderer)
	    SDL_DestroyRenderer(renderer);
	if (window)
	    SDL_DestroyWindow(window);
    }

    SDL_Quit();
    return 0;
}
