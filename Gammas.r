
#ifdef mw_rez
	#include "Types.r"
	#include "MacTypes.r"
	#include "Menus.r"
#else
	#include <Carbon/Carbon.r>
#endif


data 'carb' (0) {
	$"00"
};

resource 'MBAR' (1000) {
	{1000, 1100, 1200}
};

resource 'MBAR' (1100) {
	{1000, 1200}
};

resource 'MENU' (1000)
{
	1000,
	0,
	0x7FFFFFFD,
	enabled,
	apple, {
		"About Gammas...", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain
	}
};

resource 'xmnu' (1000, purgeable) {
	versionZero {
		{
			dataItem { 'abou', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph }
		}
	}
};

resource 'MENU' (1100) {
	1100,
	0,
	0x7FFFFFFF,
	enabled,
	"File", {
		"Quit", noIcon, "Q", noMark, plain
	}
};

resource 'xmnu' (1100, purgeable) {
	versionZero {
		{
			dataItem { 'quit', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph }
		}
	}
};


resource 'MENU' (1200) {
	1200,
	0,
	0x7FFFFFFF,
	enabled,
	"View", {
		"Video card LUTs", noIcon, "1", noMark, plain,
		"Profile TRCs",    noIcon, "2", noMark, plain,
		"Profile vcgt",    noIcon, "3", noMark, plain,
		"Profile ndin",    noIcon, "4", noMark, plain,
	}
};

resource 'xmnu' (1200, purgeable) {
	versionZero {
		{
			dataItem { 'LUTs', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph },
			dataItem { 'TRCs', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph },
			dataItem { 'vcgt', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph },
			dataItem { 'ndin', 0x0, currScript, 0, 0, noHierID, sysFont, naturalGlyph },
		}
	}
};

