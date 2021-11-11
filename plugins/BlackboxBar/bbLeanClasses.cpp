/*
 ============================================================================
  This file is part of the bbLeanBar+ source code.

  bbLeanBar+ is a plugin for BlackBox for Windows
  Copyright � 2003-2009 grischka
  Copyright � 2006-2009 The Blackbox for Windows Development Team

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

#include "bbLeanClasses.h"


// the base class for all items on the bar:

	//-----------------------------
baritem::baritem(int type, barinfo* bi)
{
	m_bar = bi;
	m_type = type;
	m_active = false;
	m_margin = 0;
	itemRect.left = itemRect.right = 0;
	mouse_in = false;
}

//-----------------------------
baritem::~baritem()
{
	release_capture();
}

//-----------------------------
// asign the rectangle, advance the x-pointer, returns true if changed

bool baritem::set_location(int* px, int y, int w, int h, int m)
{
	int x = *px;
	bool f = false;

	if (itemRect.left != x)
		itemRect.left = x, f = true;
	x += w;
	if (itemRect.right != x)
		itemRect.right = x, f = true;
	*px = x;

	itemRect.bottom = (itemRect.top = y) + h;
	m_margin = m;
	return f;
}

//-----------------------------

int baritem::mouse_over(int mx, int my)
{
	RECT r = itemRect;
#if 1
	// extend clickable area to screen edge
	int border = styleBorderWidth + styleBevelWidth;
	if (r.top - m_bar->mon_top <= border)
		r.top = m_bar->mon_top;

	if (m_bar->mon_bottom - r.bottom <= border)
		r.bottom = m_bar->mon_bottom;
#endif
	if (my < r.top || my >= r.bottom)
		return 0;
	if (mx < r.left || mx > r.right)
		return -1;
	return 1;
}

bool baritem::menuclick(int message, unsigned flags)
{
	if (flags & MK_CONTROL)
	{
		if (WM_RBUTTONDOWN == message)
			return true;
		if (WM_RBUTTONDBLCLK == message)
			return true;
		if (WM_RBUTTONUP == message)
		{
			m_bar->show_menu(true);
			return true;
		}
	}
	return false;
}

//-----------------------------
void baritem::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (menuclick(message, flags))
		return;
	// default: show bb-menu
	if (WM_RBUTTONUP == message)
		PostMessage(BBhwnd, BB_MENU, 0, 0);
}

//-----------------------------
void baritem::draw()
{
}

//-----------------------------
void baritem::settip(void)
{
}

//-----------------------------
// calculate metrics, return true on changes
bool baritem::calc_sizes(void)
{
	return false;
};

//-----------------------------
void baritem::invalidate(int flag)
{
	InvalidateRect(m_bar->hwnd, &itemRect, FALSE);
}

//-----------------------------
void baritem::release_capture(void)
{
	if (m_bar->capture_item == this)
	{
		ReleaseCapture();
		m_bar->capture_item = NULL;
	}
}

//-----------------------------
bool baritem::check_capture(int mx, int my, int message)
{
	bool pa = m_active;
	bool ret = true;
	if (m_bar->capture_item == this)
	{
		int over = mouse_over(mx, my);
		m_active = ret = over > 0;
		if (over < 0 && m_type == M_TASK && m_bar->gesture_lock)
			ret = true;

		if (message != WM_MOUSEMOVE)
		{
			release_capture();
			m_active = false;
		}
	}
	else
		if (message == WM_LBUTTONDOWN
			|| message == WM_RBUTTONDOWN
			|| message == WM_LBUTTONDBLCLK
			|| message == WM_RBUTTONDBLCLK
			)
		{
			SetCapture(m_bar->hwnd);
			m_bar->capture_item = this;
			m_active = true;
		}

	if (m_active != pa)
		invalidate(0);

	return ret;
}





// a list class, for tasks and tray-icons, also for the entire bar

	//-----------------------------
baritemlist::baritemlist(int type, barinfo* bi) : baritem(type, bi)
{
	items = NULL;
}

//-----------------------------
baritemlist::~baritemlist()
{
	clear();
}

//-----------------------------
void baritemlist::add(class baritem* entry)
{
	append_node(&items, new_node(entry));
}

//-----------------------------
void baritemlist::clear(void)
{
	struct itemlist* i;
	dolist(i, items) delete i->item;
	freeall(&items);
}

//-----------------------------
void baritemlist::draw()
{
	struct itemlist* p; RECT rtmp;
	dolist(p, items)
		if (IntersectRect(&rtmp, &p->item->itemRect, m_bar->p_rcPaint))
			p->item->draw();
}

//-----------------------------
void baritemlist::mouse_event(int mx, int my, int message, unsigned flags)
{
	struct itemlist* p, * q;

	dolist(p, items)
	{
		if (p->item->mouse_over(mx, my) > 0)
			break;


	}
	dolist(q, items)
		if (q->item->mouse_in)
			break;

	if (q && q != p)
	{
		q->item->mouse_event(mx, my, WM_MOUSELEAVE, flags);
		q->item->mouse_in = false;
	}

	if (p)
	{
		p->item->mouse_event(mx, my, message, flags);
		p->item->mouse_in = true;
	}
	else
	{
		menuclick(message, flags);
	}

}

//-----------------------------
void baritemlist::settip()
{
	struct itemlist* p;
	dolist(p, items) p->item->settip();
}

//-----------------------------
void baritemlist::invalidate(int flag)
{
	struct itemlist* p;
	dolist(p, this->items)
		if (flag == p->item->m_type)
			p->item->invalidate(0);
}





// one task entry

taskentry::taskentry(int index, barinfo* bi) : baritem(M_TASK, bi)
{
	m_index = index;
	m_dblclk = false;
	m_showtip = false;
	a1 = a2 = false;
}

//-----------------------------
taskentry::~taskentry()
{
}

//-----------------------------
void taskentry::draw()
{
	m_showtip = false;

	struct tasklist* tl = m_bar->GetTaskPtrEx(m_index);
	if (NULL == tl) return;

	bool lit = tl->active
		|| tl->flashing
		|| tl->hwnd == m_bar->task_lock
		|| m_active
		;

	a1 = tl->active;
	StyleItem* pSI;

	if (lit) pSI = &ATaskStyle;
	else pSI = &NTaskStyle;

	if (m_bar->TaskStyle == 1)
		draw_icons(tl, lit, pSI);
	else
		draw_text(tl, lit, pSI);

}

//-----------------------------
// Icon only mode
void taskentry::draw_icons(struct tasklist* tl, bool lit, StyleItem* pSI)
{
	if (lit)
	{
		bool bordered = pSI->bordered || pSI->parentRelative;
		m_bar->pBuff->MakeStyleGradient(m_bar->hdcPaint, &itemRect, pSI, bordered);
	}

	HICON icon = tl->icon;
	if (NULL == icon)
		icon = LoadIcon(NULL, IDI_APPLICATION);

	int o = (itemRect.bottom - itemRect.top - m_bar->TASK_ICON_SIZE) / 2;

	DrawIconSatnHue(m_bar->hdcPaint, itemRect.left + o, itemRect.top + o,
		icon, m_bar->TASK_ICON_SIZE, m_bar->TASK_ICON_SIZE,
		0, NULL, DI_NORMAL,
		false == lit, m_bar->saturation, m_bar->hue);

	m_showtip = true;
}

//-----------------------------
// Text (with icon) mode
void taskentry::draw_text(struct tasklist* tl, bool lit, StyleItem* pSI)
{
	bool bordered;
	if (m_bar->task_with_border || (lit && pSI->parentRelative))
		bordered = true;
	else
		if (false == m_bar->task_with_border && false == lit)
			bordered = false;
		else
			bordered = pSI->bordered;

	m_bar->pBuff->MakeStyleGradient(m_bar->hdcPaint, &itemRect, pSI, bordered);

	HGDIOBJ oldfont = SelectObject(m_bar->hdcPaint, m_bar->hFont);
	SetBkMode(m_bar->hdcPaint, TRANSPARENT);

	RECT ThisWin = itemRect;
	RECT s1 = { 0,0,0,0 };
	RECT s2 = { 0,0,0,0 };
	WCHAR buf[8];
	wcsncpy(buf, tl->wcaption, 8);
	buf[7] = L'\0';
	BBDrawTextAltW(m_bar->hdcPaint, buf, -1, &s1, DT_CALCRECT | DT_NOPREFIX, pSI);
	BBDrawTextAltW(m_bar->hdcPaint, tl->wcaption, -1, &s2, DT_CALCRECT | DT_NOPREFIX, pSI);

	int o, f, i;
	o = f = 0;
	if ((m_bar->TaskStyle & 2) && NULL != tl->icon)
	{
		o = (itemRect.bottom - itemRect.top - m_bar->TASK_ICON_SIZE) / 2;
		f = m_bar->TASK_ICON_SIZE + o - m_bar->labelBorder;
	}

	i = m_bar->labelIndent;
	ThisWin.left += i + f;
	ThisWin.right -= i;
	int s = ThisWin.right - ThisWin.left;

	BBDrawTextAltW(m_bar->hdcPaint, tl->wcaption, -1, &ThisWin,
		(s > s1.right ? TBJustify : DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_NOPREFIX),
		pSI
	);

	SelectObject(m_bar->hdcPaint, oldfont);

	if (s < s2.right)
		m_showtip = true;

	if (f)
	{
		DrawIconSatnHue(m_bar->hdcPaint, itemRect.left + o, itemRect.top + o,
			tl->icon, m_bar->TASK_ICON_SIZE, m_bar->TASK_ICON_SIZE,
			0, NULL, DI_NORMAL,
			false == lit, m_bar->saturation, m_bar->hue);
	}
}

void taskentry::settip()
{
	if (m_showtip)
	{
		struct tasklist* tl = m_bar->GetTaskPtrEx(m_index);
		if (tl)
			SetToolTipW(m_bar->hwnd, &itemRect, tl->wcaption);
	}
}


//-----------------------------

void taskentry::mouse_event(int mx, int my, int message, unsigned flags)
{
	HWND Window;
	struct tasklist* tl;
	bool shift_down, iconic;
	int gesture, d, w;

	if (menuclick(message, flags))
		return;

	if (false == check_capture(mx, my, message))
		return;

	tl = m_bar->GetTaskPtrEx(m_index);

	if (NULL == tl)
		return;

	Window = tl->hwnd;
	shift_down = 0 != (flags & MK_SHIFT);

	switch (message)
	{
		//====================
		// Restore and focus window
		case WM_LBUTTONUP:
			gesture = 0;
			d = mx - press_x;
			w = (m_bar->mon_rect.right - m_bar->mon_rect.left) / 30;
			if (d < -w)
				gesture = -1;
			if (d > w)
				gesture = 1;

			iconic = FALSE != IsIconic(Window);
			if (m_bar->taskSysmenu
				&& false == iconic
				&& false == gesture
				&& false == sysmenu_exists()
				&& tl->wkspc == currentScreen
				&& a2
				&& (WS_MINIMIZEBOX & GetWindowLongPtr(Window, GWL_STYLE)))
			{
				PostMessage(BBhwnd, BB_WINDOWMINIMIZE, 0, (LPARAM)Window);
				break;
			}

			if (gesture && m_bar->gesture_lock)
			{
				if (tl->wkspc != currentScreen)
				{
					PostMessage(BBhwnd, BB_BRINGTOFRONT, BBBTF_CURRENT, (LPARAM)Window);
				}
				else
				{
					if (gesture < 0)
						PostMessage(BBhwnd, BB_WORKSPACE, BBWS_MOVEWINDOWLEFT, (LPARAM)Window);
					else
						PostMessage(BBhwnd, BB_WORKSPACE, BBWS_MOVEWINDOWRIGHT, (LPARAM)Window);
					PostMessage(BBhwnd, BB_BRINGTOFRONT, BBBTF_CURRENT, (LPARAM)Window);
				}
			}
			else if (shift_down)
				PostMessage(BBhwnd, BB_BRINGTOFRONT, BBBTF_CURRENT, (LPARAM)Window);
			else
				PostMessage(BBhwnd, BB_BRINGTOFRONT, 0, (LPARAM)Window);

			// Avoid flicker between when the mouse is released and
			// the window becomes active
			m_bar->task_lock = Window;
			SetTimer(m_bar->hwnd, TASKLOCK_TIMER, 400, NULL);
			break;

			//====================

		case WM_RBUTTONUP:
			if (shift_down)
			{
				PostMessage(BBhwnd, BB_WINDOWCLOSE, 0, (LPARAM)Window);
				break;
			}

			if (m_bar->taskSysmenu)
			{
				RECT r = itemRect;
				int b = m_margin - NTaskStyle.borderWidth;
				r.top -= b;
				r.bottom += b;
				ClientToScreen(m_bar->hwnd, (POINT*)&r.left);
				ClientToScreen(m_bar->hwnd, (POINT*)&r.right);
				ShowSysmenu(Window, m_bar->hwnd, &r, MY_BROAM);
				break;
			}

			PostMessage(BBhwnd, BB_WINDOWMINIMIZE, 0, (LPARAM)Window);
			//PostMessage(Window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
			break;

			//====================
			// Move window to the next/previous workspace

		case WM_MBUTTONUP:
			if (tl->wkspc != currentScreen)
				PostMessage(BBhwnd, BB_BRINGTOFRONT, BBBTF_CURRENT, (LPARAM)Window);
			else
				if (shift_down)
					PostMessage(BBhwnd, BB_WORKSPACE, BBWS_MOVEWINDOWLEFT, (LPARAM)Window);
				else
					PostMessage(BBhwnd, BB_WORKSPACE, BBWS_MOVEWINDOWRIGHT, (LPARAM)Window);
			break;

			//====================

		case WM_LBUTTONDBLCLK:
			a2 = a1;
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
			m_dblclk = m_active;
			break;

		case WM_LBUTTONDOWN:
			press_x = mx;
			m_bar->gesture_lock = true;
			SetTimer(m_bar->hwnd, GESTURE_TIMER, 800, NULL);

			a2 = a1;
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			m_dblclk = false;
			break;

			//====================
		case BB_DRAGOVER:
			m_bar->task_over_hwnd = Window;
			break;

			//====================
	}
}




// one tray-icon

trayentry::trayentry(int index, barinfo* bi) : baritem(M_TRAY, bi)
{
	m_index = index;
}

//-----------------------------
trayentry::~trayentry()
{
}

//-----------------------------
void trayentry::invalidate(int flag)
{
	baritem::invalidate(0);
}

//-----------------------------
void trayentry::draw()
{
	systemTray* icon = m_bar->GetTrayIconEx(m_index);
	if (icon)
	{
		DrawIconSatnHue(m_bar->hdcPaint, itemRect.left + 1, itemRect.top + 1,
			icon->hIcon, m_bar->TRAY_ICON_SIZE, m_bar->TRAY_ICON_SIZE,
			0, NULL, DI_NORMAL,
			false == mouse_in, m_bar->saturation, m_bar->hue);
	}
}

void trayentry::settip()
{
	systemTray* icon = m_bar->GetTrayIconEx(m_index);
	if (icon)
	{
		SetToolTip(m_bar->hwnd, &itemRect, icon->szTip);
		make_bb_balloon(m_bar, icon, &itemRect);
	}
}


//-----------------------------
void trayentry::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (MK_SHIFT & flags)
	{
		if (WM_RBUTTONDOWN == message)
			return;
		if (WM_RBUTTONDBLCLK == message)
			return;
		if (WM_RBUTTONUP == message)
		{
			m_bar->trayShowIcon(m_bar->RealTrayIndex(m_index), -1);
			return;
		}
	}

	if ((WM_MOUSEMOVE == message && false == mouse_in)
		|| WM_MOUSELEAVE == message
		)
		InvalidateRect(m_bar->hwnd, &itemRect, FALSE);

	systemTrayIconPos pos;
	pos.hwnd = m_bar->hwnd;
	pos.r = itemRect;
	ForwardTrayMessage(m_bar->RealTrayIndex(m_index), message, &pos);
}





// common base class for clock, workspace-label, window-label

barlabel::barlabel(int type, barinfo* bi, WCHAR const* text, int s)
	: baritem(type, bi)
	, m_Style(s)
	, m_text(text)
{
}

//-----------------------------
void barlabel::draw()
{
	StyleItem* pSI = (StyleItem*)GetSettingPtr(m_Style);
	pSI->TextColor = m_bar->alphaEnabled ? (pSI->TextColor < 0x101010 ? 0x444444 : pSI->TextColor) : pSI->TextColor;
	m_bar->pBuff->MakeStyleGradient(m_bar->hdcPaint, &itemRect, pSI, pSI->bordered);
	SetBkMode(m_bar->hdcPaint, TRANSPARENT);
	HGDIOBJ oldfont = SelectObject(m_bar->hdcPaint, m_bar->hFont);
	RECT r;
	int i = m_bar->labelIndent;
	r.left = itemRect.left + i;
	r.right = itemRect.right - i;
	r.top = itemRect.top;
	r.bottom = itemRect.bottom;
	BBDrawTextAltW(m_bar->hdcPaint, m_text, -1, &r, TBJustify, pSI);
	SelectObject(m_bar->hdcPaint, oldfont);

}





// workspace-label


workspace_label::workspace_label(barinfo* bi)
	: barlabel(M_WSPL, bi, screenName, SN_TOOLBARLABEL)
{
}

//-----------------------------
void workspace_label::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (menuclick(message, flags))
		return;
	if (message == WM_LBUTTONUP)
		PostMessage(BBhwnd, BB_WORKSPACE, BBWS_DESKRIGHT, 0);
	else
		if (message == WM_RBUTTONUP)
			PostMessage(BBhwnd, BB_WORKSPACE, BBWS_DESKLEFT, 0);
		else
			if (message == WM_MBUTTONUP)
				PostMessage(BBhwnd, BB_MENU, 1, 0);
}





// window-label


window_label::window_label(barinfo* bi)
	: barlabel(M_WINL, bi, bi->windowlabel, SN_TOOLBARWINDOWLABEL)
{
}





// clock-label

clock_displ::clock_displ(barinfo* bi)
	: barlabel(M_CLCK, bi, bi->clockTime, SN_TOOLBARCLOCK)
{
	m_bar->set_clock_string();
}

void clock_displ::settip()
{
	if (m_bar->clockTips)
	{
		SetToolTipW(m_bar->hwnd, &itemRect, m_bar->clockTimeTips);
	}
}
//-----------------------------
void clock_displ::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (menuclick(message, flags))
		return;

	int n;
	char* cmd;
	switch (message)
	{
		case WM_LBUTTONDBLCLK:
			SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)"control.exe timedate.cpl");
			break;
		case WM_LBUTTONUP:
			if (flags & MK_SHIFT)
			{
				SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)"SndVol.exe");
				break;
			}
			if (flags & MK_ALT)
			{
				SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)"explorer.exe ms-availablenetworks:");
				break;
			}

			if (m_bar->clkBtn1Click[0] == '\0')
			{
				n = 1;
				flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
				PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
			}
			else
			{
				cmd = m_bar->clkBtn1Click;
				SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
			}
			break;
		case WM_RBUTTONUP:
			if (m_bar->clkBtn2Click[0] == '\0')
			{
				n = 2;
				flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
				PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
			}
			else
			{
				cmd = m_bar->clkBtn2Click;
				SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
			}
			break;
		case WM_MBUTTONUP:
			if (m_bar->clkBtn3Click[0] == '\0')
			{
				n = 3;
				flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
				PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
			}
			else
			{
				cmd = m_bar->clkBtn3Click;
				SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
			}
			break;
		case WM_XBUTTONUP:
			switch (HIWORD(flags))
			{
				case XBUTTON1:
					if (m_bar->clkBtn4Click[0] == '\0')
					{
						n = 4;
						flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
						PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
					}
					else
					{
						cmd = m_bar->clkBtn4Click;
						SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
					}
					break;
				case XBUTTON2:
					if (m_bar->clkBtn5Click[0] == '\0')
					{
						n = 5;
						flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
						PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
					}
					else
					{
						cmd = m_bar->clkBtn5Click;
						SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
					}
					break;
				case XBUTTON3:
					if (m_bar->clkBtn6Click[0] == '\0')
					{
						n = 6;
						flags &= (MK_CONTROL | MK_SHIFT | MK_ALT);
						PostMessage(BBhwnd, BB_DESKCLICK, flags, n);
					}
					else
					{
						cmd = m_bar->clkBtn6Click;
						SendMessage(BBhwnd, BB_EXECUTEASYNC, 0, (LPARAM)cmd);
					}
					break;
			}
			break;
	}
}





// fill in a space or new line

spacer::spacer(int typ, barinfo* bi) : baritem(typ, bi)
{
	ZeroMemory(&itemRect, sizeof(RECT));
}





// buttons

	//-----------------------------
bar_button::bar_button(int m, barinfo* bi) : baritem(m, bi)
{
	dir = m == M_TDPB || m == M_CUOB ? 0 : m == M_WINB_L || m == M_WSPB_L ? -1 : 1;
}

//-----------------------------
void bar_button::draw()
{
	StyleItem* pSI = (StyleItem*)GetSettingPtr(
		m_active || (dir > 0 && m_bar->force_button_pressed)
		? SN_TOOLBARBUTTONP : SN_TOOLBARBUTTON
	);

	m_bar->pBuff->MakeStyleGradient(m_bar->hdcPaint, &itemRect, pSI, pSI->bordered);

	HPEN Pen = CreatePen(PS_SOLID, 1, pSI->picColor);
	HGDIOBJ other = SelectObject(m_bar->hdcPaint, Pen);
	int w = (itemRect.right - itemRect.left) / 2;
	int x = itemRect.left + w;
	int y = itemRect.top + w;

	if (0 == dir)
	{
		int z = 2;
		Arc(m_bar->hdcPaint, x - 2, y - 2, x + z, y + z, x, 0, x, 0);
		if (m_type == M_CUOB && false == m_bar->currentOnly)
		{
			z--;
			MoveToEx(m_bar->hdcPaint, x - 1, y - z, NULL);
			LineTo(m_bar->hdcPaint, x - 1, y + z);
			MoveToEx(m_bar->hdcPaint, x, y - z, NULL);
			LineTo(m_bar->hdcPaint, x, y + z);
			MoveToEx(m_bar->hdcPaint, x + 1, y - z, NULL);
			LineTo(m_bar->hdcPaint, x + 1, y + z);
		}
	}
	else
	{
		if (!m_bar->arrowBullets)
		{
			if (1 == dir)
			{
				MoveToEx(m_bar->hdcPaint, x + 3, y + 3, NULL);
				LineTo(m_bar->hdcPaint, x + 3, y - 3);
				LineTo(m_bar->hdcPaint, x - 3, y - 3);
				LineTo(m_bar->hdcPaint, x - 3, y - 1);
				LineTo(m_bar->hdcPaint, x + 1, y - 1);
				LineTo(m_bar->hdcPaint, x + 1, y + 3);
				LineTo(m_bar->hdcPaint, x + 3, y + 3);
				MoveToEx(m_bar->hdcPaint, x - 3, y + 3, NULL);
				LineTo(m_bar->hdcPaint, x - 1, y + 3);
				LineTo(m_bar->hdcPaint, x - 1, y + 1);
				LineTo(m_bar->hdcPaint, x - 3, y + 1);
				LineTo(m_bar->hdcPaint, x - 3, y + 3);
			}
			else
			{
				MoveToEx(m_bar->hdcPaint, x - 3, y - 3, NULL);
				LineTo(m_bar->hdcPaint, x - 3, y + 3);
				LineTo(m_bar->hdcPaint, x + 3, y + 3);
				LineTo(m_bar->hdcPaint, x + 3, y + 1);
				LineTo(m_bar->hdcPaint, x - 1, y + 1);
				LineTo(m_bar->hdcPaint, x - 1, y - 3);
				LineTo(m_bar->hdcPaint, x - 3, y - 3);
				MoveToEx(m_bar->hdcPaint, x + 3, y - 3, NULL);
				LineTo(m_bar->hdcPaint, x + 1, y - 3);
				LineTo(m_bar->hdcPaint, x + 1, y - 1);
				LineTo(m_bar->hdcPaint, x + 3, y - 1);
				LineTo(m_bar->hdcPaint, x + 3, y - 3);
			}
		}
		else
		{
			bbDrawPix(m_bar->hdcPaint, &itemRect, pSI->picColor, dir > 0 ? BS_TRIANGLE : -BS_TRIANGLE);
		}
	}
	DeleteObject(SelectObject(m_bar->hdcPaint, other));
}

//-----------------------------
// for the buttons, the mouse is captured on button-down
void bar_button::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (false == check_capture(mx, my, message))
		return;

	if (message == WM_LBUTTONUP)
	{
		switch (m_type)
		{
			case M_CUOB:
				m_bar->currentOnly = false == m_bar->currentOnly;
				m_bar->NewTasklist();

				m_bar->WriteRCSettings();
				m_bar->update(UPD_DRAW);
				break;

			case M_TDPB:
				if (++m_bar->TaskStyle == 3)
					m_bar->TaskStyle = 0;

				m_bar->WriteRCSettings();
				m_bar->update(UPD_DRAW);
				break;

			case M_WINB_L:
			case M_WINB_R:
				PostMessage(BBhwnd, BB_WORKSPACE,
					(dir > 0) ^ m_bar->reverseTasks
					? BBWS_NEXTWINDOW
					: BBWS_PREVWINDOW,
					m_bar->currentOnly ^ (0 == (flags & MK_SHIFT))
				);
				break;

			case M_WSPB_L:
			case M_WSPB_R:
				PostMessage(BBhwnd, BB_WORKSPACE, dir > 0 ? BBWS_DESKRIGHT : BBWS_DESKLEFT, 0);
				break;
		}
	}
	else if (message == WM_RBUTTONUP)
	{
		switch (m_type)
		{
			case M_CUOB:
				m_bar->trayToggleShowAll(-1);
				break;

			case M_TDPB:
				if (++m_bar->TaskStyle == 3)
					m_bar->TaskStyle = 0;

				m_bar->WriteRCSettings();
				m_bar->update(UPD_DRAW);
				break;

			case M_WINB_L:
			case M_WINB_R:
				PostMessage(BBhwnd, BB_WORKSPACE,
					(dir > 0) ^ m_bar->reverseTasks
					? BBWS_PREVWINDOW
					: BBWS_NEXTWINDOW,
					m_bar->currentOnly ^ (0 == (flags & MK_SHIFT))
				);
				break;

			case M_WSPB_L:
			case M_WSPB_R:
				PostMessage(BBhwnd, BB_WORKSPACE, dir > 0 ? BBWS_DESKLEFT : BBWS_DESKRIGHT, 0);
				break;
		}
	}
}


void bar_button::settip()
{
	if (m_type == M_CUOB)
		SetToolTip(m_bar->hwnd, &itemRect,
			"left-click: toggle current-only taskmode"
			"\nright-click: toggle hidden trayicons"
		);
}






// task zone

taskitemlist::taskitemlist(barinfo* bi) : baritemlist(M_TASKLIST, bi) { len = 0; }

//-----------------------------
// This one assigns the individual locations and sizes for
// the items in the task-list

bool taskitemlist::calc_sizes(void)
{
	bool f = false;
	int ts = m_bar->GetTaskListSizeEx();
	int n;

	if (ts != len)
	{
		clear();
		for (n = 0; n < ts; ++n)
			add(new taskentry(m_bar->reverseTasks ? ts - n - 1 : n, m_bar));
		f = true;
		len = ts;
	}

	if (0 == ts)
		return f;

	int b = styleBevelWidth;
	int w = itemRect.right - itemRect.left + b;
	int h = itemRect.bottom - itemRect.top;
	int xpos = itemRect.left;
	//int is = m_bar->TASK_ICON_SIZE + b + 2;
	int is = h + b;
	bool icon_mode = 1 == m_bar->TaskStyle;
	int min_width = is / 2;
	int max_width = imax(w * m_bar->taskMaxWidth / 100, is);
	if (w / ts >= max_width)
		w = ts * max_width;

	struct itemlist* p;
	n = 0;
	dolist(p, items)
	{
		int left, right;
		if (icon_mode)
		{
			left = xpos + n * is;
			right = left + is - b;
		}
		else
		{
			left = xpos + w * n / ts;
			right = xpos + w * (n + 1) / ts - b;
			if (right - left < min_width)
				right = left + min_width;
		}

		if (right > itemRect.right)
			break;

		f |= p->item->set_location(&left, itemRect.top, right - left, h, m_margin);
		++n;
	}
	return f;
}

//-----------------------------
void taskitemlist::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (flags & MK_ALT)
	{
		switch (message)
		{
			case WM_LBUTTONDOWN:
			case WM_LBUTTONDBLCLK:
				if (++m_bar->TaskStyle == 3)
					m_bar->TaskStyle = 0;

				m_bar->WriteRCSettings();
				m_bar->update(UPD_DRAW);
				break;

			case WM_RBUTTONDOWN:
			case WM_RBUTTONDBLCLK:
				m_bar->currentOnly = false == m_bar->currentOnly;
				m_bar->NewTasklist();

				m_bar->WriteRCSettings();
				m_bar->update(UPD_DRAW);
				break;
		}
		return;
	}
	baritemlist::mouse_event(mx, my, message, flags);
}

void taskitemlist::invalidate(int flag)
{
	baritem::invalidate(0);
}

/*
	void taskitemlist::draw()
	{
		if (TaskStyle == 1)
		{
			m_bar->pBuff->MakeStyleGradient(hdcPaint,  &itemRect, TaskStyle_L, false);
		}
		baritemlist::draw();
	}
*/




// tray zone

trayitemlist::trayitemlist(barinfo* bi) : baritemlist(M_TRAYLIST, bi)
{
	len = 0;
}

//-----------------------------
// This one assigns the individual locations and sizes for
// the items in the tray-icon-list
bool trayitemlist::calc_sizes(void)
{
	int w, h, ts, n, s, xpos;
	struct itemlist* p;
	bool f;

	h = itemRect.bottom - itemRect.top;
	w = itemRect.right - itemRect.left;

	f = false;
	ts = m_bar->GetTraySizeEx();

	if (ts != len)
	{
		clear();
		for (n = 0; n < ts; ++n)
			add(new trayentry(n, m_bar));
		f = true;
		len = ts;
	}

	if (0 == ts)
		return f;

	s = (h - m_bar->TRAY_ICON_SIZE) / 2;
	xpos = itemRect.left;

	n = 0;
	dolist(p, items)
	{
		int right = xpos + m_bar->TRAY_ICON_SIZE + 2;
		if (right > itemRect.right)
			break;

		f |= p->item->set_location(&xpos, itemRect.top + s - 1, right - xpos, s + m_bar->TRAY_ICON_SIZE + 1, m_margin);
		++n;
	};

	return f;
}

void trayitemlist::invalidate(int flag)
{
	if (flag == M_TRAYLIST)
		baritemlist::invalidate(M_TRAY);
	else
		baritem::invalidate(0);
}

void trayitemlist::mouse_event(int mx, int my, int message, unsigned flags)
{
	if ((MK_ALT | MK_CONTROL) & flags)
	{
		if (WM_RBUTTONDOWN == message)
			return;
		if (WM_RBUTTONDBLCLK == message)
			return;
		if (WM_RBUTTONUP == message)
		{
			if (MK_ALT & flags)
				m_bar->trayToggleShowAll(-1);
			else
				m_bar->trayMenu(true);
			return;
		}
	}
	baritemlist::mouse_event(mx, my, message, flags);
}

/*
	void trayitemlist::draw()
	{
		m_bar->pBuff->MakeStyleGradient(hdcPaint,  &itemRect, TaskStyle_L, false);
		baritemlist::draw();
	}
*/





// LeanBar - the main class

	//-----------------------------
LeanBar::LeanBar(barinfo* bi) : baritemlist(M_BARLIST, bi)
{

}

void LeanBar::settip()
{
	baritemlist::settip();
	//ClearToolTips(m_bar->hwnd);
}


//-----------------------------
void LeanBar::invalidate(int flag)
{
	if (M_TASKLIST == flag || M_TRAYLIST == flag || M_WINL == flag)
	{
		baritemlist::invalidate(flag); // redraw related items
		return;
	}

	if (UPD_NEW == flag)
		create_bar(); // rebuild items from scratch

	if (calc_sizes() || UPD_DRAW == flag)
	{
		baritem::invalidate(0); // redraw entire bar
		return;
	}

	baritemlist::invalidate(flag);
}

//-----------------------------
// check for capture, otherwise dispatch the mouse event
void LeanBar::mouse_event(int mx, int my, int message, unsigned flags)
{
	if (m_bar->hwnd == GetCapture())
	{
		// on capture, the captured item gets the message
		if (m_bar->capture_item)
			m_bar->capture_item->mouse_event(mx, my, message, flags);
		else
			ReleaseCapture(); // bar has been rebuilt in between
		return;

	}
	baritemlist::mouse_event(mx, my, message, flags);
}

//-----------------------------
// build everything from scratch
void LeanBar::create_bar()
{
	char* item_ptr;

	clear();
	max_label_width = 0;
	max_clock_width = 0;

	for (item_ptr = m_bar->item_string; *item_ptr; item_ptr++)
	{
		switch (*item_ptr)
		{
			case M_TDPB:
			case M_CUOB:
			case M_WSPB_L:
			case M_WSPB_R:
			case M_WINB_L:
				add(new bar_button(*item_ptr, m_bar));
				break;
			case M_WINB_R:
				add(new bar_button(*item_ptr, m_bar));
				break;
			case M_NEWLINE:
			case M_SPAC:
				add(new spacer(*item_ptr, m_bar));
				break;
			case M_WSPL:
				add(new workspace_label(m_bar));
				break;
			case M_CLCK:
				add(new clock_displ(m_bar));
				break;
			case M_WINL:
				add(new window_label(m_bar));
				break;
			case M_TASK:
				add(new taskitemlist(m_bar));
				break;
			case M_TRAY:
				add(new trayitemlist(m_bar));
				break;

		}
	}
}

//-----------------------------
bool LeanBar::calc_sizes(void)
{
	struct itemlist* p;
	int s, line, top, xpos, height;
	bool f;

	xpos = 0;
	s = m_bar->TRAY_ICON_SIZE + 2;
	trayzone_width = imax(s / 3, s * m_bar->GetTraySizeEx());
	f = this->set_location(&xpos, 0, m_bar->width, m_bar->height, 0);

	// loop though lines
	for (p = items, top = line = 0; p; ++line)
	{
		height = m_bar->bbLeanBarLineHeight[line];
		f |= calc_line_size(&p, top, top + height, height);
		top += height - styleBorderWidth;
	}
	return f;
}

bool LeanBar::calc_line_size(itemlist** p0, int top, int bottom, int height)
{
	int winlabel_width,
		taskzone_width,
		label_width,
		clock_width;

	clock_width =
		label_width =
		winlabel_width =
		taskzone_width = 0;

	bool f = false;

	int b = styleBevelWidth;
	int bo = styleBorderWidth + b;
	int button_padding = (height - m_bar->buttonH) / 2 - styleBorderWidth;
	int prev_margin = b;
	int xpos = styleBorderWidth;
	bool isbutton = false;

	int t0, t1, lr;
	t0 = lr = 0;

	struct itemlist* p;

	// --- 1st pass ----------------------------------
	dolist(p, *p0)
	{
		t1 = p->item->m_type;
		if (M_NEWLINE == t1)
			break;

		int pm = prev_margin;
		prev_margin = b;
		xpos += pm;
		bool ib = isbutton;
		isbutton = false;

		switch (t1)
		{
			case M_TDPB:
			case M_CUOB:
			case M_WSPB_L:
			case M_WSPB_R:
			case M_WINB_L:
			case M_WINB_R:
				xpos += imax(0, button_padding - pm) + m_bar->buttonH;
				if (ib && button_padding == 0)
					xpos -= m_bar->buttonBorder;
				isbutton = true;
				prev_margin = button_padding;
				break;

			case M_WSPL:
				label_width = m_bar->labelWidth + 3 + 2 * m_bar->labelIndent;
				lr |= 2 + (t0 == 0);
				break;

			case M_CLCK:
				clock_width = m_bar->clockWidth + 3 + 2 * m_bar->labelIndent;
				lr |= 4 + (t0 == 0);
				break;

			case M_WINL:
				winlabel_width = 1;
				break;

			case M_SPAC:
				xpos += styleBevelWidth + 1;
				break;

			case M_TASKLIST:
				taskzone_width = 1;
				break;

			case M_TRAYLIST:
				xpos += trayzone_width;
				break;
		}
		t0 = t1;
	}
	if (t0 == M_CLCK || t0 == M_WSPL) lr |= 8;

	xpos += prev_margin + styleBorderWidth;

	// --- label and clock balance ---------------------------------
	if ((lr & 2) && (max_label_width < label_width || max_label_width >= label_width + 12))
		max_label_width = label_width + 4;
	if ((lr & 4) && (max_clock_width < clock_width || max_clock_width >= clock_width + 12))
		max_clock_width = clock_width + 4;
	if (lr == 15)
		max_label_width = max_clock_width = imax(max_label_width, max_clock_width);
	if (lr & 2)
		xpos += max_label_width;
	if (lr & 4)
		xpos += max_clock_width;

	// --- assign variable widths ----------------------------------
	int rest_width = imax(0, itemRect.right - xpos);

	if (taskzone_width && winlabel_width)
	{

		// i just did the opposite of what the original dev did because i dont like when the winlabel bar is like half of the bar... i prefer to have a bigger taskzone. like the original windows taskbar
		winlabel_width = 150;
		taskzone_width = rest_width - winlabel_width;
	}
	else if (winlabel_width)
	{
		winlabel_width = rest_width;
	}
	else if (taskzone_width)
	{
		taskzone_width = rest_width;
	}

	// --- 2nd pass ----------------------------------
	prev_margin = b;
	xpos = styleBorderWidth;

	dolist(p, *p0)
	{
		baritem* gi = p->item;

		if (M_NEWLINE == gi->m_type)
		{
			p = p->next;
			break;
		}

		int ypos = top + bo;
		int hs = height - bo - bo;
		int ws = 0;
		int pm = prev_margin;
		bool ib = isbutton;

		xpos += pm;
		prev_margin = b;
		isbutton = false;

		switch (gi->m_type)
		{
			case M_TDPB:
			case M_CUOB:
			case M_WSPB_L:
			case M_WSPB_R:
			case M_WINB_L:
			case M_WINB_R:
				xpos += imax(0, button_padding - pm);
				if (ib && button_padding == 0)
					xpos -= m_bar->buttonBorder;
				isbutton = true;
				prev_margin = button_padding;
				ypos = top + (height - (hs = ws = m_bar->buttonH)) / 2;
				break;

			case M_SPAC:
				ws = b + 1;
				break;

			case M_TRAYLIST:
				ws = trayzone_width;
				break;

			case M_WSPL:
				ws = max_label_width;
				ypos = top + (height - (hs = m_bar->labelH)) / 2;
				break;
			case M_CLCK:
				ws = max_clock_width;
				ypos = top + (height - (hs = m_bar->labelH)) / 2;
				break;
			case M_WINL:
				ws = winlabel_width;
				ypos = top + (height - (hs = m_bar->labelH)) / 2;
				break;
			case M_TASKLIST:
				ws = taskzone_width;
				ypos = top + (height - (hs = m_bar->labelH)) / 2;
				break;
		}

		f |= gi->set_location(&xpos, ypos, ws, hs, ypos - top);
		if (gi->m_type >= M_BARLIST)
			gi->calc_sizes();
	}
	*p0 = p;
	return f;
}

//-----------------------------
// Here sizes are calculated in two passes: The first pass
// gets all fixed sizes. Then the remaining space is assigned
// to the variable ones (windowlabel/taskzone). The second pass
// assigns the actual x-coords.

//bool LeanBar::calc_line_size(struct itemlist** p0, int top, int bottom, int height)
//{
//
//}
