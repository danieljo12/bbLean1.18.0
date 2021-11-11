#pragma once

/*
 ============================================================================
  This file is part of the bbLeanBar+ source code.

  bbLeanBar+ is a plugin for BlackBox for Windows
  Copyright © 2003-2009 grischka
  Copyright © 2006-2009 The Blackbox for Windows Development Team

  http://bb4win.sourceforge.net/bblean/

  bbLeanBar+ is free software, released under the GNU General Public License
  (GPL version 2). See for details:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.

 ============================================================================
*/
#include "BlackboxBar.h"
#include <BBApi.h>
#include <bblib.h>
#include <tooltips.h>
//#include <DrawText.h>

bool ShowSysmenu(HWND TaskWnd, HWND BarWnd, RECT* pRect, const char* plugin_broam);
bool exec_sysmenu_command(const char* temp, bool sendToSwitchTo);
bool sysmenu_exists();

extern "C" API_EXPORT void bbDrawPix(HDC hDC, RECT * p_rect, COLORREF picColor, int style);

#ifndef NO_DROP
class TinyDropTarget* init_drop_targ(HWND hwnd);
void exit_drop_targ(TinyDropTarget* tinyDropTarget);
#endif


// possible bar items
enum BARITEMS {
	M_EOS = 0,
	M_NEWLINE,

	M_TASK,
	M_TRAY,

	M_WSPL,
	M_CLCK,
	M_WINL,
	M_SPAC,
	M_CUOB,     // CurrentOnlyButton
	M_TDPB,     // TaskDisplayButton
	M_WSPB_L,
	M_WSPB_R,

	M_WINB_L,
	M_WINB_R,

	// list classes
	M_BARLIST = 256,
	M_TASKLIST,
	M_TRAYLIST
};




// the base class for all items on the bar:

class baritem {
public:
	struct barinfo* m_bar;     // the pointer to the plugin_info structure
	RECT itemRect;            // the rectangle of the item
	int m_type;         // the item's M_XXX ID
	bool m_active;      // pressed state
	int m_margin;       //
	bool mouse_in;

	//-----------------------------
	baritem(int type, barinfo* bi);

	//-----------------------------
	virtual ~baritem();

	//-----------------------------
	// asign the rectangle, advance the x-pointer, returns true if changed
	bool set_location(int* px, int y, int w, int h, int m);

	//-----------------------------
	// check the item for mouse-over
	int mouse_over(int mx, int my);

	bool menuclick(int message, unsigned flags);

	//-----------------------------
	virtual void mouse_event(int mx, int my, int message, unsigned flags);

	//-----------------------------
	virtual void draw();

	//-----------------------------
	virtual void settip(void);

	//-----------------------------
	// calculate metrics, return true on changes
	virtual bool calc_sizes(void);

	//-----------------------------
	virtual void invalidate(int flag);

	//-----------------------------
	void release_capture(void);

	//-----------------------------
	bool check_capture(int mx, int my, int message);

};

struct itemlist {
	struct itemlist* next;
	struct baritem* item;
};


// a list class, for tasks and tray-icons, also for the entire bar

class baritemlist : public baritem {
public:
	struct itemlist* items;

	//-----------------------------
	baritemlist(int type, barinfo* bi);

	//-----------------------------
	virtual ~baritemlist();

	// adds a bar item to the list.
	void add(class baritem* entry);

	// clears the list.
	void clear(void);

	// calls the draw method of all bar items.
	void draw(); // NEVERUSED override

	// checks every bar item to see if it was the one which the user clicked on.
	virtual void mouse_event(int mx, int my, int message, unsigned flags); // NEVERUSED override

	//-----------------------------
	void settip(); // NEVERUSED override

	//-----------------------------
	void invalidate(int flag); // NEVERUSED override
};




// one task entry

class taskentry : public baritem {
public:
	int m_index;
	bool m_showtip;
	bool m_dblclk;
	bool a1, a2;
	int press_x;

	//-----------------------------
	taskentry(int index, barinfo* bi);

	//-----------------------------
	~taskentry();

	//-----------------------------
	void draw(); // NEVERUSED override

	//-----------------------------
	// Icon only mode
	void draw_icons(struct tasklist* tl, bool lit, StyleItem* pSI);

	//-----------------------------
	// Text (with icon) mode
	void draw_text(struct tasklist* tl, bool lit, StyleItem* pSI);


	void settip(); // NEVERUSED override


	//-----------------------------

	void mouse_event(int mx, int my, int message, unsigned flags); //NEVERUSED override
};




// one tray-icon

class trayentry : public baritem {
public:
	int m_index;

	//-----------------------------
	trayentry(int index, barinfo* bi);

	//-----------------------------
	~trayentry();

	//-----------------------------
	void invalidate(int flag); // NEVERUSED override

	//-----------------------------
	void draw(); // NEVERUSED override


	void settip(); // NEVERUSED override


	//-----------------------------
	void mouse_event(int mx, int my, int message, unsigned flags); // NEVERUSED override
};




// common base class for clock, workspace-label, window-label

class barlabel : public baritem {
public:
	int m_Style;
	WCHAR const* m_text;

	//-----------------------------
	barlabel(int type, barinfo* bi, WCHAR const* text, int s);

	//-----------------------------
	void draw(); // NEVERUSED override
};



// workspace-label

class workspace_label : public barlabel {
public:
	workspace_label(barinfo* bi);

	//-----------------------------
	void mouse_event(int mx, int my, int message, unsigned flags); // NEVERUSED override

};



// window-label

class window_label : public barlabel {
public:
	window_label(barinfo* bi);
};



// clock-label

class clock_displ : public barlabel {
public:
	clock_displ(barinfo* bi);

	void settip(); // NEVERUSED override
	//-----------------------------
	void mouse_event(int mx, int my, int message, unsigned flags); // NEVERUSED override
};




// fill in a space or new line

class spacer : public baritem {
public:
	spacer(int typ, barinfo* bi);
};




// buttons

class bar_button : public baritem {
public:
	int dir;

	//-----------------------------
	bar_button(int m, barinfo* bi);

	//-----------------------------
	void draw(); // NEVERUSED override

	//-----------------------------
	// for the buttons, the mouse is captured on button-down
	void mouse_event(int mx, int my, int message, unsigned flags); // NEVERUSED override

	void settip(); // NEVERUSED override

};




// task zone

class taskitemlist : public baritemlist {
	int len;

public:
	taskitemlist(barinfo* bi);

	//-----------------------------
	// This one assigns the individual locations and sizes for
	// the items in the task-list

	bool calc_sizes(void); // NEVERUSED override

	//-----------------------------
	void mouse_event(int mx, int my, int message, unsigned flags);

	void invalidate(int flag);

	/*
		void draw();
	*/
};




// tray zone

class trayitemlist : public baritemlist {
	int len;

public:
	trayitemlist(barinfo* bi);

	//-----------------------------
	// This one assigns the individual locations and sizes for
	// the items in the tray-icon-list
	bool calc_sizes(void);

	void invalidate(int flag);

	void mouse_event(int mx, int my, int message, unsigned flags);

	/*
		void draw();
	*/
};




// LeanBar - the main class

class LeanBar : public baritemlist {
public:
	int max_label_width;
	int max_clock_width;
	int trayzone_width;

	//-----------------------------
	LeanBar(barinfo* bi);


	void settip();


	//-----------------------------
	void invalidate(int flag);

	//-----------------------------
	// check for capture, otherwise dispatch the mouse event
	void mouse_event(int mx, int my, int message, unsigned flags);

	//-----------------------------
	// build everything from scratch
	void create_bar();

	//-----------------------------
	bool calc_sizes(void);

	//-----------------------------
	// Here sizes are calculated in two passes: The first pass
	// gets all fixed sizes. Then the remaining space is assigned
	// to the variable ones (windowlabel/taskzone). The second pass
	// assigns the actual x-coords.

	bool calc_line_size(struct itemlist** p0, int top, int bottom, int height);
};

