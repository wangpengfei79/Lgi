#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GInput.h"
#include "GScrollBar.h"
#ifdef WIN32
#include <imm.h>
#endif
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GViewPriv.h"
#include "GCssTools.h"
#include "GFontCache.h"
#include "GUnicode.h"

#include "GHtmlCommon.h"
#include "GHtmlParser.h"
#include "LgiRes.h"

#define DefaultCharset              "utf-8"

#define GDCF_UTF8					-1
#define LUIS_DEBUG					0
#define POUR_DEBUG					0
#define PROFILE_POUR				0

#define ALLOC_BLOCK					64
#define IDC_VS						1000

#ifndef IDM_OPEN
#define IDM_OPEN					1
#endif
#ifndef IDM_NEW
#define	IDM_NEW						2
#endif
#ifndef IDM_COPY
#define IDM_COPY					3
#endif
#ifndef IDM_CUT
#define IDM_CUT						4
#endif
#ifndef IDM_PASTE
#define IDM_PASTE					5
#endif
#define IDM_COPY_URL				6
#define IDM_AUTO_INDENT				7
#define IDM_UTF8					8
#define IDM_PASTE_NO_CONVERT		9
#ifndef IDM_UNDO
#define IDM_UNDO					10
#endif
#ifndef IDM_REDO
#define IDM_REDO					11
#endif
#define IDM_FIXED					12
#define IDM_SHOW_WHITE				13
#define IDM_HARD_TABS				14
#define IDM_INDENT_SIZE				15
#define IDM_TAB_SIZE				16
#define IDM_DUMP					17
#define IDM_RTL						18
#define IDM_COPY_ORIGINAL			19

#define PAINT_BORDER				Back
#define PAINT_AFTER_LINE			Back

#define CODEPAGE_BASE				100
#define CONVERT_CODEPAGE_BASE		200

#if !defined(WIN32) && !defined(toupper)
#define toupper(c)					(((c)>='a'&&(c)<='z') ? (c)-'a'+'A' : (c))
#endif

static char SelectWordDelim[] = " \t\n.,()[]<>=?/\\{}\"\';:+=-|!@#$%^&*";

#include "GRichTextEditPriv.h"

//////////////////////////////////////////////////////////////////////
enum UndoType
{
	UndoDelete, UndoInsert, UndoChange
};

class GRichEditUndo : public GUndoEvent
{
	GRichTextEdit *View;
	UndoType Type;

public:
	GRichEditUndo(	GRichTextEdit *view,
					char16 *t,
					int len,
					int at,
					UndoType type)
	{
		View = view;
		Type = type;
	}

	~GRichEditUndo()
	{
	}

	void OnChange()
	{
	}

	// GUndoEvent
    void ApplyChange()
	{
	}

    void RemoveChange()
	{
	}
};

//////////////////////////////////////////////////////////////////////
GRichTextEdit::GRichTextEdit(	int Id,
						int x, int y, int cx, int cy,
						GFontType *FontType)
	: ResObject(Res_Custom)
{
	// init vars
	GView::d->Css.Reset(d = new GRichTextPriv(this));
	
	// setup window
	SetId(Id);

	// default options
	#if WINNATIVE
	CrLf = true;
	SetDlgCode(DLGC_WANTALLKEYS);
	#else
	CrLf = false;
	#endif
	d->Padding(GCss::Len(GCss::LenPx, 4));
	d->BackgroundColor(GCss::ColorDef(GCss::ColorRgb, Rgb24To32(LC_WORKSPACE)));
	SetFont(SysFont);

	#if 0 // def _DEBUG
	Name("<html>\n"
		"<body>\n"
		"	This is some <b style='font-size: 20pt; color: green;'>bold text</b> to test with.<br>\n"
		"   A second line of text for testing.\n"
		"</body>\n"
		"</html>\n");
	#endif
}

GRichTextEdit::~GRichTextEdit()
{
	// 'd' is owned by the GView CSS autoptr.
}

bool GRichTextEdit::IsDirty()
{
	return false;
}

void GRichTextEdit::IsDirty(bool d)
{
}

char16 *GRichTextEdit::MapText(char16 *Str, int Len, bool RtlTrailingSpace)
{
	if (ObscurePassword || ShowWhiteSpace || RtlTrailingSpace)
	{
	}

	return Str;
}

void GRichTextEdit::SetFixedWidthFont(bool i)
{
	if (FixedWidthFont ^ i)
	{
		if (i)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GDocView::SetFixedWidthFont(i);
			}
		}

		OnFontChange();
		Invalidate();
	}
}

void GRichTextEdit::SetReadOnly(bool i)
{
	GDocView::SetReadOnly(i);

	#if WINNATIVE
	SetDlgCode(i ? DLGC_WANTARROWS : DLGC_WANTALLKEYS);
	#endif
}

void GRichTextEdit::SetTabSize(uint8 i)
{
	TabSize = limit(i, 2, 32);
	OnFontChange();
	OnPosChange();
	Invalidate();
}

void GRichTextEdit::SetWrapType(uint8 i)
{
	GDocView::SetWrapType(i);

	OnPosChange();
	Invalidate();
}

GFont *GRichTextEdit::GetFont()
{
	return d->Font;
}

void GRichTextEdit::SetFont(GFont *f, bool OwnIt)
{
	if (!f)
		return;
	
	if (OwnIt)
	{
		d->Font.Reset(f);
	}
	else if (d->Font.Reset(new GFont))
	{		
		*d->Font = *f;
		d->Font->Create(NULL, 0, 0);
	}
	
	OnFontChange();
}

void GRichTextEdit::OnFontChange()
{
}

void GRichTextEdit::PourText(int Start, int Length /* == 0 means it's a delete */)
{
}

void GRichTextEdit::PourStyle(int Start, int EditSize)
{
}

bool GRichTextEdit::Insert(int At, char16 *Data, int Len)
{
	return false;
}

bool GRichTextEdit::Delete(int At, int Len)
{
	return false;
}

void GRichTextEdit::DeleteSelection(char16 **Cut)
{
	if (d->Cursor &&
		d->Selection)
	{
		GArray<char16> DeletedText;
		GArray<char16> *DelTxt = Cut ? &DeletedText : NULL;

		bool Cf = d->CursorFirst();
		GRichTextPriv::BlockCursor *Start = Cf ? d->Cursor : d->Selection;
		GRichTextPriv::BlockCursor *End = Cf ? d->Selection : d->Cursor;
		if (Start->Blk == End->Blk)
		{
			// In the same block... just delete the text
			int Len = End->Offset - Start->Offset;
			Start->Blk->DeleteAt(Start->Offset, Len, DelTxt);
		}
		else
		{
			// Multi-block delete...

			// 1) Delete all the content to the end of the first block
			int StartLen = Start->Blk->Length();
			if (Start->Offset < StartLen)
			{
				Start->Blk->DeleteAt(Start->Offset, StartLen - Start->Offset, DelTxt);
			}

			// 2) Delete any blocks between 'Start' and 'End'
			unsigned i = d->Blocks.IndexOf(Start->Blk);
			unsigned EndIdx = d->Blocks.IndexOf(End->Blk);
			if (i >= 0 && EndIdx >= i)
			{
				for (++i; d->Blocks[i] != End->Blk && i < d->Blocks.Length(); i++)
				{
					GRichTextPriv::Block *&b = d->Blocks[i];
					b->CopyAt(0, -1, DelTxt);
					d->Blocks.DeleteAt(i, true);
					DeleteObj(b);
				}
			}
			else LgiAssert(0);

			// 3) Delete any text up to the Cursor in the 'End' block
			End->Blk->DeleteAt(0, End->Offset, DelTxt);
		}

		// Set the cursor and update the screen
		d->Cursor->Set(Start->Blk, Start->Offset);
		d->Selection.Reset();
		Invalidate();

		if (Cut)
		{
			DelTxt->Add(0);
			*Cut = DelTxt->Release();
		}
	}
	else LgiAssert(0);
}

int64 GRichTextEdit::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GRichTextEdit::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LGI_PrintfInt64, i);
	Name(Str);
}

char *GRichTextEdit::Name()
{
	d->ToHtml();
	return d->UtfNameCache;
}

static GHtmlElement *FindElement(GHtmlElement *e, HtmlTag TagId)
{
	if (e->TagId == TagId)
		return e;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		GHtmlElement *c = FindElement(e->Children[i], TagId);
		if (c)
			return c;
	}
	
	return NULL;
}

void GRichTextEdit::OnAddStyle(const char *MimeType, const char *Styles)
{
	if (d->CreationCtx)
	{
		d->CreationCtx->StyleStore.Parse(Styles);
	}
}

bool GRichTextEdit::Name(const char *s)
{
	d->Empty();
	d->OriginalText = s;
	
	GHtmlElement Root(NULL);

	if (!d->CreationCtx.Reset(new GRichTextPriv::CreateContext(d)))
		return false;

	if (!d->GHtmlParser::Parse(&Root, s))
		return d->Error(_FL, "Failed to parse HTML.");

	GHtmlElement *Body = FindElement(&Root, TAG_BODY);
	if (!Body)
		Body = &Root;

	bool Status = d->FromHtml(Body, *d->CreationCtx);
	if (Status)
		SetCursor(0, false);
	
	if (!d->Blocks.Length())
	{
		d->EmptyDoc();
	}
	
	// d->DumpBlocks();
	
	return Status;
}

char16 *GRichTextEdit::NameW()
{
	d->WideNameCache.Reset(Utf8ToWide(Name()));
	return d->WideNameCache;
}

bool GRichTextEdit::NameW(const char16 *s)
{
	GAutoString a(WideToUtf8(s));
	return Name(a);
}

char *GRichTextEdit::GetSelection()
{
	if (HasSelection())
	{
	}

	return 0;
}

bool GRichTextEdit::HasSelection()
{
	return d->Selection.Get() != NULL;
}

void GRichTextEdit::SelectAll()
{
	Invalidate();
}

void GRichTextEdit::UnSelectAll()
{
	bool Update = HasSelection();

	if (Update)
	{
		Invalidate();
	}
}

int GRichTextEdit::GetLines()
{
	return 0;
}

void GRichTextEdit::GetTextExtent(int &x, int &y)
{
}

void GRichTextEdit::PositionAt(int &x, int &y, int Index)
{
}

int GRichTextEdit::GetCursor(bool Cur)
{
	if (!d->Cursor)
		return -1;
		
	int CharPos = 0;
	for (unsigned i=0; i<d->Blocks.Length(); i++)
	{
		GRichTextPriv::Block *b = d->Blocks[i];
		if (d->Cursor->Blk == b)
			return CharPos + d->Cursor->Offset;
		CharPos += b->Length();
	}
	
	LgiAssert(!"Cursor block not found.");
	return -1;
}

int GRichTextEdit::IndexAt(int x, int y)
{
	return d->HitTest(x, y);
}

void GRichTextEdit::SetCursor(int i, bool Select, bool ForceFullUpdate)
{
	int Offset = -1;
	GRichTextPriv::Block *Blk = d->GetBlockByIndex(i, &Offset);
	if (Blk)
	{
		GAutoPtr<GRichTextPriv::BlockCursor> c(new GRichTextPriv::BlockCursor(Blk, Offset));
		if (c)
		{
			c->Blk->GetPosFromIndex(&c->Pos, &c->Line, Offset);
			d->SetCursor(c, Select);
		}
	}
}

bool GRichTextEdit::Cut()
{
	if (!HasSelection())
		return false;

	char16 *Txt = NULL;
	DeleteSelection(&Txt);
	bool Status = true;
	if (Txt)
	{
		GClipBoard Cb(this);
		Status = Cb.TextW(Txt);
		DeleteArray(Txt);
	}

	return Status;
}

bool GRichTextEdit::Copy()
{
	if (!HasSelection())
		return false;

	return true;
}

bool GRichTextEdit::Paste()
{
	GClipBoard Clip(this);
	
	return false;
}

bool GRichTextEdit::ClearDirty(bool Ask, char *FileName)
{
	if (1 /*dirty*/)
	{
		int Answer = (Ask) ? LgiMsg(this,
									LgiLoadString(L_TEXTCTRL_ASK_SAVE, "Do you want to save your changes to this document?"),
									LgiLoadString(L_TEXTCTRL_SAVE, "Save"),
									MB_YESNOCANCEL) : IDYES;
		if (Answer == IDYES)
		{
			GFileSelect Select;
			Select.Parent(this);
			if (!FileName &&
				Select.Save())
			{
				FileName = Select.Name();
			}

			Save(FileName);
		}
		else if (Answer == IDCANCEL)
		{
			return false;
		}
	}

	return true;
}

bool GRichTextEdit::Open(const char *Name, const char *CharSet)
{
	bool Status = false;
	GFile f;

	if (f.Open(Name, O_READ|O_SHARE))
	{
		size_t Bytes = (size_t)f.GetSize();
		SetCursor(0, false);
		
		char *c8 = new char[Bytes + 4];
		if (c8)
		{
			if (f.Read(c8, (int)Bytes) == Bytes)
			{
				char *DataStart = c8;

				c8[Bytes] = 0;
				c8[Bytes+1] = 0;
				c8[Bytes+2] = 0;
				c8[Bytes+3] = 0;
				
				if ((uchar)c8[0] == 0xff && (uchar)c8[1] == 0xfe)
				{
					// utf-16
					if (!CharSet)
					{
						CharSet = "utf-16";
						DataStart += 2;
					}
				}
				
			}

			DeleteArray(c8);
		}
		else
		{
		}

		Invalidate();
	}

	return Status;
}

bool GRichTextEdit::Save(const char *Name, const char *CharSet)
{
	GFile f;
	if (f.Open(Name, O_WRITE))
	{
		f.SetSize(0);
	}
	return false;
}

void GRichTextEdit::UpdateScrollBars(bool Reset)
{
	if (VScroll)
	{
		//GRect Before = GetClient();

	}
}

bool GRichTextEdit::DoCase(bool Upper)
{
	return true;
}

int GRichTextEdit::GetLine()
{
	int Idx = 0;
	return Idx + 1;
}

void GRichTextEdit::SetLine(int i)
{
}

bool GRichTextEdit::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto line:"), "Text");
	if (Dlg.DoModal() == IDOK &&
		Dlg.Str)
	{
		SetLine(atoi(Dlg.Str));
	}

	return true;
}

GDocFindReplaceParams *GRichTextEdit::CreateFindReplaceParams()
{
	return new GDocFindReplaceParams3;
}

void GRichTextEdit::SetFindReplaceParams(GDocFindReplaceParams *Params)
{
	if (Params)
	{
	}
}

bool GRichTextEdit::DoFindNext()
{
	return false;
}

bool
RichText_FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	return true;
}

bool GRichTextEdit::DoFind()
{
	char *u = 0;
	if (HasSelection())
	{
	}
	else
	{
	}

	GFindDlg Dlg(this, u, RichText_FindCallback, this);
	Dlg.DoModal();
	DeleteArray(u);
	
	Focus(true);

	return false;
}

bool GRichTextEdit::DoReplace()
{
	return false;
}

void GRichTextEdit::SelectWord(int From)
{
	Invalidate();
}

bool GRichTextEdit::OnFind(char16 *Find, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	return false;
}

bool GRichTextEdit::OnReplace(char16 *Find, char16 *Replace, bool All, bool MatchWord, bool MatchCase, bool SelectionOnly)
{
	if (ValidStrW(Find))
	{
	}	
	
	return false;
}

bool GRichTextEdit::OnMultiLineTab(bool In)
{
	return false;
}

void GRichTextEdit::OnSetHidden(int Hidden)
{
}

void GRichTextEdit::OnPosChange()
{
	static bool Processing = false;

	if (!Processing)
	{
		Processing = true;
		GLayout::OnPosChange();

		// GRect c = GetClient();
		Processing = false;
	}
}

int GRichTextEdit::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	for (char *s = Formats.First(); s; )
	{
		if (!_stricmp(s, "text/uri-list") ||
			!_stricmp(s, "text/html") ||
			!_stricmp(s, "UniformResourceLocatorW"))
		{
			s = Formats.Next();
		}
		else
		{
			// LgiTrace("Ignoring format '%s'\n", s);
			Formats.Delete(s);
			DeleteArray(s);
			s = Formats.Current();
		}
	}

	return Formats.Length() ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

int GRichTextEdit::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	/* FIXME
	if (!_stricmp(Format, "text/uri-list") ||
		!_stricmp(Format, "text/html") ||
		!_stricmp(Format, "UniformResourceLocatorW"))
	{
		if (Data->IsBinary())
		{
			char16 *e = (char16*) ((char*)Data->Value.Binary.Data + Data->Value.Binary.Length);
			char16 *s = (char16*)Data->Value.Binary.Data;
			int len = 0;
			while (s < e && s[len])
			{
				len++;
			}
			// Insert(Cursor, s, len);
			Invalidate();
			return DROPEFFECT_COPY;
		}
	}
	*/

	return DROPEFFECT_NONE;
}

void GRichTextEdit::OnCreate()
{
	SetWindow(this);
	DropTarget(true);

	if (Focus())
		SetPulse(1500);
}

void GRichTextEdit::OnEscape(GKey &K)
{
}

bool GRichTextEdit::OnMouseWheel(double l)
{
	if (VScroll)
	{
		int NewPos = (int)VScroll->Value() + (int) l;
		NewPos = limit(NewPos, 0, GetLines());
		VScroll->Value(NewPos);
		Invalidate();
	}
	
	return true;
}

void GRichTextEdit::OnFocus(bool f)
{
	Invalidate();
	SetPulse(f ? 500 : -1);
}

int GRichTextEdit::HitText(int x, int y)
{
	return d->HitTest(x, y);
}

void GRichTextEdit::Undo()
{
}

void GRichTextEdit::Redo()
{
}

void GRichTextEdit::DoContextMenu(GMouse &m)
{
	GMenuItem *i;
	GSubMenu RClick;
	GAutoString ClipText;
	{
		GClipBoard Clip(this);
		ClipText.Reset(NewStr(Clip.Text()));
	}

	#if LUIS_DEBUG
	RClick.AppendItem("Dump Layout", IDM_DUMP, true);
	RClick.AppendSeparator();
	#endif

	/*
	GStyle *s = HitStyle(HitText(m.x, m.y));
	if (s)
	{
		if (s->OnMenu(&RClick))
		{
			RClick.AppendSeparator();
		}
	}
	*/

	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_CUT, "Cut"), IDM_CUT, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_PASTE, "Paste"), IDM_PASTE, ClipText != 0);
	RClick.AppendSeparator();

	#if 0
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_UNDO, "Undo"), IDM_UNDO, false /* UndoQue.CanUndo() */);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_REDO, "Redo"), IDM_REDO, false /* UndoQue.CanRedo() */);
	RClick.AppendSeparator();

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_FIXED, "Fixed Width Font"), IDM_FIXED, true);
	if (i) i->Checked(GetFixedWidthFont());
	#endif

	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_AUTO_INDENT, "Auto Indent"), IDM_AUTO_INDENT, true);
	if (i) i->Checked(AutoIndent);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_SHOW_WHITESPACE, "Show Whitespace"), IDM_SHOW_WHITE, true);
	if (i) i->Checked(ShowWhiteSpace);
	
	i = RClick.AppendItem(LgiLoadString(L_TEXTCTRL_HARD_TABS, "Hard Tabs"), IDM_HARD_TABS, true);
	if (i) i->Checked(HardTabs);
	
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_INDENT_SIZE, "Indent Size"), IDM_INDENT_SIZE, true);
	RClick.AppendItem(LgiLoadString(L_TEXTCTRL_TAB_SIZE, "Tab Size"), IDM_TAB_SIZE, true);
	RClick.AppendItem("Copy Original", IDM_COPY_ORIGINAL, d->OriginalText.Get() != NULL);

	if (Environment)
		Environment->AppendItems(&RClick);

	int Id = 0;
	m.ToScreen();
	switch (Id = RClick.Float(this, m.x, m.y))
	{
		#if LUIS_DEBUG
		case IDM_DUMP:
		{
			int n=0;
			for (GTextLine *l=Line.First(); l; l=Line.Next(), n++)
			{
				LgiTrace("[%i] %i,%i (%s)\n", n, l->Start, l->Len, l->r.Describe());

				char *s = WideToUtf8(Text + l->Start, l->Len);
				if (s)
				{
					LgiTrace("%s\n", s);
					DeleteArray(s);
				}
			}
			break;
		}
		#endif
		case IDM_FIXED:
		{
			SetFixedWidthFont(!GetFixedWidthFont());							
			SendNotify(GNotifyFixedWidthChanged);
			break;
		}
		case IDM_CUT:
		{
			Cut();
			break;
		}
		case IDM_COPY:
		{
			Copy();
			break;
		}
		case IDM_PASTE:
		{
			Paste();
			break;
		}
		case IDM_UNDO:
		{
			Undo();
			break;
		}
		case IDM_REDO:
		{
			Redo();
			break;
		}
		case IDM_AUTO_INDENT:
		{
			AutoIndent = !AutoIndent;
			break;
		}
		case IDM_SHOW_WHITE:
		{
			ShowWhiteSpace = !ShowWhiteSpace;
			Invalidate();
			break;
		}
		case IDM_HARD_TABS:
		{
			HardTabs = !HardTabs;
			break;
		}
		case IDM_INDENT_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", IndentSize);
			GInput i(this, s, "Indent Size:", "Text");
			if (i.DoModal())
			{
				IndentSize = atoi(i.Str);
			}
			break;
		}
		case IDM_TAB_SIZE:
		{
			char s[32];
			sprintf_s(s, sizeof(s), "%i", TabSize);
			GInput i(this, s, "Tab Size:", "Text");
			if (i.DoModal())
			{
				SetTabSize(atoi(i.Str));
			}
			break;
		}
		case IDM_COPY_ORIGINAL:
		{
			GClipBoard c(this);
			c.Text(d->OriginalText);
			break;
		}
		default:
		{
			/*
			if (s)
			{
				s->OnMenuClick(Id);
			}
			*/

			if (Environment)
			{
				Environment->OnMenu(this, Id, 0);
			}
			break;
		}
	}
}

void GRichTextEdit::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	if (m.Down())
	{
		Focus(true);
		
		if (m.IsContextMenu())
		{
			DoContextMenu(m);
			return;
		}
		else
		{
			Focus(true);

			int Hit = HitText(m.x, m.y);
			d->WordSelectMode = !Processed && m.Double();

			if (Hit >= 0)
			{
				SetCursor(Hit, m.Shift());
				if (d->WordSelectMode)
					SelectWord(Hit);
			}
		}
	}

	if (!Processed)
	{
		Capture(m.Down());
	}
}

int GRichTextEdit::OnHitTest(int x, int y)
{
	#ifdef WIN32
	if (GetClient().Overlap(x, y))
	{
		return HTCLIENT;
	}
	#endif
	return GView::OnHitTest(x, y);
}

void GRichTextEdit::OnMouseMove(GMouse &m)
{
	int Hit = d->HitTest(m.x, m.y);
	if (IsCapturing())
	{
		if (!d->WordSelectMode)
		{
			SetCursor(Hit, m.Left());
		}
		else
		{
			/*
			int Min = Hit < d->WordSelectMode ? Hit : d->WordSelectMode;
			int Max = Hit > d->WordSelectMode ? Hit : d->WordSelectMode;

			for (SelStart = Min; SelStart > 0; SelStart--)
			{
				if (strchr(SelectWordDelim, Text[SelStart]))
				{
					SelStart++;
					break;
				}
			}

			for (SelEnd = Max; SelEnd < Size; SelEnd++)
			{
				if (strchr(SelectWordDelim, Text[SelEnd]))
				{
					break;
				}
			}

			Cursor = SelEnd;
			Invalidate();
			*/
		}
	}

	#ifdef WIN32
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);
	if (c.Overlap(m.x, m.y))
	{
		/*
		GStyle *s = HitStyle(Hit);
		TCHAR *c = (s) ? s->GetCursor() : 0;
		if (!c) c = IDC_IBEAM;
		::SetCursor(LoadCursor(0, MAKEINTRESOURCE(c)));
		*/
	}
	#endif
}

bool GRichTextEdit::OnKey(GKey &k)
{
	if (k.Down())
	{
		// Blink = true;
	}

	// k.Trace("GRichTextEdit::OnKey");

	if (k.IsContextMenu())
	{
		GMouse m;
		/*
		m.x = CursorPos.x1;
		m.y = CursorPos.y1 + (CursorPos.Y() >> 1);
		m.Target = this;
		*/
		DoContextMenu(m);
	}
	else if (k.IsChar)
	{
		switch (k.c16)
		{
			default:
			{
				// process single char input
				if
				(
					!GetReadOnly()
					&&
					(
						(k.c16 >= ' ' || k.c16 == VK_TAB)
						&&
						k.c16 != 127
					)
				)
				{
					if (k.Down())
					{
						// letter/number etc
						if (d->Cursor &&
							d->Cursor->Blk)
						{
							if (d->Cursor->Blk->AddText(d->Cursor->Offset, &k.c16, 1))
							{
								d->Cursor->Set(d->Cursor->Offset + 1);
								Invalidate();
							}
						}

						/*
						if (SelStart >= 0)
						{
							bool MultiLine = false;
							if (k.c16 == VK_TAB)
							{
								int Min = min(SelStart, SelEnd), Max = max(SelStart, SelEnd);
								for (int i=Min; i<Max; i++)
								{
									if (Text[i] == '\n')
									{
										MultiLine = true;
									}
								}
							}
							if (MultiLine)
							{
								if (OnMultiLineTab(k.Shift()))
								{
									return true;
								}
							}
							else
							{
								DeleteSelection();
							}
						}
						
						GTextLine *l = GetTextLine(Cursor);
						int Len = (l) ? l->Len : 0;
						
						if (l && k.c16 == VK_TAB && (!HardTabs || IndentSize != TabSize))
						{
							int x = GetColumn();							
							int Add = IndentSize - (x % IndentSize);
							
							if (HardTabs && ((x + Add) % TabSize) == 0)
							{
								int Rx = x;
								int Remove;
								for (Remove = Cursor; Text[Remove - 1] == ' ' && Rx % TabSize != 0; Remove--, Rx--);
								int Chars = Cursor - Remove;
								Delete(Remove, Chars);
								Insert(Remove, &k.c16, 1);
								Cursor = Remove + 1;
								
								Invalidate();
							}
							else
							{							
								char16 *Sp = new char16[Add];
								if (Sp)
								{
									for (int n=0; n<Add; n++) Sp[n] = ' ';
									if (Insert(Cursor, Sp, Add))
									{
										l = GetTextLine(Cursor);
										int NewLen = (l) ? l->Len : 0;
										SetCursor(Cursor + Add, false, Len != NewLen - 1);
									}
									DeleteArray(Sp);
								}
							}
						}
						else
						{
							char16 In = k.GetChar();

							if (In == '\t' &&
								k.Shift() &&
								Cursor > 0)
							{
								l = GetTextLine(Cursor);
								if (Cursor > l->Start)
								{
									if (Text[Cursor-1] == '\t')
									{
										Delete(Cursor - 1, 1);
										SetCursor(Cursor, false, false);
									}
									else if (Text[Cursor-1] == ' ')
									{
										int Start = Cursor - 1;
										while (Start >= l->Start && strchr(" \t", Text[Start-1]))
											Start--;
										int Depth = SpaceDepth(Text + Start, Text + Cursor);
										int NewDepth = Depth - (Depth % IndentSize);
										if (NewDepth == Depth && NewDepth > 0)
											NewDepth -= IndentSize;
										int Use = 0;
										while (SpaceDepth(Text + Start, Text + Start + Use + 1) < NewDepth)
											Use++;
										Delete(Start + Use, Cursor - Start - Use);
										SetCursor(Start + Use, false, false);
									}
								}
								
							}
							else if (In && Insert(Cursor, &In, 1))
							{
								l = GetTextLine(Cursor);
								int NewLen = (l) ? l->Len : 0;
								SetCursor(Cursor + 1, false, Len != NewLen - 1);
							}
						}
						*/
					}
					return true;
				}
				break;
			}
			case VK_RETURN:
			{
				if (GetReadOnly())
					break;

				if (k.Down() && k.IsChar)
				{
					OnEnter(k);
				}
				return true;
				break;
			}
			case VK_BACKSPACE:
			{
				if (GetReadOnly())
					break;

				if (k.Ctrl())
				{
				    // Ctrl+H
				}
				else if (k.Down())
				{
					if (HasSelection())
					{
						DeleteSelection();
					}
					else if (d->Cursor &&
							 d->Cursor->Blk)
					{
						if (d->Cursor->Offset > 0)
						{
							if (d->Cursor->Blk->DeleteAt(d->Cursor->Offset-1, 1))
							{
								d->Cursor->Set(d->Cursor->Offset - 1);
								Invalidate();
							}
						}
						else
						{
							LgiTrace("%s:%i - Impl deleting char from previous block\n", _FL);
						}
					}
				}
				return true;
				break;
			}
		}
	}
	else // not a char
	{
		switch (k.vkey)
		{
			case VK_TAB:
				return true;
			case VK_RETURN:
			{
				return !GetReadOnly();
			}
			case VK_BACKSPACE:
			{
				if (!GetReadOnly())
				{
					if (k.Alt())
					{
						if (k.Down())
						{
							if (k.Ctrl())
							{
								Redo();
							}
							else
							{
								Undo();
							}
						}
					}
					else if (k.Ctrl())
					{
						if (k.Down())
						{
							/*
							int Start = Cursor;
							while (IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							while (!IsWhiteSpace(Text[Cursor-1]) && Cursor > 0)
								Cursor--;

							Delete(Cursor, Start - Cursor);
							Invalidate();
							*/
						}
					}

					return true;
				}
				break;
			}
			case VK_F3:
			{
				if (k.Down())
				{
					DoFindNext();
				}
				return true;
				break;
			}
			case VK_LEFT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						d->SetCursor(d->CursorFirst() ? d->Cursor : d->Selection);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_StartOfLine;
						else
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkLeftWord : GRichTextPriv::SkLeftChar,
								k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_RIGHT:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					if (HasSelection() && !k.Shift())
					{
						GRect r = d->SelectionRect();
						Invalidate(&r);
						d->SetCursor(d->CursorFirst() ? d->Selection : d->Cursor);
					}
					else
					{
						#ifdef MAC
						if (k.System())
							goto Jump_EndOfLine;
						#endif

						d->Seek(d->Cursor,
								k.Ctrl() ? GRichTextPriv::SkRightWord : GRichTextPriv::SkRightChar,
								k.Shift());
					}
				}
				return true;
				break;
			}
			case VK_UP:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageUp;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkUpLine,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_DOWN:
			{
				if (k.Alt())
					return false;

				if (k.Down())
				{
					#ifdef MAC
					if (k.Ctrl())
						goto GTextView4_PageDown;
					#endif

					d->Seek(d->Cursor,
							GRichTextPriv::SkDownLine,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_END:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_EndOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocEnd : GRichTextPriv::SkLineEnd,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_HOME:
			{
				if (k.Down())
				{
					#ifdef MAC
					if (!k.Ctrl())
						Jump_StartOfLine:
					#endif

					d->Seek(d->Cursor,
							k.Ctrl() ? GRichTextPriv::SkDocStart : GRichTextPriv::SkLineStart,
							k.Shift());

					/*
					char16 *Line = Text + l->Start;
					char16 *s;
					char16 SpTab[] = {' ', '\t', 0};
					for (s = Line; (SubtractPtr(s,Line) < l->Len) && StrchrW(SpTab, *s); s++);
					int Whitespace = SubtractPtr(s, Line);

					if (l->Start + Whitespace == Cursor)
					{
						SetCursor(l->Start, k.Shift());
					}
					else
					{
						SetCursor(l->Start + Whitespace, k.Shift());
					}
					*/
				}
				return true;
				break;
			}
			case VK_PAGEUP:
			{
				#ifdef MAC
				GTextView4_PageUp:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkUpPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_PAGEDOWN:
			{
				#ifdef MAC
				GTextView4_PageDown:
				#endif
				if (k.Down())
				{
					d->Seek(d->Cursor,
							GRichTextPriv::SkDownPage,
							k.Shift());
				}
				return true;
				break;
			}
			case VK_INSERT:
			{
				if (k.Down())
				{
					if (k.Ctrl())
					{
						Copy();
					}
					else if (k.Shift())
					{
						if (!GetReadOnly())
						{
							Paste();
						}
					}
				}
				return true;
				break;
			}
			case VK_DELETE:
			{
				if (!GetReadOnly())
				{
					if (k.Down())
					{
						GRichTextPriv::Block *b;
						if (HasSelection())
						{
							if (k.Shift())
								Cut();
							else
								DeleteSelection();
						}
						else if (d->Cursor &&
								 (b = d->Cursor->Blk))
						{
							if (d->Cursor->Offset < b->Length() - 1)
							{
								if (d->Cursor->Blk->DeleteAt(d->Cursor->Offset, 1))
									Invalidate();
							}
							else
							{
								LgiTrace("%s:%i - Impl deleting char from next block\n", _FL);
							}
						}
					}
					return true;
				}
				break;
			}
			default:
			{
				if (k.c16 == 17) break;

				if (k.Modifier() &&
					!k.Alt())
				{
					switch (k.GetChar())
					{
						case 0xbd: // Ctrl+'-'
						{
							/*
							if (k.Down() &&
								Font->PointSize() > 1)
							{
								Font->PointSize(Font->PointSize() - 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 0xbb: // Ctrl+'+'
						{
							/*
							if (k.Down() &&
								Font->PointSize() < 100)
							{
								Font->PointSize(Font->PointSize() + 1);
								OnFontChange();
								Invalidate();
							}
							*/
							break;
						}
						case 'a':
						case 'A':
						{
							if (k.Down())
							{
								// select all
								/*
								SelStart = 0;
								SelEnd = Size;
								Invalidate();
								*/
							}
							return true;
							break;
						}
						case 'y':
						case 'Y':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Redo();
								}
								return true;
							}
							break;
						}
						case 'z':
						case 'Z':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									if (k.Shift())
									{
										Redo();
									}
									else
									{
										Undo();
									}
								}
								return true;
							}
							break;
						}
						case 'x':
						case 'X':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									Cut();
								}
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Shift())
								return false;
							
							if (k.Down())
								Copy();
							return true;
							break;
						}
						case 'v':
						case 'V':
						{
							if (!k.Shift() &&
								!GetReadOnly())
							{
								if (k.Down())
								{
									Paste();
								}
								return true;
							}
							break;
						}
						case 'f':
						{
							if (k.Down())
							{
								DoFind();
							}
							return true;
							break;
						}
						case 'g':
						case 'G':
						{
							if (k.Down())
							{
								DoGoto();
							}
							return true;
							break;
						}
						case 'h':
						case 'H':
						{
							if (k.Down())
							{
								DoReplace();
							}
							return true;
							break;
						}
						case 'u':
						case 'U':
						{
							if (!GetReadOnly())
							{
								if (k.Down())
								{
									DoCase(k.Shift());
								}
								return true;
							}
							break;
						}
						case VK_RETURN:
						{
							if (!GetReadOnly() && !k.Shift())
							{
								if (k.Down())
								{
									OnEnter(k);
								}
								return true;
							}
							break;
						}
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void GRichTextEdit::OnEnter(GKey &k)
{
	// enter
	/*
	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	char16 InsertStr[256] = {'\n', 0};

	GTextLine *CurLine = GetTextLine(Cursor);
	if (CurLine && AutoIndent)
	{
		int WsLen = 0;
		for (;	WsLen < CurLine->Len &&
				WsLen < (Cursor - CurLine->Start) &&
				strchr(" \t", Text[CurLine->Start + WsLen]); WsLen++);
		if (WsLen > 0)
		{
			memcpy(InsertStr+1, Text+CurLine->Start, WsLen * sizeof(char16));
			InsertStr[WsLen+1] = 0;
		}
	}

	if (Insert(Cursor, InsertStr, StrlenW(InsertStr)))
	{
		SetCursor(Cursor + StrlenW(InsertStr), false, true);
	}
	*/
}

void GRichTextEdit::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	pDC->Colour(colour);
	pDC->Rectangle(&r);
}

void GRichTextEdit::OnPaint(GSurface *pDC)
{
	pDC->Colour(
		#if 0 // def _DEBUG
		GColour(255, 222, 255)
		#else
		GColour(LC_WORKSPACE, 24)
		#endif
		);
	pDC->Rectangle();
	
	GRect r = GetClient();
	GCssTools ct(d, d->Font);
	r = ct.PaintBorderAndPadding(pDC, r);

	d->Layout(r);
	d->Paint(pDC);
}

GMessage::Result GRichTextEdit::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CUT:
		{
			Cut();
			break;
		}
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_PASTE:
		{
			Paste();
			break;
		}
		#if defined WIN32
		case WM_GETTEXTLENGTH:
		{
			return 0 /*Size*/;
		}
		case WM_GETTEXT:
		{
			int Chars = (int)MsgA(Msg);
			char *Out = (char*)MsgB(Msg);
			if (Out)
			{
				char *In = (char*)LgiNewConvertCp(LgiAnsiToLgiCp(), NameW(), LGI_WideCharset, Chars);
				if (In)
				{
					int Len = (int)strlen(In);
					memcpy(Out, In, Len);
					DeleteArray(In);
					return Len;
				}
			}
			return 0;
		}

		/* This is broken... the IME returns garbage in the buffer. :(
		case WM_IME_COMPOSITION:
		{
			if (Msg->b & GCS_RESULTSTR) 
			{
				HIMC hIMC = ImmGetContext(Handle());
				if (hIMC)
				{
					int Size = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
					char *Buf = new char[Size];
					if (Buf)
					{
						ImmGetCompositionString(hIMC, GCS_RESULTSTR, Buf, Size);

						char16 *Utf = (char16*)LgiNewConvertCp(LGI_WideCharset, Buf, LgiAnsiToLgiCp(), Size);
						if (Utf)
						{
							Insert(Cursor, Utf, StrlenW(Utf));
							DeleteArray(Utf);
						}

						DeleteArray(Buf);
					}

					ImmReleaseContext(Handle(), hIMC);
				}
				return 0;
			}
			break;
		}
		*/
		#endif
	}

	return GLayout::OnEvent(Msg);
}

int GRichTextEdit::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl->GetId() == IDC_VSCROLL && VScroll)
	{
		Invalidate();
	}

	return 0;
}

void GRichTextEdit::OnPulse()
{
	if (!ReadOnly)
	{
		/*
		Blink = !Blink;

		GRect p = CursorPos;
		p.Offset(-ScrollX, 0);
		Invalidate(&p);
		*/
	}
}

void GRichTextEdit::OnUrl(char *Url)
{
	if (Environment)
	{
		Environment->OnNavigate(this, Url);
	}
}

bool GRichTextEdit::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = 32;
	Inf.Width.Max = -1;

	// Inf.Height.Min = (Font ? Font->GetHeight() : 18) + 4;
	Inf.Height.Max = -1;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
class GRichTextEdit_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GRichTextEdit") == 0)
		{
			return new GRichTextEdit(-1, 0, 0, 2000, 2000);
		}

		return 0;
	}
} RichTextEdit_Factory;