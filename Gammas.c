
// This application presents a window to display various tables 
// associated with each display connected to the computer.
//
// The tables (or LUTs) that can be displayed for each display are:
//  1) the current LUT loaded in the display's video card 
//  2) the TRC tags of the current profile for the display
//      This curve repesents the combined responce curve of 
//      the display and the video card
//  3) the vgct tag of the current profile for the display.
//      This curve repesents the correction that is needed in order 
//      for the the displays native responce curve to appear as
//      the desires target response.
//  4) the ndin tag of the current profile for the display.
//      This curve repesents the native responce curve of the 
//      display alone (i.e with a linear video card table).
//
// In most situations tables 1 and 3 should be the same
// because ColorSync allways tries to load the 'vcgt' curve of 
// the current profile into the vidoe card.
//
// The current table displayed can be selected from the "View" menu.
// When appropriate, the current profile name is displayed
// When appropriate, the actual or approximate gamma value for the 
// table is displayed.
//
// This application requires CarbonLib 1.1 or greater.
// As a result, it can run on Mac OS 8.6 and greater.

#define TARGET_CARBON 1

#if __MWERKS__
	#include <MacTypes.h>
	#include <CarbonEvents.h>
	#include <MacWindows.h>
	#include <CFPreferences.h>
	#include <Displays.h>
	#include <Gestalt.h>
	#include <QuickDraw.h>
	#include <NumberFormatting.h>
	#include <CMApplication.h>
	#include <Debugging.h>
#else
	#include <Carbon/Carbon.h>
#endif

#include "math.h"


enum
{
	kLUTsCmd				= 'LUTs',
	kTRCsCmd				= 'TRCs',
	kvcgtCmd				= 'vcgt',
	kndinCmd				= 'ndin',
	
	kAboutCmd				= 'abou',
	kCloseCmd				= 'clos',
	kQuitCmd				= 'quit'
};


typedef struct curves
{
	UInt8	table[3][256];
	UInt16	tableCount;
	Fixed	max[3];
	Fixed	gam[3];
	Str255	str;
} curves;


// Globals 
Boolean		gOnX = false;
OSType		gMode = 0;

// Prototypes 
static pascal void EventLoopTimer (EventLoopTimerRef timer, void *userData);
static pascal OSStatus EventCommandProcess (EventHandlerCallRef callRef, EventRef event, void *userData);
static pascal OSStatus EventWindowDrawContent (EventHandlerCallRef callRef, EventRef event, void *userData);

static void Initialize (void);
static void InitializeWindows (void);

static void SetMenuCommandIDCheck(UInt32 commandID, Boolean check);
static StringPtr GetMenuCommandIDText(UInt32 commandID, Str255 string);

static void ChangeMode (OSType newMode);
static void UpdateTitles (void);
static void UpdateWindows (void);
static void DrawCurves (curves* c);

static OSStatus GetCurves (DisplayIDType id, curves* c, OSType mode);
static OSStatus GetVCGTCurvesCommon (CMVideoCardGamma* gamma, curves* c);
static OSStatus GetLutCurves (DisplayIDType id, curves* c);
static OSStatus GetVCGTCurves (DisplayIDType id, curves* c);
static OSStatus GetTRCCurves (DisplayIDType id, curves* c);
static OSStatus GetNDINCurves (DisplayIDType id, curves* c);


//---------------------------------------------------------------------------
int main (void)
{
	Initialize();
	RunApplicationEventLoop();
	return 0;	
}


//---------------------------------------------------------------------------
static void Initialize (void)
{
	Handle					menuBar = nil;
	long					result = 0;
	EventTypeSpec			eType;
	EventHandlerUPP			uppMenu = nil;
	EventLoopTimerUPP		uppTimer = nil;
	EventLoopRef			loop;
	
	InitCursor();
	
	// Setup the menu bar
	if ((Gestalt(gestaltSystemVersion, &result)==noErr) && (result >= 0xA00))
		gOnX = true;
	
	if ((Gestalt(gestaltMenuMgrAttr, &result)==noErr) && (result & gestaltMenuMgrAquaLayoutMask))
		menuBar = GetNewMBar(1100);
	else
		menuBar = GetNewMBar(1000);
	SetMenuBar(menuBar);
	DisposeHandle(menuBar);
	DrawMenuBar();
	
	// Install timer
	uppTimer = NewEventLoopTimerUPP(EventLoopTimer);
	loop = GetMainEventLoop();
	(void) InstallEventLoopTimer(loop, 4.0, 2.0, uppTimer, 0L, nil);
	
	// Install menu command handler
	eType.eventClass = kEventClassCommand;
	eType.eventKind = kEventCommandProcess;
	uppMenu = NewEventHandlerUPP(EventCommandProcess);
	(void) InstallApplicationEventHandler(uppMenu, 1, &eType, 0L, nil);
	
	InitializeWindows();
}


//---------------------------------------------------------------------------
static void InitializeWindows (void)
{
	OSErr					err;
	WindowPtr				window;
	Rect					windRect = {0,0,100,100};
	GDHandle				gd;
	Rect					r;
	DisplayIDType			id;
	EventTypeSpec			eType;
	static EventHandlerUPP	uppDraw = nil;
	static short			lastTop=0, lastLeft=0;
	
	// Create a window for each display
	gd = GetDeviceList();
	while (gd)
	{
		windRect = r = (**gd).gdRect;
		windRect.top = (r.top+r.bottom)/2 - 128;
		windRect.left = (r.left+r.right)/2 - 128;
		
		// This will stagger windows if two displays are mirrored
		if (windRect.top==lastTop && windRect.left==lastLeft)
			windRect.top += 20, windRect.left += 20;
		lastTop = windRect.top, lastLeft = windRect.left;
		
		windRect.bottom = windRect.top + 256;
		windRect.right = windRect.left + 256;
		
		err = DMGetDisplayIDByGDevice( gd, &id, false);
		require_noerr(err, next);
		
		err = CreateNewWindow( kDocumentWindowClass,
					kWindowCloseBoxAttribute|kWindowStandardHandlerAttribute,
				  		&windRect, &window);
		require_noerr(err, next);
		
		if (uppDraw==nil)
			uppDraw = NewEventHandlerUPP(EventWindowDrawContent);
		
		eType.eventClass = kEventClassWindow;
		eType.eventKind = kEventWindowDrawContent;
		err = InstallWindowEventHandler(window, uppDraw, 1, &eType, (void*)id, nil);
		require_noerr(err, next);
		
		ChangeWindowAttributes(window,0,kWindowCollapseBoxAttribute);
		ShowWindow(window);
		
	next:
		
		gd = GetNextDevice(gd);
	}
	
	ChangeMode(kLUTsCmd);
}


#pragma mark -


//---------------------------------------------------------------------------
  
static pascal void EventLoopTimer (EventLoopTimerRef timer, void *userData)
{
#pragma unused (timer, userData)
	UpdateWindows();
}


//---------------------------------------------------------------------------
  
static pascal OSStatus EventCommandProcess (EventHandlerCallRef callRef, EventRef event, void *userData)
{
#pragma unused (callRef, userData)
	OSStatus		theErr = noErr;
	HICommand		command = {};
	UInt32			inSize = sizeof(HICommand);
	
	theErr = GetEventParameter(event, kEventParamDirectObject, typeHICommand, 
								nil, inSize, nil, &command);
	require_noerr(theErr, bail);
	
	switch (command.commandID)
	{
		case kQuitCmd:
			QuitApplicationEventLoop();
			break;

		case kLUTsCmd:
		case kTRCsCmd:
		case kvcgtCmd:
		case kndinCmd:
			ChangeMode(command.commandID);
			break;
		
		case kAboutCmd:
		{
			Str255		aboutStr = "\pGammas";
			Str255		cprtStr = "\pThis application displays various tables "
									"associated with each display connected to "
									"the computer and its profile.";
			AlertStdAlertParamRec alertParam = {
					false,					// Make alert movable modal
					false,					// Is there a help button?
					nil,					// ModalFilterUPP filterProc
					(ConstStringPtr)-1, nil, nil,
					1,						// Which button behaves as the default
					2,						// Which one behaves as cancel (can be 0)
					kWindowAlertPositionParentWindow };
			StandardAlert (kAlertPlainAlert, aboutStr, cprtStr, &alertParam, nil);
		}
		break;
		
		default:
			theErr = eventNotHandledErr;
	}
	
bail:
	
	return theErr;
}


//---------------------------------------------------------------------------
static pascal OSStatus EventWindowDrawContent (EventHandlerCallRef callRef, EventRef event, void *userData)
{
#pragma unused (callRef)
	OSStatus		theErr = noErr;
	WindowPtr		theWindow = nil;
	DisplayIDType	id = (DisplayIDType)userData;
	Rect			r;
	curves			c = {};
	
	theErr = GetEventParameter(event, kEventParamDirectObject, typeWindowRef, 
								nil, 4, nil, &theWindow);
	require_noerr(theErr, bail);
	
	GetPortBounds(GetWindowPort(theWindow),&r);
	EraseRect(&r);
	
	theErr = GetCurves(id, &c, gMode);
	if (theErr) return theErr;
	
	DrawCurves(&c);
	
bail:
	
	return theErr;
}


#pragma mark -


//---------------------------------------------------------------------------
static void SetMenuCommandIDCheck(UInt32 commandID, Boolean check)
{
	MenuRef			menu = nil;
	MenuItemIndex	i;
	OSStatus		err;
	
	err = GetIndMenuItemWithCommandID(nil, commandID, 1, &menu, &i);
	if (!err)
		CheckMenuItem(menu,i,check);
}


//---------------------------------------------------------------------------
static StringPtr GetMenuCommandIDText(UInt32 commandID, Str255 string)
{
	MenuRef			menu = nil;
	MenuItemIndex	i;
	
	OSStatus		err;
	
	err = GetIndMenuItemWithCommandID(nil, commandID, 1, &menu, &i);
	if (!err)
	{
		GetMenuItemText(menu,i,string);
		return string;
	}
	return nil;
}


#pragma mark -


//---------------------------------------------------------------------------
static void ChangeMode (OSType newMode)
{
	SetMenuCommandIDCheck(gMode, false);
	gMode = newMode;
	UpdateTitles();
	UpdateWindows();
	SetMenuCommandIDCheck(gMode, true);
}


//---------------------------------------------------------------------------
static void UpdateTitles (void)
{
	WindowRef	w;
	Str255		s;
	
	GetMenuCommandIDText(gMode, s);

	w = GetWindowList();
	while (w)
	{
		SetWTitle(w,s);				
		w = GetNextWindow(w);
	}
}


//---------------------------------------------------------------------------
static void UpdateWindows (void)
{
	WindowRef	w;
	Rect		r = {0,0,256,256};
	
	w = GetWindowList();
	while (w)
	{
		InvalWindowRect(w,&r);
		w = GetNextWindow(w);
	}
}


//---------------------------------------------------------------------------
static void FixedToString (Fixed f, StringPtr s)
{
	short	i;
	short	len;
	
	i = (f*1000)>>16;
	NumToString(i,s);
	len = s[0];
	
	if (len<4)
	{
		s[4] = (len>0) ? s[len-0] : '0';
		s[3] = (len>1) ? s[len-1] : '0';
		s[2] = (len>2) ? s[len-2] : '0';
		s[1] = '0';
		len = s[0] = 4;
	}
	
	s[0] = len+1;
	s[len+1] = s[len+0];
	s[len+0] = s[len-1];
	s[len-1] = s[len-2];
	s[len-2] = '.';
}


//---------------------------------------------------------------------------
static void DrawCurves (curves* c)
{
	int			i,x,y0,y1,y2;
	static int	lx,ly0,ly1,ly2;
	Str63	s;
	
	x = 5;
	y0 = 15;
	
	// Draw the name of the profile if present
	TextSize(9);
	MoveTo(x,y0);
	DrawString(c->str);
	y0+=15;
	
	// draw the simple gamma values if present
	if (c->gam[0] && c->gam[1] && c->gam[2])
	{
		if (c->gam[0]==c->gam[1] && c->gam[1]==c->gam[2])
		{
			FixedToString(c->gam[0], s);
			MoveTo(x,y0);
			DrawString(s);
		}
		else
		{
			ForeColor(redColor);
			FixedToString(c->gam[0], s);
			MoveTo(x,y0);
			DrawString(s);
			y0+=15;
			ForeColor(greenColor);
			FixedToString(c->gam[1], s);
			MoveTo(x,y0);
			DrawString(s);
			y0+=15;
			ForeColor(blueColor);
			FixedToString(c->gam[2], s);
			MoveTo(x,y0);
			DrawString(s);
			ForeColor(blackColor);
		}
	}
	
	lx = 0; ly0=ly1=ly2=255;
	
	// draw the curves
	for (i=0; i<c->tableCount; i++)
	{
		x = i * 255 / (c->tableCount-1);
		
		y0 = 255 - c->table[0][i];
		y1 = 255 - c->table[1][i];
		y2 = 255 - c->table[2][i];
		
		if (y0==y1 && y1==y2)
			MoveTo(lx,ly0), LineTo(x, y0);
		else if (y0==y1)
			MoveTo(lx,ly0),LineTo(x, y0), ForeColor(blueColor),  MoveTo(lx,ly2),LineTo(x,y2);
		else if (y1==y2)
			MoveTo(lx,ly1),LineTo(x, y1), ForeColor(redColor),   MoveTo(lx,ly0),LineTo(x,y0);
		else if (y0==y2)
			MoveTo(lx,ly0),LineTo(x, y0), ForeColor(greenColor), MoveTo(lx,ly1),LineTo(x,y1);
		else
		{	
			ForeColor(redColor),   MoveTo(lx,ly0),LineTo(x,y0);
			ForeColor(greenColor), MoveTo(lx,ly1),LineTo(x,y1);
			ForeColor(blueColor),  MoveTo(lx,ly2),LineTo(x,y2);
		}
		ForeColor(blackColor);
		
		lx = x; ly0=y0;  ly1=y1;  ly2=y2;
	}
}


//---------------------------------------------------------------------------
static void FillCurveTable (curves* c, int t)
{
	int i;
	double v,m,g,d;
	
	m = ((double)c->max[t]) / 65536.0;
	g = ((double)c->gam[t]) / 65536.0;
	c->tableCount = 256;

	for (i=0; i<256; i++)
	{
		v = ((double)i) / 255.0;
		d = pow(v,g) * m;
		c->table[t][i] = (d * 255.0 + 0.5);
	}	
}


//---------------------------------------------------------------------------
static void FillCurveTables (curves* c)
{
	int t;
	for (t=0; t<3; t++)
		FillCurveTable(c, t);
}

#pragma mark -

//---------------------------------------------------------------------------
static OSStatus GetCurves (DisplayIDType id, curves* c, OSType mode)
{
	OSStatus		theErr;
	
	if (gOnX)
	{
		CFStringRef  domain = CFSTR("com.apple.ColorSyncDeviceList");
		CFPreferencesSynchronize(domain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
		CFPreferencesSynchronize(domain, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	}
	
	switch (mode)
	{
		case kLUTsCmd:
		theErr = GetLutCurves(id, c);
		break;

		case kTRCsCmd:
		theErr = GetTRCCurves(id, c);
		break;

		case kvcgtCmd:
		theErr = GetVCGTCurves(id, c);
		break;

		case kndinCmd:
		theErr = GetNDINCurves(id, c);
		break;
		
		default:
		theErr = paramErr;
		break;
	}
	
	if (c->tableCount == 0)
		FillCurveTables(c);
	
	return theErr;
}


//---------------------------------------------------------------------------
static OSStatus GetVCGTCurvesCommon (CMVideoCardGamma* gamma, curves* c)
{
	if (gamma->tagType == cmVideoCardGammaFormulaType)
	{
 		c->max[0] = gamma->u.formula.redMax;
 		c->gam[0] = gamma->u.formula.redGamma;
 		c->max[1] = gamma->u.formula.greenMax;
 		c->gam[1] = gamma->u.formula.greenGamma;
 		c->max[2] = gamma->u.formula.blueMax;
 		c->gam[2] = gamma->u.formula.blueGamma;
	}
	else if (gamma->tagType == cmVideoCardGammaTableType)
	{
		UInt8*	data;
		int		i,t;
		UInt16	channels, entryCount, entrySize;
		long	chanbytes;
		
		channels = gamma->u.table.channels;
		entryCount = gamma->u.table.entryCount;
		entrySize = gamma->u.table.entrySize;
		chanbytes = entryCount * entrySize * (channels==1 ? 0 : 1);
		data = (UInt8*)gamma->u.table.data;
		
		c->tableCount = entryCount;
		if (entryCount<=256)
			for (t=0; t<3; t++)
				for (i=0; i<entryCount; i++)
					c->table[t][i] = data[chanbytes*t + entrySize*i];
	}
	
	return noErr;
}


//---------------------------------------------------------------------------
static OSStatus GetLutCurves (DisplayIDType id, curves* c)
{
	OSStatus		theErr;
	UInt32			size;
	CMVideoCardGamma* gamma = nil;

	theErr = CMGetGammaByAVID(id, nil, &size);
	require_noerr(theErr, bail);
	
	c->str[0] = 0;
	
	gamma = (CMVideoCardGamma*) NewPtrClear(size);
	require(gamma, bail);
	
	theErr = CMGetGammaByAVID(id, gamma, &size);
	require_noerr(theErr, bail);

	theErr = GetVCGTCurvesCommon(gamma, c);
	require_noerr(theErr, bail);
	
bail:
	
	if (gamma) DisposePtr((Ptr)gamma);
	
	return theErr;
}


//---------------------------------------------------------------------------
static OSStatus GetVCGTCurves (DisplayIDType id, curves* c)
{
	OSStatus		theErr;
	CMProfileRef	prof = nil;
	UInt32			size;
	CMVideoCardGammaType* gamma = nil;
	ScriptCode		code;
	
	theErr = CMGetProfileByAVID(id, &prof);
	require_noerr(theErr, bail);
	
	(void) CMGetScriptProfileDescription(prof,c->str, &code);
	
	theErr = CMGetProfileElement(prof,cmVideoCardGammaTag, &size, nil);
 	require_noerr(theErr, bail);
	
	gamma = (CMVideoCardGammaType*) NewPtrClear(size);
	require(gamma, bail);
	
	theErr = CMGetProfileElement(prof,cmVideoCardGammaTag, &size, gamma);
 	require_noerr(theErr, bail);
 	require(gamma->typeDescriptor == cmSigVideoCardGammaType, bail);
 	
	theErr = GetVCGTCurvesCommon(&gamma->gamma, c);
	require_noerr(theErr, bail);
	
bail:
	
	if (prof) CMCloseProfile(prof);
	if (gamma) DisposePtr((Ptr)gamma);
	
	return theErr;
}


//---------------------------------------------------------------------------
static OSStatus GetTRCCurves (DisplayIDType id, curves* c)
{
	OSStatus		theErr;
	CMProfileRef	prof = nil;
	CMCurveType*	curv = nil;
	UInt32			curvSize;
	int				t,i;
	OSType			tags[3] = {cmRedTRCTag, cmGreenTRCTag, cmBlueTRCTag};
	ScriptCode		code;
	
	theErr = CMGetProfileByAVID(id, &prof);
	require_noerr(theErr, bail);
	
	(void) CMGetScriptProfileDescription(prof,c->str, &code);
	
	for (t=0; t<3; t++)
	{
		theErr = CMGetProfileElement(prof,tags[t], &curvSize, nil);
	 	require_noerr(theErr, bail);
	 
		if (curv) DisposePtr((Ptr)curv);
		curv = (CMCurveType*) NewPtrClear(curvSize);
		require(curv, bail);
		
		theErr = CMGetProfileElement(prof,tags[t], &curvSize, curv);
	 	require_noerr(theErr, bail);
	 	require(curv->typeDescriptor == cmSigCurveType, bail);
	 	
	 	if (curv->countValue == 0)
	 	{
	 		c->max[t] = 0x00010000;
	 		c->gam[t] = 0x00010000;
	 	}
	 	else if (curv->countValue == 1)
	 	{
	 		c->max[t] = 0x00010000;
	 		c->gam[t] = ((long)(curv->data[0]))<<8;
	 	}
	 	else
	 	{
	 		c->max[t] = 0x00010000;
	 		c->gam[t] = 0x00010000;
	 		for (i=0; i<256; i++)
	 			c->table[t][i] = i;
	 	}
 	}
	
bail:
	
	if (prof) CMCloseProfile(prof);
	if (curv) DisposePtr((Ptr)curv);
	
	return theErr;
}


//---------------------------------------------------------------------------
static OSStatus GetNDINCurves (DisplayIDType id, curves* c)
{
	OSStatus		theErr;
	CMProfileRef	prof = nil;
	UInt32			size;
	UInt32* 		ndin = nil;
	ScriptCode		code;
	
	theErr = CMGetProfileByAVID(id, &prof);
	require_noerr(theErr, bail);
	
	(void) CMGetScriptProfileDescription(prof,c->str, &code);
	
	theErr = CMGetProfileElement(prof,'ndin', &size, nil);
 	require_noerr(theErr, bail);
 
	ndin = (UInt32*) NewPtrClear(size);
	require(ndin, bail);
	
	theErr = CMGetProfileElement(prof,'ndin', &size, ndin);
 	require_noerr(theErr, bail);
 	require(ndin[0] == 'ndin', bail);
 	
	c->max[0] = 0x00010000;
	c->max[1] = 0x00010000;
	c->max[2] = 0x00010000;
	c->gam[0] = ndin[11];
	c->gam[1] = ndin[12];
	c->gam[2] = ndin[13];
	
	if (ndin[2]>48 && ndin[14])
	{
		UInt16*	ptr16;
		UInt8*	data;
		int		i,t;
		UInt16	channels, entryCount, entrySize;
		long	chanbytes;
		
		ptr16 = (UInt16*)(&ndin[14]);
		channels = ptr16[0];
		entryCount = ptr16[1];
		entrySize = ptr16[2];
		chanbytes = entryCount * entrySize * (channels==1 ? 0 : 1);
		data = (UInt8*)&ptr16[3];
		
		c->tableCount = entryCount;
		if (entryCount<=256)
			for (t=0; t<3; t++)
				for (i=0; i<entryCount; i++)
					c->table[t][i] = *(data + (chanbytes*t) + (entrySize*i));
	}
	
bail:
	
	if (prof) CMCloseProfile(prof);
	if (ndin) DisposePtr((Ptr)ndin);
	
	return theErr;
}
