// override.cpp - キレイなTextOut
// 2006/09/27

#include "override.h"

#include <malloc.h>		// _alloca
#include <mbctype.h>	// _getmbcp

const UINT g_Quality = 2;
const UINT g_Weight = 0;
const UINT g_Enhance = 0;
const UINT g_UseSubPixel = 0;
const UINT g_SubPixelDirection = 0;
const float g_MaxHeight = 0;
const int  g_Scale = 3;
UINT g_ForceClearType = 0;


struct {
	HDC	hdc;
	HBITMAP	hbmp;
	BYTE*	lpPixels;
	SIZE	dibSize;
	CRITICAL_SECTION  cs;
} g_Cache = { NULL,NULL,NULL, {0,0},{0} };

struct CacheLock {
	CacheLock()  { EnterCriticalSection(&g_Cache.cs); }
	~CacheLock() { LeaveCriticalSection(&g_Cache.cs); }
};

void  CacheInit()
{
	InitializeCriticalSection(&g_Cache.cs);
}

void  CacheTerm()
{
	if(g_Cache.hdc) DeleteDC(g_Cache.hdc);
	if(g_Cache.hbmp) DeleteObject(g_Cache.hbmp);
	g_Cache.hdc = NULL;
	g_Cache.hbmp = NULL;
	g_Cache.lpPixels = NULL;
	DeleteCriticalSection(&g_Cache.cs);
}

void CacheFillSolidRect(COLORREF rgb, const RECT*)
{
	rgb = RGB(GetBValue(rgb), GetGValue(rgb), GetRValue(rgb));
	DWORD* p = (DWORD*)g_Cache.lpPixels;

	unsigned long* pend = (unsigned long*)p + (g_Cache.dibSize.cx * g_Cache.dibSize.cy);

	while((unsigned long*)p < pend)
		*p++ = rgb;
}


BOOL IsFontExcluded(const LPCWSTR lpszFaceName) {
	if (lpszFaceName && _wcsicmp(lpszFaceName, L"Myriad Pro Cond") == 0) {
		return FALSE;
	}
	return TRUE;
}

// 有効なDCかどうかをチェックする
BOOL IsValidDC(const HDC hdc){
	if (GetDeviceCaps(hdc, TECHNOLOGY) != DT_RASDISPLAY) return FALSE;
	// ここでフォントチェックも行う
	WCHAR szFaceName[LF_FACESIZE] = L"";
	GetTextFaceW(hdc, LF_FACESIZE, szFaceName);
	if (IsFontExcluded(szFaceName)) return FALSE;
	return TRUE;
}


// こんな感じなら コンパイラの最適化＋CPUの先読み実行で何とかならんかのぉ？
// LookupTablex3はとりあえず止め。後でベンチしてみる。
// (2006/09/22) サブピクセルレンダリングを元に戻してみた
//              ウィンドウを重ねたときなどに出る色化けはあいかわらず...
void  ScaleDIB(BYTE* lpPixels, int width, int height)
{
	int  srcWidthBytes = g_Cache.dibSize.cx * 4;
	int  srcSkipBytes  = srcWidthBytes - (width*4);
	int  dstSkipBytes  = srcWidthBytes - (width*4 / g_Scale);

	register BYTE* dst  = lpPixels;
	register BYTE* src0 = lpPixels;
	width  /= g_Scale;
	height /= g_Scale;

	if(g_Scale == 4) {
		for(int y=0 ; y < height ; y++) {
			register BYTE* src1 = src0 + srcWidthBytes;
			register BYTE* src2 = src1 + srcWidthBytes;
			register BYTE* src3 = src2 + srcWidthBytes;
			for(int x=0 ; x < width ; x++) {
				dst[0]=(src0[0] + src0[4] + src0[8] + src0[12] +
					src1[0] + src1[4] + src1[8] + src1[12] +
					src2[0] + src2[4] + src2[8] + src2[12] +
					src3[0] + src3[4] + src3[8] + src3[12]) >> 4;
				dst[1]=(src0[1] + src0[5] + src0[9] + src0[13] +
					src1[1] + src1[5] + src1[9] + src1[13] +
					src2[1] + src2[5] + src2[9] + src2[13] +
					src3[1] + src3[5] + src3[9] + src3[13]) >> 4;
				dst[2]=(src0[2] + src0[6] + src0[10] + src0[14] +
					src1[2] + src1[6] + src1[10] + src1[14] +
					src2[2] + src2[6] + src2[10] + src2[14] +
					src3[2] + src3[6] + src3[10] + src3[14]) >> 4;
				dst+=4; src0+=4*4; src1+=4*4; src2+=4*4; src3+=4*4;
			}
			dst += dstSkipBytes;
			src0 = src3 + srcSkipBytes;
		}
	}
	else if(g_Scale == 3) {
		if (g_UseSubPixel==0) {
			for(int y=0 ; y < height ; y++) {
				// Don't use subpixels
				register BYTE* src1 = src0 + srcWidthBytes;
				register BYTE* src2 = src1 + srcWidthBytes;
				for(int x=0 ; x < width; x++) {
					dst[0]=(src0[0] + src0[4] + src0[8] +
						src1[0] + src1[4] + src1[8] +
						src2[0] + src2[4] + src2[8]) / 9;
					dst[1]=(src0[1] + src0[5] + src0[9] +
						src1[1] + src1[5] + src1[9] +
						src2[1] + src2[5] + src2[9]) / 9;
					dst[2]=(src0[2] + src0[6] + src0[10] +
						src1[2] + src1[6] + src1[10] +
						src2[2] + src2[6] + src2[10]) / 9;
					dst+=4; src0+=4*3; src1+=4*3; src2+=4*3;
				}
				dst += dstSkipBytes;
				src0 = src2 + srcSkipBytes;
			}
		} else {
			if (g_SubPixelDirection==0) {
				for(int y=0 ; y < height ; y++) {
					// RGB
					register BYTE* src1 = src0 + srcWidthBytes;
					register BYTE* src2 = src1 + srcWidthBytes;
					dst[0]=(src0[0] + src0[4] + src0[8] +
						src1[0] + src1[4] + src1[8] +
						src2[0] + src2[4] + src2[8]) / 9;
					dst[1]=(src0[1] + src0[5] + src0[9] +
						src1[1] + src1[5] + src1[9] +
						src2[1] + src2[5] + src2[9]) / 9;
					dst[2]=(src0[2] + src0[6] + src0[10] +
						src1[2] + src1[6] + src1[10] +
						src2[2] + src2[6] + src2[10]) / 9;
					dst+=4;src0+=4*3; src1+=4*3; src2+=4*3;
					for(int x=1 ; x < width-1; x++) {
						dst[0]=(src0[8] + src0[12] + src0[16] +
							src1[8] + src1[12] + src1[16] +
							src2[8] + src2[12] + src2[16]) / 9;
						dst[1]=(src0[5] + src0[9] + src0[13] +
							src1[5] + src1[9] + src1[13] +
							src2[5] + src2[9] + src2[13]) / 9;
						dst[2]=(src0[2] + src0[6] + src0[10] +
							src1[2] + src1[6] + src1[10] +
							src2[2] + src2[6] + src2[10]) / 9;
						dst+=4; src0+=4*3; src1+=4*3; src2+=4*3;
					}
					dst[0]=(src0[0] + src0[4] + src0[8] +
						src1[0] + src1[4] + src1[8] +
						src2[0] + src2[4] + src2[8]) / 9;
					dst[1]=(src0[1] + src0[5] + src0[9] +
						src1[1] + src1[5] + src1[9] +
						src2[1] + src2[5] + src2[9]) / 9;
					dst[2]=(src0[2] + src0[6] + src0[10] +
						src1[2] + src1[6] + src1[10] +
						src2[2] + src2[6] + src2[10]) / 9;
					//dst+=4; src0+=4*3; src1+=4*3; src2+=4*3;
					dst += dstSkipBytes + 4;
					src0 = src2 + (4*3) + srcSkipBytes;
				}
			} else {
				for(int y=0 ; y < height ; y++) {
					// BGR
					register BYTE* src1 = src0 + srcWidthBytes;
					register BYTE* src2 = src1 + srcWidthBytes;
					dst[0]=(src0[0] + src0[4] + src0[8] +
						src1[0] + src1[4] + src1[8] +
						src2[0] + src2[4] + src2[8]) / 9;
					dst[1]=(src0[1] + src0[5] + src0[9] +
						src1[1] + src1[5] + src1[9] +
						src2[1] + src2[5] + src2[9]) / 9;
					dst[2]=(src0[2] + src0[6] + src0[10] +
						src1[2] + src1[6] + src1[10] +
						src2[2] + src2[6] + src2[10]) / 9;
					dst+=4;src0+=4*3; src1+=4*3; src2+=4*3;
					for(int x=1 ; x < width-1; x++) {
						dst[0]=(src0[0] + src0[4] + src0[8] +
							src1[0] + src1[4] + src1[8] +
							src2[0] + src2[4] + src2[8]) / 9;
						dst[1]=(src0[5] + src0[9] + src0[13] +
							src1[5] + src1[9] + src1[13] +
							src2[5] + src2[9] + src2[13]) / 9;
						dst[2]=(src0[10] + src0[14] + src0[18] +
							src1[10] + src1[14] + src1[18] +
							src2[10] + src2[14] + src2[18]) / 9;
						dst+=4; src0+=4*3; src1+=4*3; src2+=4*3;
					}
					dst[0]=(src0[0] + src0[4] + src0[8] +
						src1[0] + src1[4] + src1[8] +
						src2[0] + src2[4] + src2[8]) / 9;
					dst[1]=(src0[1] + src0[5] + src0[9] +
						src1[1] + src1[5] + src1[9] +
						src2[1] + src2[5] + src2[9]) / 9;
					dst[2]=(src0[2] + src0[6] + src0[10] +
						src1[2] + src1[6] + src1[10] +
						src2[2] + src2[6] + src2[10]) / 9;
					//dst+=4; src0+=4*3; src1+=4*3; src2+=4*3;
					dst += dstSkipBytes + 4;
					src0 = src2 + (4*3) + srcSkipBytes;
				}
			}
		}
	}
	else if(g_Scale == 2) {
		for(int y=0 ; y < height ; y++) {
			register BYTE* src1 = src0 + srcWidthBytes;
			for(int x=0 ; x < width ; x++) {
				dst[0]=(src0[0] + src0[4] +
					src1[0] + src1[4]) >> 2;
				dst[1]=(src0[1] + src0[5] +
					src1[1] + src1[5]) >> 2;
				dst[2]=(src0[2] + src0[6] +
					src1[2] + src1[6]) >> 2;
				dst+=4; src0+=4*2; src1+=4*2;
			}
			dst += dstSkipBytes;
			src0 = src1 + srcSkipBytes;
		}
	}
}


HDC  CreateCacheDC()
{
	if(!g_Cache.hdc)
		g_Cache.hdc = CreateCompatibleDC(NULL);
	return g_Cache.hdc;
}

HBITMAP  CreateCacheDIB(HDC hdc, int width, int height, BYTE** lpPixels)
{
	if(g_Cache.dibSize.cx < width || g_Cache.dibSize.cy < height) {
		if(g_Cache.hbmp)
			DeleteObject(g_Cache.hbmp);

		if(g_Cache.dibSize.cx < width)
			g_Cache.dibSize.cx = width;
		if(g_Cache.dibSize.cy < height)
			g_Cache.dibSize.cy = height;

		BITMAPINFOHEADER bmiHeader;
		bmiHeader.biSize = sizeof(bmiHeader);
		bmiHeader.biWidth = g_Cache.dibSize.cx;
		bmiHeader.biHeight = -g_Cache.dibSize.cy;
		bmiHeader.biPlanes = 1;
		bmiHeader.biBitCount = 32;
		bmiHeader.biCompression = BI_RGB;
		bmiHeader.biSizeImage = 0;
		bmiHeader.biXPelsPerMeter = 0;
		bmiHeader.biYPelsPerMeter = 0;
		bmiHeader.biClrUsed = 0;
		bmiHeader.biClrImportant = 0;
		g_Cache.hbmp = CreateDIBSection(hdc, (CONST BITMAPINFO*)&bmiHeader, DIB_RGB_COLORS, (LPVOID*)&g_Cache.lpPixels, NULL, 0);
	}
	*lpPixels = g_Cache.lpPixels;
	return g_Cache.hbmp;
}


BOOL WINAPI IMPL_GetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int cbString, LPSIZE lpSize)
{
	if ((g_Quality == 0) || (!IsValidDC(hdc)))
		return ORIG_GetTextExtentPoint32W(hdc, lpString, cbString, lpSize);

	int grmode = GetGraphicsMode(hdc);
	if(grmode != GM_ADVANCED)
		SetGraphicsMode(hdc, GM_ADVANCED);
	XFORM xformOrig;
	GetWorldTransform(hdc, &xformOrig);
	BOOL b = FALSE;
	if (xformOrig.eM12 < -0.1f || 0.1f < xformOrig.eM12 || xformOrig.eM21 < -0.1f || 0.1f < xformOrig.eM21) {
		b = ORIG_GetTextExtentPoint32W(hdc, lpString, cbString, lpSize);
	} else {
		XFORM xformScale = {
			xformOrig.eM11 * g_Scale, xformOrig.eM12 * g_Scale,
			xformOrig.eM21 * g_Scale, xformOrig.eM22 * g_Scale,
			xformOrig.eDx  * g_Scale, xformOrig.eDy  * g_Scale
		};
		SetWorldTransform(hdc, &xformScale);
		b = ORIG_GetTextExtentPoint32W(hdc, lpString, cbString, lpSize);
		SetWorldTransform(hdc, &xformOrig);
	}
	if(grmode != GM_ADVANCED)
		SetGraphicsMode(hdc, grmode);
	return b;
}


int div_ceil(int a, int b)
{
	if(a % b)
		return (a>0)? (a/b+1): (a/b-1);
	return a / b;
}

HFONT WINAPI IMPL_CreateFontW(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCWSTR lpszFace)
{
	DWORD fdwNewQuality = fdwQuality;
	if ((g_Quality > 0) && (!IsFontExcluded(lpszFace)))
		fdwNewQuality = ANTIALIASED_QUALITY;
	else
		fdwNewQuality = g_ForceClearType ? CLEARTYPE_NATURAL_QUALITY : ANTIALIASED_QUALITY;

	return ORIG_CreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwNewQuality, fdwPitchAndFamily, lpszFace);
}

HFONT WINAPI IMPL_CreateFontIndirectW(CONST LOGFONTW *lplf)
{
	LOGFONTW lf = *lplf;
	if ((g_Quality > 0) && !IsFontExcluded(lplf->lfFaceName)) {
		lf.lfQuality = ANTIALIASED_QUALITY;
	}
	else {
		lf.lfQuality = g_ForceClearType ? CLEARTYPE_NATURAL_QUALITY : ANTIALIASED_QUALITY;
	}
	return ORIG_CreateFontIndirectW(&lf);
}


BOOL WINAPI IMPL_ExtTextOutW(HDC hdc, int nXStart, int nYStart, UINT fuOptions, CONST RECT *lprc, LPCWSTR lpString, UINT cbString, CONST INT *lpDx)
{
	CacheLock  __lock;
	int   error = 0;

	POINT curPos = { nXStart, nYStart };
	POINT destPos;
	SIZE  destSize;
	POINT canvasPos;
	SIZE  canvasSize;

	HDC     hCanvasDC = NULL;
	HGDIOBJ hPrevFont = NULL;
	HBITMAP hBmp = NULL;
	BYTE*   lpPixels = NULL;

	XFORM  xformNoTrns;
	XFORM  xformScale = {1.0f,0.0f,  0.0f,1.0f,  0.0f,0.0f};
	UINT   align;
	SIZE   textSize;

	if(lpString == NULL || cbString == 0 || g_Quality==0) {
		error = 11;//throw
	}
	if(!error) {
		if(!IsValidDC(hdc))
			error = 14;	// hdc is invalid
	}
	if(!error) {
		hCanvasDC = CreateCacheDC();
		if(!hCanvasDC)
			error = 1;
	}
	// get cursor / xform
	if(!error) {
		align = GetTextAlign(hdc);
		if(align & TA_UPDATECP) {
			GetCurrentPositionEx(hdc, &curPos);
		}

		GetWorldTransform(hdc, &xformNoTrns);
		if(xformNoTrns.eM12 < -0.1f || 0.1f < xformNoTrns.eM12 ||
		   xformNoTrns.eM21 < -0.1f || 0.1f < xformNoTrns.eM21) {
			error = 12;// throw rotation
		}
		xformNoTrns.eDx = 0.0f; //clear translation
		xformNoTrns.eDy = 0.0f;
		xformScale.eM11 = xformNoTrns.eM11 * g_Scale;//scaling
		xformScale.eM22 = xformNoTrns.eM22 * g_Scale;
		SetGraphicsMode(hCanvasDC, GM_ADVANCED);
		SetWorldTransform(hCanvasDC, &xformScale);
	}
	//copy font
	if(!error) {
		hPrevFont = SelectObject(hCanvasDC, GetCurrentObject(hdc, OBJ_FONT));
	}

	SetTextCharacterExtra(hCanvasDC, GetTextCharacterExtra(hdc));
	//text size
	if(!error) {
		if(fuOptions & ETO_GLYPH_INDEX)
			GetTextExtentPointI(hCanvasDC, (LPWORD)lpString, (int)cbString, &textSize);
		else
			GetTextExtentExPointW(hCanvasDC, lpString, (int)cbString, 0, NULL, NULL, &textSize);

		if(lpDx) {
			textSize.cx = 0;
			if(fuOptions & ETO_PDY)
				for(UINT i=0; i < cbString; textSize.cx += lpDx[i],i+=2);
			else
				for(UINT i=0; i < cbString; textSize.cx += lpDx[i++]);
		}

		TEXTMETRIC tm;
		GetTextMetrics(hdc, &tm);
		if (tm.tmItalic) {
			ABC abcWidth={0, 0, 0};
			GetCharABCWidthsW(hdc, lpString[cbString-1], lpString[cbString-1], &abcWidth);
			textSize.cx += tm.tmOverhang - abcWidth.abcC;
		}

		//if(textSize.cy * xformNoTrns.eM22 >= 24.0f) //max 24pixel
		if (g_MaxHeight > 0) {
			if(textSize.cy * xformNoTrns.eM22 >= g_MaxHeight) //max 26pixel
				error = 13;//throw large size
		}
	}

	if(!error) {
		LOGFONT lf;
		GetObject(GetCurrentObject(hdc, OBJ_FONT),sizeof(LOGFONT), &lf);

		if (lf.lfEscapement != 0)
			error = 15;// rotated font
	}

	//rectangle
	if(!error) {
		RECT rc;
		UINT horiz = align & (TA_LEFT|TA_RIGHT|TA_CENTER);
		UINT vert  = align & (TA_BASELINE|TA_TOP);//TA_BOTTOM
		if(horiz == TA_CENTER) {
			rc.left  = curPos.x - div_ceil(textSize.cx, 2);
			rc.right = curPos.x + div_ceil(textSize.cx, 2);
			//no move
		}
		else if(horiz == TA_RIGHT) {
			rc.left  = curPos.x - textSize.cx;
			rc.right = curPos.x;
			curPos.x -= textSize.cx;//move pos
		}
		else {
			rc.left  = curPos.x;
			rc.right = curPos.x + textSize.cx;
			curPos.x += textSize.cx;//move pos
		}
		if(vert == TA_BASELINE) {
			TEXTMETRIC metric = {0};
			GetTextMetrics(hCanvasDC, &metric);
			rc.top = curPos.y - metric.tmAscent;
			rc.bottom = curPos.y + metric.tmDescent;
		}
		else {
			rc.top = curPos.y;
			rc.bottom = curPos.y + textSize.cy;
		}

		canvasPos.x = canvasPos.y = 0;

		//clipping
		if(lprc && (fuOptions & ETO_CLIPPED)) {
			if(rc.left < lprc->left) {
				canvasPos.x -= div_ceil( (int)((lprc->left - rc.left) * xformScale.eM11), g_Scale);
				rc.left = lprc->left;
			}
			if(rc.right > lprc->right) {
				if(horiz == TA_RIGHT)
					canvasPos.x -= div_ceil( (int)((rc.right - lprc->right) * xformScale.eM11), g_Scale);
				rc.right = lprc->right;
			}
			if(rc.top < lprc->top) {
				canvasPos.y -= div_ceil( (int)((lprc->top - rc.top) * xformScale.eM22), g_Scale);
				rc.top = lprc->top;
			}
			if(rc.bottom > lprc->bottom) {
				rc.bottom = lprc->bottom;
			}
		}

		destPos.x = rc.left;
		destPos.y = rc.top;
		destSize.cx = rc.right - rc.left;
		destSize.cy = rc.bottom - rc.top;
		canvasSize.cx = (int)(destSize.cx * xformScale.eM11);
		canvasSize.cy = (int)(destSize.cy * xformScale.eM22);

		if(destSize.cx < 1 || destSize.cy < 1)
			error = 14; //throw no area
	}
	//bitmap
	if(!error) {
		hBmp = CreateCacheDIB(hCanvasDC, canvasSize.cx, canvasSize.cy, &lpPixels);
		if(!hBmp)
			error = 3;
	}
	if(!error) {
		HGDIOBJ hPrevBmp = SelectObject(hCanvasDC, hBmp);

		BOOL fillrect = (lprc && (fuOptions & ETO_OPAQUE)) ? TRUE : FALSE;

		//clear bitmap
		if(fillrect || GetBkMode(hdc) == OPAQUE) {
			SetBkMode(hCanvasDC, OPAQUE);
			COLORREF  bgcolor = GetBkColor(hdc);
			SetBkColor(hCanvasDC, bgcolor);

			RECT rc = { 0, 0, canvasSize.cx, canvasSize.cy };
			CacheFillSolidRect(bgcolor, &rc);

			if(fillrect) {
				ORIG_ExtTextOutW(hdc, 0,0, ETO_OPAQUE, lprc, NULL, 0, NULL);
			}
		}
		else {
			SetBkMode(hCanvasDC, TRANSPARENT);
			BitBlt(hCanvasDC,0,0,destSize.cx,destSize.cy, hdc,destPos.x,destPos.y, SRCCOPY);
		}

		//setup
		SetTextColor(hCanvasDC, GetTextColor(hdc));
		SetTextAlign(hCanvasDC, TA_LEFT | TA_TOP);

		//textout
		for (UINT w=0; w<=g_Weight; w++) ORIG_ExtTextOutW(hCanvasDC, canvasPos.x, canvasPos.y, fuOptions, NULL, lpString, cbString, lpDx);

		//scale
		ScaleDIB(lpPixels, canvasSize.cx, canvasSize.cy);

		//blt
		SetWorldTransform(hCanvasDC, &xformNoTrns);
		BitBlt(hdc, destPos.x, destPos.y, destSize.cx, destSize.cy, hCanvasDC, 0,0, SRCCOPY);

		SelectObject(hCanvasDC, hPrevBmp);

		if(align & TA_UPDATECP) {
			MoveToEx(hdc, curPos.x, curPos.y, NULL);
		}
	}

	//if(hBmp) DeleteObject(hBmp);
	if(hPrevFont) SelectObject(hCanvasDC, hPrevFont);
	//if(hCanvasDC) DeleteDC(hCanvasDC);

	if(!error)
		return TRUE;
	return ORIG_ExtTextOutW(hdc, nXStart, nYStart, fuOptions, lprc, lpString, cbString, lpDx);
}

//EOF

