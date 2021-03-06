#include "SDL_rect.h"
#include "SDL_image.h"
#include "denis_math.h"
#include "math.h"

static char* getFileNameFromPath(char *path)
{
    char *result = 0;
    
    int startOfFileName = 0;
    int endOfFileName = 0;
    for (int i = 0; path[i] != 0; ++i)
    {
	if (path[i] == '\\' || path[i] == '/')
	    startOfFileName = i+1;

	if (path[i+1] == 0)
	    endOfFileName = i+1;
    }
    
    result = (char*)HEAP_ALLOC((endOfFileName-startOfFileName+1)*sizeof(char));

    if (result)
    {
	copyIntoString(result, path, startOfFileName, endOfFileName);
	
	result[endOfFileName-startOfFileName] = 0;
    }
    
    return result;
}

static char* getProgramPathName()
{
    char *result = 0;
    
    TCHAR fileNameBuffer[MAX_PATH+1];
    DWORD getFileNameResult = GetModuleFileName(NULL, fileNameBuffer, MAX_PATH+1);
    if (getFileNameResult != 0 &&
	GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
	char filePath[MAX_PATH+1] = {};
	uint32 indexOfLastSlash = 0;
	for (int i = 0; i < MAX_PATH && fileNameBuffer[i] != 0; ++i)
	{
	    if (fileNameBuffer[i] == '\\')
		indexOfLastSlash = i;
	}

	copyIntoString(filePath, fileNameBuffer,
		       0, indexOfLastSlash);

	result = duplicateString(filePath);
    }
    else
    {
	//TODO(denis): try again with a bigger buffer?
    }

    return result;
}

static inline bool pointInRect(Vector2 point, SDL_Rect rect)
{
    return point.x > rect.x && point.x < rect.x+rect.w &&
        point.y > rect.y && point.y < rect.y+rect.h;
}

static inline bool pointInRect(Point2 point, SDL_Rect rect)
{
    return point.x > rect.x && point.x < rect.x+rect.w &&
        point.y > rect.y && point.y < rect.y+rect.h;
}

static inline char* convertIntToString(int num)
{
    //NOTE(denis): +1 because log10 is one character off
    // and the second +1 because we want to create a valid null-terminated string
    int numCharacters = ((int)log10((float)num)+1) + 1;
    char *result = (char*)HEAP_ALLOC(numCharacters);

    for (int i = numCharacters-2; i >= 0; --i)
    {
	result[i] = '0' + num%10;
	num /= 10;
    }
    
    return result;
}

static inline int convertStringToInt(char string[], int size)
{
    int result = 0;

    for (int i = 0; i < size; ++i)
    {
	int num = (string[i]-'0') * exponent(10, size-1-i);
	result += num;
    }

    return result;
}

static TexturedRect createFilledTexturedRect(SDL_Renderer *renderer,
					     int width, int height, uint32 colour)
{
    TexturedRect result = {};

    uint32 rmask, gmask, bmask, amask;
    amask = 0xFF000000;
    rmask = 0x00FF0000;
    gmask = 0x0000FF00;
    bmask = 0x000000FF;
    
    SDL_Surface *rectangle = SDL_CreateRGBSurface(0, width, height, 32,
						  rmask, gmask, bmask, amask);
    SDL_FillRect(rectangle, NULL, colour);
    result.image = SDL_CreateTextureFromSurface(renderer, rectangle);
    SDL_GetClipRect(rectangle, &result.pos);
    SDL_FreeSurface(rectangle);

    return result;
}

static inline TexturedRect loadImage(SDL_Renderer *renderer, char *fileName)
{
    TexturedRect result = {};
    
    SDL_Surface *tempSurf = IMG_Load(fileName);
    result.image = SDL_CreateTextureFromSurface(renderer, tempSurf);
    SDL_GetClipRect(tempSurf, &result.pos);
    SDL_FreeSurface(tempSurf);
    
    return result;
}

static inline SDL_Surface* loadImageAsSurface(char *fileName)
{
    SDL_Surface *result = 0;

    result = IMG_Load(fileName);
    
    return result;
}

static inline SDL_Color hexColourToRGBA(uint32 hex)
{
    SDL_Color result = {};

    //NOTE(denis): hex colour format 0xAARRGGBB
    result.r = (hex >> 16) & 0xFF;
    result.g = (hex >> 8) & 0xFF;
    result.b = hex & 0xFF;
    result.a = hex >> 24;

    return result;
}

//TODO(denis): dunno if these belong here
static void initializeSelectionBox(SDL_Renderer *renderer,
				   TexturedRect *selectionBox, uint32 tileSize)
{
    *selectionBox = createFilledTexturedRect(renderer, tileSize, tileSize, 0x77FFFFFF);
}

static bool moveSelectionInRect(TexturedRect *selectionBox, Vector2 mousePos,
				SDL_Rect rect, uint32 tileSize)
{
    bool shouldBeDrawn = true;
    
    if (selectionBox->image != NULL)
    {
	if (pointInRect(mousePos, rect))
	{
	    Vector2 offset = {rect.x, rect.y};
	    Vector2 newPos = ((mousePos - offset)/tileSize) * tileSize + offset;
	    
	    selectionBox->pos.x = newPos.x;
	    selectionBox->pos.y = newPos.y;
	    selectionBox->pos.h = tileSize;
	    selectionBox->pos.w = tileSize;
	}
	else
	    shouldBeDrawn = false;
    }
    
    return shouldBeDrawn;
}
