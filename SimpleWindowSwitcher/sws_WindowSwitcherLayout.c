#include "sws_WindowSwitcherLayout.h"

static BOOL CALLBACK _sws_WindowSwitcherLayout_EnumWindowsCallback(_In_ HWND hWnd, _In_ sws_WindowSwitcherLayout* _this)
{
	if (sws_WindowHelpers_IsAltTabWindow(hWnd) || (_this->bIncludeWallpaper && !_this->bWallpaperAlwaysLast && hWnd == _this->hWndWallpaper))
	{
		sws_WindowSwitcherLayoutWindow swsLayoutWindow;
		sws_WindowSwitcherLayoutWindow_Initialize(&swsLayoutWindow, hWnd, NULL);
		sws_vector_PushBack(&_this->pWindowList, &swsLayoutWindow);
		DWORD band;
		_sws_GetWindowBand(hWnd, &band);
		if (band != ZBID_DESKTOP) _this->numTopMost++;
	}

	return TRUE;
}

sws_error_t sws_WindowSwitcherLayout_InvalidateLayout(sws_WindowSwitcherLayout* _this)
{
	sws_error_t rv = SWS_ERROR_SUCCESS;

	sws_WindowSwitcherLayoutWindow* pWindowList = _this->pWindowList.pList;
	for (int iCurrentWindow = _this->pWindowList.cbSize - 1; iCurrentWindow >= 0; iCurrentWindow--)
	{
		sws_WindowSwitcherLayoutWindow_Erase(&(pWindowList[iCurrentWindow]));
	}
}

static inline unsigned int _sws_WindowSwitcherLayout_GetInitialLeft(sws_WindowSwitcherLayout* _this)
{
	return _this->cbElementLeftPadding + _this->cbLeftPadding;
}

static inline unsigned int _sws_WindowSwitcherLayout_GetRightIncrement(sws_WindowSwitcherLayout* _this)
{
	return (_this->cbRightPadding + _this->cbElementRightPadding) + _sws_WindowSwitcherLayout_GetInitialLeft(_this);
}

static inline unsigned int _sws_WindowSwitcherLayout_GetInitialTop(sws_WindowSwitcherLayout* _this)
{
	return _this->cbElementTopPadding + (_this->cbTopPadding + _this->cbRowTitleHeight + _this->cbVDividerPadding);
}

static inline unsigned int _sws_WindowSwitcherLayout_GetBottomIncrement(sws_WindowSwitcherLayout* _this)
{
	return (_this->cbThumbnailAvailableHeight + _this->cbBottomPadding) + _this->cbElementBottomPadding + _sws_WindowSwitcherLayout_GetInitialTop(_this);
}

sws_error_t sws_WindowSwitcherLayout_ComputeLayout(sws_WindowSwitcherLayout* _this, int direction, HWND hTarget)
{
	sws_error_t rv = SWS_ERROR_SUCCESS;

	if (!rv)
	{
		unsigned int cbInitialLeft = _sws_WindowSwitcherLayout_GetInitialLeft(_this) + _this->cbMasterLeftPadding;
		unsigned int cbInitialTop = _sws_WindowSwitcherLayout_GetInitialTop(_this) + _this->cbMasterTopPadding;
		unsigned int cbCurrentLeft = cbInitialLeft, cbCurrentTop = cbInitialTop;
		unsigned int cbMaxWidthHit = 0, cbMaxWidth = 0;

		int iObtainedIndex = 0;

		sws_WindowSwitcherLayoutWindow* pWindowList = _this->pWindowList.pList;

		if (_this->iWidth)
		{
			cbMaxWidth = _this->iWidth; // -_this->cbPadding; !!!!!
		}
		else
		{
			cbMaxWidth = _this->cbMaxWidth;
		}

		BOOL bHasTarget = FALSE;
		int iTarget = -1;

		if (direction != SWS_WINDOWSWITCHERLAYOUT_COMPUTE_DIRECTION_INITIAL)
		{
			if (direction == SWS_WINDOWSWITCHERLAYOUT_COMPUTE_DIRECTION_BACKWARD)
			{
				bHasTarget = TRUE;
				iObtainedIndex = _this->pWindowList.cbSize - 1;
			}
			else if (direction == SWS_WINDOWSWITCHERLAYOUT_COMPUTE_DIRECTION_FORWARD)
			{
				if (_this->iIndex == _this->pWindowList.cbSize - 1)
				{
					iObtainedIndex = _this->iIndex;
				}
				else
				{
					int i = 0, state = 0, h = 0;
					for (i = _this->pWindowList.cbSize - 1; i >= 0; i--)
					{
						if (state == 0)
						{
							if (pWindowList[i].hThumbnail)
							{
								h = pWindowList[i].rcThumbnail.top;
								state = 1;
							}
						}
						else if (state == 1)
						{
							if (h != pWindowList[i].rcThumbnail.top)
							{
								break;
							}
						}
					}
					iObtainedIndex = i;
				}
			}
			sws_WindowSwitcherLayout_InvalidateLayout(_this);
		}

		BOOL bIsWidthComputed = (_this->iWidth != 0);
		BOOL bFinishedLayout = FALSE;

		while (1)
		{
			int iCurrentCount = 0;

			cbInitialLeft = _sws_WindowSwitcherLayout_GetInitialLeft(_this) + _this->cbMasterLeftPadding;
			cbInitialTop = _sws_WindowSwitcherLayout_GetInitialTop(_this) + _this->cbMasterTopPadding;
			cbCurrentLeft = cbInitialLeft;
			cbCurrentTop = cbInitialTop;
			//cbMaxWidthHit = 0, cbMaxWidth = 0; // remove this so the maximum width hit is preserved across hwnd searches or backward iterations

			for (int iCurrentWindow = iObtainedIndex ? iObtainedIndex : _this->iIndex; iCurrentWindow >= 0; iCurrentWindow--)
			{
				//TCHAR name[200];
				//GetWindowTextW(pWindowList[iCurrentWindow].hWnd, name, 200);
				//wprintf(L"%d %s ", pWindowList[iCurrentWindow].hWnd, name);

				if (pWindowList[iCurrentWindow].hWnd == _this->hWnd)
				{
					continue;
				}

				if (_this->iWidth)
				{
					cbMaxWidth = _this->iWidth; // +_this->cbPadding + _this->cbRightPadding; // ?????
				}
				else
				{
					cbMaxWidth = _this->cbMaxWidth;
				}

				if (!bFinishedLayout)
				{
					pWindowList[iCurrentWindow].iRowMax = -1;
				}

		        DwmRegisterThumbnail(
					_this->hWnd,
					pWindowList[iCurrentWindow].hWnd,
					&(pWindowList[iCurrentWindow].hThumbnail)
				);
				DwmQueryThumbnailSourceSize(pWindowList[iCurrentWindow].hThumbnail, &(pWindowList[iCurrentWindow].sizWindow));
				if ((pWindowList[iCurrentWindow].sizWindow.cy == 0) ||
					(pWindowList[iCurrentWindow].sizWindow.cx == 0))
				{
					// Fix for windows with no height or width not being displayed.
					pWindowList[iCurrentWindow].dwWindowFlags |= SWS_WINDOWSWITCHERLAYOUT_WINDOWFLAGS_ISEMPTY;
					pWindowList[iCurrentWindow].sizWindow.cy = SWS_WINDOWSWITCHERLAYOUT_EMPTYWINDOW_THUMBNAIL_HEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y);
					pWindowList[iCurrentWindow].sizWindow.cx = SWS_WINDOWSWITCHERLAYOUT_EMPTYWINDOW_THUMBNAIL_WIDTH * (_this->cbDpiX / DEFAULT_DPI_X);
				}
				if (pWindowList[iCurrentWindow].sizWindow.cy < SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINHEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y))
				{
					pWindowList[iCurrentWindow].dwWindowFlags |= SWS_WINDOWSWITCHERLAYOUT_WINDOWFLAGS_ISTOOSMALLVERTICAL;
					pWindowList[iCurrentWindow].sizWindow.cy = SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINHEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y);
				}
				if (pWindowList[iCurrentWindow].sizWindow.cx < SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINWIDTH * (_this->cbDpiX / DEFAULT_DPI_X))
				{
					pWindowList[iCurrentWindow].dwWindowFlags |= SWS_WINDOWSWITCHERLAYOUT_WINDOWFLAGS_ISTOOSMALLHORIZONTAL;
					pWindowList[iCurrentWindow].sizWindow.cx = SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINWIDTH * (_this->cbDpiX / DEFAULT_DPI_X);
				}
				if (bFinishedLayout)
				{
					DwmUnregisterThumbnail(pWindowList[iCurrentWindow].hThumbnail);
					pWindowList[iCurrentWindow].hThumbnail = 0;
				}

				unsigned int width = 0, original_width = 0;
				// original_width indicates that the window's width was too large, so we
				// reduced it and need to scale the height appropiately

				if (_this->bIncludeWallpaper && pWindowList[iCurrentWindow].hWnd == _this->hWndWallpaper)
				{
					width = ((_this->mi.rcMonitor.right - _this->mi.rcMonitor.left) *
						_this->cbThumbnailAvailableHeight) /
						(_this->mi.rcMonitor.bottom - _this->mi.rcMonitor.top);
				}
				else
				{
					width = ((pWindowList[iCurrentWindow].sizWindow.cx) *
						_this->cbThumbnailAvailableHeight) /
						(pWindowList[iCurrentWindow].sizWindow.cy);
				}
				if (width > _this->cbMaxTileWidth || width > pWindowList[iCurrentWindow].sizWindow.cx)
				{
					original_width = width;
					if (width > _this->cbMaxTileWidth)
					{
						width = _this->cbMaxTileWidth;
					}
					if (width > pWindowList[iCurrentWindow].sizWindow.cx)
					{
						width = pWindowList[iCurrentWindow].sizWindow.cx;
					}
				}

				if (bFinishedLayout)
				{
					pWindowList[iCurrentWindow].sizWindow.cx = 0;
					pWindowList[iCurrentWindow].sizWindow.cy = 0;
				}

				//wchar_t name[200];
				//GetWindowTextW(pWindowList[iCurrentWindow].hWnd, name, 200);
				//wprintf(L"%s %d %f %d\n", name, cbCurrentLeft, width, cbMaxWidth);

				BOOL bPreventSingleItemOnLastRow = FALSE;
				if (iCurrentWindow == 1)
				{
					HTHUMBNAIL hThumbnail = NULL;
					DwmRegisterThumbnail(
						_this->hWnd,
						pWindowList[0].hWnd,
						&hThumbnail
					);
					SIZE sz;
					sz.cx = 0;
					sz.cy = 0;
					if (hThumbnail)
					{
						DwmQueryThumbnailSourceSize(hThumbnail, &sz);
						DwmUnregisterThumbnail(hThumbnail);
					}
					if ((sz.cy == 0) || (sz.cx == 0))
					{
						sz.cy = SWS_WINDOWSWITCHERLAYOUT_EMPTYWINDOW_THUMBNAIL_HEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y);
						sz.cx = SWS_WINDOWSWITCHERLAYOUT_EMPTYWINDOW_THUMBNAIL_WIDTH * (_this->cbDpiX / DEFAULT_DPI_X);
					}
					if (sz.cy < SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINHEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y))
					{
						sz.cy = SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINHEIGHT * (_this->cbDpiY / DEFAULT_DPI_Y);
					}
					if (sz.cx < SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINWIDTH * (_this->cbDpiX / DEFAULT_DPI_X))
					{
						sz.cx = SWS_WINDOWSWITCHERLAYOUT_THUMBNAIL_MINWIDTH * (_this->cbDpiX / DEFAULT_DPI_X);
					}
					unsigned int next_width = 0;
					if (_this->bIncludeWallpaper && pWindowList[0].hWnd == _this->hWndWallpaper)
					{
						next_width = ((_this->mi.rcMonitor.right - _this->mi.rcMonitor.left) *
							_this->cbThumbnailAvailableHeight) /
							(_this->mi.rcMonitor.bottom - _this->mi.rcMonitor.top);
					}
					else
					{
						next_width = ((sz.cx) *
							_this->cbThumbnailAvailableHeight) /
							(sz.cy);
					}
					if (next_width > _this->cbMaxTileWidth || next_width > sz.cx)
					{
						if (next_width > _this->cbMaxTileWidth)
						{
							next_width = _this->cbMaxTileWidth;
						}
						if (next_width > sz.cx)
						{
							next_width = sz.cx;
						}
					}
					bPreventSingleItemOnLastRow = (cbCurrentLeft + width + next_width + _sws_WindowSwitcherLayout_GetRightIncrement(_this) * 2 - _sws_WindowSwitcherLayout_GetInitialLeft(_this)) > (cbMaxWidth - _this->cbMasterRightPadding);
				}

				//if (cbCurrentLeft + width + _this->cbRightPadding + _this->cbPadding > cbMaxWidth) !!!!!
				if (bPreventSingleItemOnLastRow ||
					(cbCurrentLeft + width + _sws_WindowSwitcherLayout_GetRightIncrement(_this) - _sws_WindowSwitcherLayout_GetInitialLeft(_this) > cbMaxWidth - _this->cbMasterRightPadding))
				{
					if (!bFinishedLayout)
					{
						int t = iCurrentWindow + 1;
						if (t == _this->pWindowList.cbSize) t = 0;
						for (int k = t; k < _this->pWindowList.cbSize; ++k)
						{
							if (pWindowList[k].iRowMax == -1)
							{
								pWindowList[k].iRowMax = cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this);
							}
							else
							{
								break;
							}
							//if (k == _this->pWindowList.cbSize - 1)
							//{
							//	k = -1;
							//}
						}
					}
					//if (cbCurrentLeft - _this->cbLeftPadding > cbMaxWidthHit) !!!!!
					if (cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this) > cbMaxWidthHit)
					{
						// cbMaxWidthHit = cbCurrentLeft - _this->cbLeftPadding; !!!!!
						cbMaxWidthHit = cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this);
					}
					if (!bFinishedLayout)
					{
						//printf(
						//	"compare %d %f %f\n", iCurrentWindow,
						//	cbCurrentTop + _this->cbThumbnailAvailableHeight + _this->cbBottomPadding,
						//	_this->cbMaxHeight
						//	);
						
						//if (cbCurrentTop + _this->cbThumbnailAvailableHeight + _this->cbBottomPadding > _this->cbMaxHeight) !!!!!
						if (cbCurrentTop + 
							_sws_WindowSwitcherLayout_GetBottomIncrement(_this) +
							_sws_WindowSwitcherLayout_GetBottomIncrement(_this) - _sws_WindowSwitcherLayout_GetInitialTop(_this) >
							_this->cbMaxHeight - _this->cbMasterBottomPadding)
						{
							//cbCurrentTop -= cbInitialTop + _this->cbPadding; !!!

							HWND hWnd = pWindowList[iCurrentWindow].hWnd;
							sws_WindowSwitcherLayoutWindow_Erase(&(pWindowList[iCurrentWindow]));
							iTarget = iCurrentWindow + 1;

							if (bIsWidthComputed)
							{
								break;
							}
							else
							{
								bFinishedLayout = TRUE;
							}
						}
					}
					if (!bFinishedLayout)
					{
						//cbCurrentTop += _this->cbThumbnailAvailableHeight + _this->cbBottomPadding + cbInitialTop; !!!!!
						cbCurrentTop = cbCurrentTop + _sws_WindowSwitcherLayout_GetBottomIncrement(_this);
					}
					cbCurrentLeft = cbInitialLeft;
				}

				//printf("%d %d\n", cbCurrentLeft, cbCurrentTop);

				if (!bFinishedLayout)
				{
					long hdiff = 0;

					pWindowList[iCurrentWindow].rcThumbnail.left = cbCurrentLeft;
					pWindowList[iCurrentWindow].rcThumbnail.top = cbCurrentTop;
					pWindowList[iCurrentWindow].rcThumbnail.right = cbCurrentLeft + width;
					if (pWindowList[iCurrentWindow].dwWindowFlags & SWS_WINDOWSWITCHERLAYOUT_WINDOWFLAGS_ISTOOSMALLHORIZONTAL)
					{
						SIZE szTemp;
						DwmQueryThumbnailSourceSize(pWindowList[iCurrentWindow].hThumbnail, &szTemp);
						pWindowList[iCurrentWindow].rcThumbnail.right = cbCurrentLeft + szTemp.cx;
					}
					pWindowList[iCurrentWindow].rcThumbnail.bottom = cbCurrentTop + _this->cbThumbnailAvailableHeight;
					if (original_width)
					{
						hdiff = pWindowList[iCurrentWindow].rcThumbnail.bottom;
						pWindowList[iCurrentWindow].rcThumbnail.bottom = cbCurrentTop + (width * _this->cbThumbnailAvailableHeight) / (original_width * 1.0);
						hdiff -= pWindowList[iCurrentWindow].rcThumbnail.bottom;
					}
					if (pWindowList[iCurrentWindow].dwWindowFlags & SWS_WINDOWSWITCHERLAYOUT_WINDOWFLAGS_ISTOOSMALLVERTICAL)
					{
						SIZE szTemp;
						DwmQueryThumbnailSourceSize(pWindowList[iCurrentWindow].hThumbnail, &szTemp);
						pWindowList[iCurrentWindow].rcThumbnail.right = cbCurrentLeft + szTemp.cy;
					}

					pWindowList[iCurrentWindow].rcWindow.left = cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this) + _this->cbElementLeftPadding;
					pWindowList[iCurrentWindow].rcWindow.top = cbCurrentTop - _sws_WindowSwitcherLayout_GetInitialTop(_this) + _this->cbElementTopPadding;
					pWindowList[iCurrentWindow].rcWindow.right = cbCurrentLeft + width + _sws_WindowSwitcherLayout_GetRightIncrement(_this) - _sws_WindowSwitcherLayout_GetInitialLeft(_this) - _this->cbElementRightPadding;
					pWindowList[iCurrentWindow].rcWindow.bottom = cbCurrentTop + _sws_WindowSwitcherLayout_GetBottomIncrement(_this) - _sws_WindowSwitcherLayout_GetInitialTop(_this) - _this->cbElementBottomPadding;
					if (original_width)
					{
						pWindowList[iCurrentWindow].rcWindow.bottom -= hdiff;
					}

					//pWindowList[iCurrentWindow].rcWindow.left = cbCurrentLeft - _this->cbLeftPadding;
					//pWindowList[iCurrentWindow].rcWindow.top = cbCurrentTop - _this->cbTopPadding - _this->cbRowTitleHeight;
					//pWindowList[iCurrentWindow].rcWindow.right = cbCurrentLeft + width + _this->cbRightPadding;
					//pWindowList[iCurrentWindow].rcWindow.bottom = cbCurrentTop + _this->cbThumbnailAvailableHeight + _this->cbBottomPadding;
				}

				//cbCurrentLeft += width + _this->cbRightPadding + _this->cbLeftPadding; !!!!!
				cbCurrentLeft = cbCurrentLeft + width + _sws_WindowSwitcherLayout_GetRightIncrement(_this);

				/*if (iCurrentWindow == 0)
				{
					iCurrentWindow = _this->pWindowList.cbSize;
				}*/

				iTarget = iCurrentWindow;
				iCurrentCount++;
				if (iCurrentCount == _this->pWindowList.cbSize)
				{
					break;
				}
			}

			if (hTarget && direction == SWS_WINDOWSWITCHERLAYOUT_COMPUTE_DIRECTION_INITIAL)
			{
				BOOL bShouldBreak = FALSE;
				int iObtained = 0;
				for (int j = 0; j < _this->pWindowList.cbSize; ++j)
				{
					if (pWindowList[j].hThumbnail)
					{
						if (pWindowList[j].hWnd == hTarget)
						{
							bShouldBreak = TRUE;
						}
						iObtained = j;
					}
				}
				if (bShouldBreak)
				{
					break;
				}
				else
				{
					int iTmpTop = pWindowList[iObtained].rcWindow.top;
					for (int j = iObtained; j >= 0; j--)
					{
						if (pWindowList[j].rcWindow.top != iTmpTop)
						{
							iObtained = j;
							break;
						}
					}
					sws_WindowSwitcherLayout_InvalidateLayout(_this);
					iObtainedIndex = iObtained;
					bFinishedLayout = FALSE;
				}
				continue;
			}
			if (!bHasTarget)
			{
				break;
			}
			else
			{
				int j, h = pWindowList[iObtainedIndex].rcThumbnail.top;
				if (!iTarget)
				{
					break;
				}
				for (j = iObtainedIndex; j >= 0; j--)
				{
					if (pWindowList[j].rcThumbnail.top != h)
					{
						j++;
						break;
					}
				}
				if (j == _this->iIndex)
				{
					break;
				}
				else
				{
					sws_WindowSwitcherLayout_InvalidateLayout(_this);
					iObtainedIndex = j - 1;
				}
			}
		}

		//if (cbCurrentLeft - _this->cbLeftPadding > cbMaxWidthHit) !!!!!
		if (cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this) > cbMaxWidthHit)
		{
			// cbMaxWidthHit = cbCurrentLeft - _this->cbLeftPadding; !!!!!
			cbMaxWidthHit = cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this);
		}
		for (int k = 0; k < _this->pWindowList.cbSize; ++k)
		{
			if (pWindowList[k].iRowMax == -1)
			{
				pWindowList[k].iRowMax = cbCurrentLeft - _sws_WindowSwitcherLayout_GetInitialLeft(_this);
			}
			else
			{
				break;
			}
			//if (k == _this->pWindowList.cbSize - 1)
			//{
			//	k = -1;
			//}
		}

		// ?????
		//if (_this->iWidth)
		//{
		//	cbMaxWidthHit = _this->iWidth - _this->cbPadding;
		//}

		if (!_this->iWidth)
		{
			//_this->iHeight = cbCurrentTop + _this->cbThumbnailAvailableHeight + _this->cbBottomPadding + _this->cbPadding; !!!!!
			_this->iHeight = cbCurrentTop + _sws_WindowSwitcherLayout_GetBottomIncrement(_this) - _sws_WindowSwitcherLayout_GetInitialTop(_this) + _this->cbMasterBottomPadding;
			//_this->iWidth = cbMaxWidthHit + _this->cbPadding; !!!!!
			_this->iWidth = cbMaxWidthHit + _this->cbMasterRightPadding;
			_this->iX = ((_this->mi.rcWork.right - _this->mi.rcWork.left) - _this->iWidth) / 2 + _this->mi.rcWork.left;
			_this->iY = ((_this->mi.rcWork.bottom - _this->mi.rcWork.top) - _this->iHeight) / 2 + _this->mi.rcWork.top;
			//printf("height: %d, cbCurrentTop: %d, %f %f %f\n", _this->iHeight, cbCurrentTop, _this->cbThumbnailAvailableHeight, _this->cbBottomPadding, _this->cbPadding);
		}

		for (int iCurrentWindow = _this->pWindowList.cbSize - 1; iCurrentWindow >= 0; iCurrentWindow--)
		{
			if (pWindowList[iCurrentWindow].iRowMax)
			{
				if (pWindowList[iCurrentWindow].hThumbnail)
				{
					unsigned int diff = (_this->iWidth - _this->cbMasterRightPadding) < pWindowList[iCurrentWindow].iRowMax ? 0 : _this->iWidth - _this->cbMasterRightPadding - pWindowList[iCurrentWindow].iRowMax;
					pWindowList[iCurrentWindow].rcThumbnail.left += diff / 2;
					pWindowList[iCurrentWindow].rcThumbnail.right += diff / 2;
					////wchar_t name[200];
					////GetWindowTextW(pWindowList[iCurrentWindow].hWnd, name, 200);
					////wprintf(L"%s %d %d\n", name, pWindowList[iCurrentWindow].rcThumbnail.left, pWindowList[iCurrentWindow].rcThumbnail.right);

					pWindowList[iCurrentWindow].rcWindow.left += diff / 2;
					pWindowList[iCurrentWindow].rcWindow.right += diff / 2;

					DWM_THUMBNAIL_PROPERTIES dskThumbProps;
					ZeroMemory(&dskThumbProps, sizeof(DWM_THUMBNAIL_PROPERTIES));
					dskThumbProps.dwFlags = DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_RECTDESTINATION;
					dskThumbProps.fSourceClientAreaOnly = FALSE;
					dskThumbProps.fVisible = direction;
					dskThumbProps.opacity = 255;
					dskThumbProps.rcDestination = pWindowList[iCurrentWindow].rcThumbnail;
					if (_this->bIncludeWallpaper && pWindowList[iCurrentWindow].hWnd == _this->hWndWallpaper)
					{
						dskThumbProps.dwFlags |= DWM_TNP_RECTSOURCE;
						dskThumbProps.rcSource.left = _this->mi.rcMonitor.left;
						dskThumbProps.rcSource.right = _this->mi.rcMonitor.right;
						dskThumbProps.rcSource.top = _this->mi.rcMonitor.top;
						dskThumbProps.rcSource.bottom = _this->mi.rcMonitor.bottom;
					}
					HRESULT hr = DwmUpdateThumbnailProperties(pWindowList[iCurrentWindow].hThumbnail, &dskThumbProps);
					if (FAILED(hr))
					{
						// error
					}
				}
			}
		}
	}

	if (!rv)
	{
		/*printf("\n");
		sws_WindowSwitcherLayoutWindow* pWindowList = _this->pWindowList.pList;
		for (UINT i = 0; i < _this->pWindowList.cbSize; ++i)
		{
			TCHAR name[200];
			GetWindowText(pWindowList[i].hWnd, name, 200);
			wprintf(L"%d %s\n", pWindowList[i].hWnd, name);
		}*/
	}

	return rv;
}

void sws_WindowSwitcherLayout_Clear(sws_WindowSwitcherLayout* _this)
{
	if (_this)
	{
		DeleteObject(_this->hFontRegular);
		DeleteObject(_this->hFontRegular2);
		sws_WindowSwitcherLayoutWindow* pWindowList = _this->pWindowList.pList;
		if (pWindowList)
		{
			for (int iCurrentWindow = 0; iCurrentWindow < _this->pWindowList.cbSize; ++iCurrentWindow)
			{
				sws_WindowSwitcherLayoutWindow_Clear(&(pWindowList[iCurrentWindow]));
			}
			sws_vector_Clear(&(_this->pWindowList));
		}
		memset(_this, 0, sizeof(sws_WindowSwitcherLayout));
	}
}

sws_error_t sws_WindowSwitcherLayout_Initialize(
	sws_WindowSwitcherLayout* _this, 
	HMONITOR hMonitor, 
	HWND hWnd, 
	DWORD* settings, 
	sws_vector* pHWNDList, 
	HWND hWndTarget,
	HWND hWndWallpaper
)
{
	sws_error_t rv = SWS_ERROR_SUCCESS;

	if (!rv)
	{
		if (!_this)
		{
			rv = sws_error_Report(sws_error_GetFromInternalError(SWS_ERROR_NO_MEMORY), NULL);
		}
		memset(_this, 0, sizeof(sws_WindowSwitcherLayout));
	}
	if (!rv)
	{
		rv = sws_WindowHelpers_Initialize();
	}
	if (!rv)
	{
		rv = sws_vector_Initialize(&(_this->pWindowList), sizeof(sws_WindowSwitcherLayoutWindow));
	}
	_this->mi.cbSize = sizeof(MONITORINFO);
	if (!rv)
	{
		if (!GetMonitorInfoW(
			hMonitor,
			&(_this->mi)
		))
		{
			rv = sws_error_Report(sws_error_GetFromWin32Error(GetLastError()), NULL);
		}
	}
	if (!rv)
	{
		_this->bWallpaperAlwaysLast = SWS_WINDOWSWITCHERLAYOUT_WALLPAPER_ALWAYS_LAST;
		_this->bIncludeWallpaper = SWS_WINDOWSWITCHERLAYOUT_INCLUDE_WALLPAPER;
		if (settings) _this->bIncludeWallpaper = settings[3] && settings[9];
		_this->bWallpaperToggleBehavior = SWS_WINDOWSWITCHERLAYOUT_WALLPAPER_TOGGLE;
		_this->hWndWallpaper = hWndWallpaper;
		if (_this->bIncludeWallpaper)
		{
			if (_this->bWallpaperAlwaysLast && !hWndTarget)
			{
				sws_WindowSwitcherLayoutWindow swsLayoutWindow;
				sws_WindowSwitcherLayoutWindow_Initialize(&swsLayoutWindow, _this->hWndWallpaper, NULL);
				sws_vector_PushBack(&_this->pWindowList, &swsLayoutWindow);
			}
		}
	}
	if (!rv)
	{
		/*WCHAR wszShellExperienceHostPath[MAX_PATH];
		GetWindowsDirectoryW(wszShellExperienceHostPath, MAX_PATH);
		wcscat_s(wszShellExperienceHostPath, MAX_PATH, L"\\SystemApps\\ShellExperienceHost_cw5n1h2txyewy\\ShellExperienceHost.exe");
		WCHAR wszStartMenuExperienceHostPath[MAX_PATH];
		GetWindowsDirectoryW(wszStartMenuExperienceHostPath, MAX_PATH);
		wcscat_s(wszStartMenuExperienceHostPath, MAX_PATH, L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\StartMenuExperienceHost.exe");
		WCHAR wszSearchHostPath[MAX_PATH];
		GetWindowsDirectoryW(wszSearchHostPath, MAX_PATH);
		wcscat_s(wszSearchHostPath, MAX_PATH, L"\\SystemApps\\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\\SearchHost.exe");*/
		if (pHWNDList)
		{
			wchar_t* targetAUMID = sws_WindowHelpers_GetAUMIDForHWND(hWndTarget);
			sws_window* windowList = pHWNDList->pList;
			sws_window* window = NULL;
			if (hWndTarget)
			{
				for (int i = 0; i < pHWNDList->cbSize; ++i)
				{
					if (windowList[i].hWnd == hWndTarget)
					{
						window = &(windowList[i]);
						break;
					}
				}
			}
			//if (hWndTarget && window && window->bIsApplicationFrameHost)
			//{
			//	sws_WindowSwitcherLayoutWindow swsLayoutWindow;
			//	sws_WindowSwitcherLayoutWindow_Initialize(&swsLayoutWindow, window->hWnd, window->wszPath);
			//	sws_vector_PushBack(&_this->pWindowList, &swsLayoutWindow);
			//}
			//else
			{
				WCHAR wszRundll32Path[MAX_PATH];
				GetSystemDirectoryW(wszRundll32Path, MAX_PATH);
				wcscat_s(wszRundll32Path, MAX_PATH, L"\\rundll32.exe");
				//HWND hWndForeground = GetForegroundWindow();
				for (int i = pHWNDList->cbSize - 1; i >= 0; i--)
				{
					/*if (!_wcsicmp(wszShellExperienceHostPath, windowList[i].wszPath) ||
						!_wcsicmp(wszStartMenuExperienceHostPath, windowList[i].wszPath) ||
						!_wcsicmp(wszSearchHostPath, windowList[i].wszPath))
					{
						continue;
					}*/
					BOOL isCloaked;
					DwmGetWindowAttribute(windowList[i].hWnd, DWMWA_CLOAKED, &isCloaked, sizeof(BOOL));
					if (isCloaked)
					{
						continue;
						/*if (!windowList[i].bIsApplicationFrameHost)
						{
							continue;
						}
						if (windowList[i].bIsApplicationFrameHost && windowList[i].hWnd != hWndForeground)
						{
							continue;
						}*/
					}
					if (hWndTarget && hWndTarget != windowList[i].hWnd)
					{
						if (targetAUMID)
						{
							if (!(windowList[i].wszAUMID && !wcscmp(targetAUMID, windowList[i].wszAUMID))) continue;
						}
						else
						{
							if (!window)
							{
								continue;
							}
							else if (window->dwProcessId != windowList[i].dwProcessId && _wcsicmp(window->wszPath, windowList[i].wszPath))
							{
								continue;
							}
						}
					}
					if (!hWndTarget && settings[10] && _wcsicmp(windowList[i].wszPath, wszRundll32Path))
					{
						BOOL bShouldContinue = FALSE;
						for (int j = i - 1; j >= 0; j--)
						{
							if (sws_WindowHelpers_IsAltTabWindow(windowList[j].hWnd) && windowList[i].wszAUMID && windowList[j].wszAUMID)
							{
								if (!wcscmp(windowList[i].wszAUMID, windowList[j].wszAUMID) && (settings[4] ? MonitorFromWindow(windowList[i].hWnd, MONITOR_DEFAULTTONULL) == MonitorFromWindow(windowList[j].hWnd, MONITOR_DEFAULTTONULL) : TRUE))
								{
									windowList[j].pNextWindow = windowList + i;
									bShouldContinue = TRUE;
									break;
								}
							}
							else if (sws_WindowHelpers_IsAltTabWindow(windowList[j].hWnd) &&
								(windowList[i].dwProcessId == windowList[j].dwProcessId || !_wcsicmp(windowList[i].wszPath, windowList[j].wszPath)) &&
								(settings[4] ? MonitorFromWindow(windowList[i].hWnd, MONITOR_DEFAULTTONULL) == MonitorFromWindow(windowList[j].hWnd, MONITOR_DEFAULTTONULL) : TRUE))
							{
								bShouldContinue = TRUE;
								break;
							}
						}
						if (bShouldContinue)
						{
							continue;
						}
					}
					if (settings[4] && hMonitor != MonitorFromWindow(windowList[i].hWnd, MONITOR_DEFAULTTONULL))
					{
						continue;
					}
					sws_WindowSwitcherLayoutWindow swsLayoutWindow;
					sws_WindowSwitcherLayoutWindow_Initialize(&swsLayoutWindow, windowList[i].hWnd, windowList[i].wszPath);
					for (sws_window* pcw = windowList + i; pcw != NULL; pcw = pcw->pNextWindow) sws_WindowSwitcherLayoutWindow_AddGroupedWnd(&swsLayoutWindow, pcw->hWnd);
					sws_vector_PushBack(&_this->pWindowList, &swsLayoutWindow);
				}
			}
			if (targetAUMID) CoTaskMemFree(targetAUMID);
		}
	}
	////if (!rv)
	////{
	////	rv = sws_WindowHelpers_RealEnumWindows((WNDENUMPROC)_sws_WindowSwitcherLayout_EnumWindowsCallback, (LPARAM)_this);
	////}

	_this->cbMaxHeight = 0;
	_this->cbMaxWidth = 0;
	_this->cbRowWidth = 0;
	_this->cbRowHeight = SWS_WINDOWSWITCHERLAYOUT_ROWHEIGHT;
	if (settings) _this->cbRowHeight = settings[0];
	_this->cbRowTitleHeight = SWS_WINDOWSWITCHERLAYOUT_ROWTITLEHEIGHT;
	_this->cbMasterTopPadding = SWS_WINDOWSWITCHERLAYOUT_MASTER_PADDING_TOP;
	if (settings) _this->cbMasterTopPadding = settings[8];
	_this->cbMasterBottomPadding = SWS_WINDOWSWITCHERLAYOUT_MASTER_PADDING_BOTTOM;
	if (settings) _this->cbMasterBottomPadding = _this->cbMasterTopPadding;
	_this->cbMasterLeftPadding = SWS_WINDOWSWITCHERLAYOUT_MASTER_PADDING_LEFT;
	if (settings) _this->cbMasterLeftPadding = _this->cbMasterTopPadding;
	_this->cbMasterRightPadding = SWS_WINDOWSWITCHERLAYOUT_MASTER_PADDING_RIGHT;
	if (settings) _this->cbMasterRightPadding = _this->cbMasterTopPadding;
	_this->cbElementTopPadding = SWS_WINDOWSWITCHERLAYOUT_ELEMENT_PADDING_TOP;
	_this->cbElementBottomPadding = SWS_WINDOWSWITCHERLAYOUT_ELEMENT_PADDING_BOTTOM;
	_this->cbElementLeftPadding = SWS_WINDOWSWITCHERLAYOUT_ELEMENT_PADDING_LEFT;
	_this->cbElementRightPadding = SWS_WINDOWSWITCHERLAYOUT_ELEMENT_PADDING_RIGHT;
	_this->cbTopPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_TOP;
	_this->cbBottomPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_BOTTOM;
	_this->cbLeftPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_LEFT;
	_this->cbRightPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_RIGHT;
	_this->cbHDividerPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_DIVIDER_HORIZONTAL;
	_this->cbHDividerPadding = SWS_WINDOWSWITCHERLAYOUT_PADDING_DIVIDER_HORIZONTAL;
	_this->cbMaxTileWidth = _this->cbRowHeight * SWS_WINDOWSWITCHERLAYOUT_MAX_TILE_WIDTH;
	_this->cbThumbnailAvailableHeight = 0;
	_this->hWnd = hWnd;
	_this->hMonitor = hMonitor;
	_this->iIndex = _this->pWindowList.cbSize - 1;

	if (!rv)
	{
		int pw = SWS_WINDOWSWITCHERLAYOUT_PERCENTAGEWIDTH;
		if (settings) pw = settings[1];
		_this->cbMaxWidth = (unsigned int)((double)(_this->mi.rcWork.right - _this->mi.rcWork.left) * (pw / 100.0));
		if (settings && settings[5] != 0 && _this->cbMaxWidth > settings[5])
		{
			_this->cbMaxWidth = settings[5];
		}
		int ph = SWS_WINDOWSWITCHERLAYOUT_PERCENTAGEHEIGHT;
		if (settings) ph = settings[2];
		_this->cbMaxHeight = (unsigned int)((double)(_this->mi.rcWork.bottom - _this->mi.rcWork.top) * (ph / 100.0));
		if (settings && settings[6] != 0 && _this->cbMaxHeight > settings[6])
		{
			_this->cbMaxHeight = settings[6];
		}

		HRESULT hr = GetDpiForMonitor(
			hMonitor,
			MDT_DEFAULT,
			&(_this->cbDpiX),
			&(_this->cbDpiY)
		);
		rv = sws_error_Report(sws_error_GetFromHRESULT(hr), NULL);

		_this->cbRowHeight *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbRowTitleHeight *= (_this->cbDpiY / DEFAULT_DPI_X);
		_this->cbMasterTopPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbMasterLeftPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbMasterBottomPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbMasterRightPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbTopPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbLeftPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbBottomPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbRightPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbElementTopPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbElementLeftPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbElementBottomPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbElementRightPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbHDividerPadding *= (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->cbVDividerPadding *= (_this->cbDpiX / DEFAULT_DPI_X);
		_this->cbMaxTileWidth *= (_this->cbDpiX / DEFAULT_DPI_X);

		_this->cbThumbnailAvailableHeight = _this->cbRowHeight - _this->cbRowTitleHeight - _this->cbTopPadding - 2 * _this->cbBottomPadding;
	}
	if (!rv)
	{
		sws_WindowSwitcherLayoutWindow* pWindowList = _this->pWindowList.pList;
		for (int iCurrentWindow = _this->pWindowList.cbSize - 1; iCurrentWindow >= 0; iCurrentWindow--)
		{
			if (!pWindowList[iCurrentWindow].hIcon)
			{
				double factor = SWS_UWP_ICON_SCALE_FACTOR;
				pWindowList[iCurrentWindow].rcIcon.left = 0;
				pWindowList[iCurrentWindow].rcIcon.top = 0;
				pWindowList[iCurrentWindow].rcIcon.right = _this->cbRowTitleHeight * (SWS_WINDOWSWITCHERLAYOUT_PERCENTAGEICON / 100.0);
				pWindowList[iCurrentWindow].rcIcon.bottom = pWindowList[iCurrentWindow].rcIcon.right;
				pWindowList[iCurrentWindow].szIcon = pWindowList[iCurrentWindow].rcIcon.right;
				pWindowList[iCurrentWindow].hIcon = sws_DefAppIcon;
				/*sws_IconPainter_CallbackParams* params = malloc(sizeof(sws_IconPainter_CallbackParams));
				if (params)
				{
					params->hWnd = hWnd;
					params->index = iCurrentWindow;
					if (!_this->timestamp)
					{
						_this->timestamp = sws_milliseconds_now();
					}
					params->timestamp = _this->timestamp;
					params->bIsDesktop = (_this->bIncludeWallpaper && pWindowList[iCurrentWindow].hWnd == _this->hWndWallpaper);
					if (!sws_IconPainter_ExtractAndDrawIconAsync(pWindowList[iCurrentWindow].hWnd, params))
					{
						pWindowList[iCurrentWindow].hIcon = sws_DefAppIcon;
						free(params);
					}
				}*/
			}
		}
	}
	if (!rv)
	{
		LOGFONT logFont;
		memset(&logFont, 0, sizeof(logFont));
		logFont.lfHeight = SWS_FONT_SIZE * (_this->cbDpiY / DEFAULT_DPI_Y);
		wcscpy_s(logFont.lfFaceName, 32, _T(SWS_FONT_NAME));
		logFont.lfWeight = FW_REGULAR;
		_this->hFontRegular = CreateFontIndirectW(&logFont);
		if (!_this->hFontRegular)
		{
			rv = sws_error_Report(sws_error_GetFromWin32Error(GetLastError()), NULL);
		}
		logFont.lfHeight = SWS_FONT_SIZE2 * (_this->cbDpiY / DEFAULT_DPI_Y);
		_this->hFontRegular2 = CreateFontIndirectW(&logFont);
		if (!_this->hFontRegular2)
		{
			rv = sws_error_Report(sws_error_GetFromWin32Error(GetLastError()), NULL);
		}
	}

	return rv;
}
