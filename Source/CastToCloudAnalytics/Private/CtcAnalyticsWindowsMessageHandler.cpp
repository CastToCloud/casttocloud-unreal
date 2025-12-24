// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsWindowsMessageHandler.h"

#if PLATFORM_WINDOWS

bool FCtcWindowsMessageHandler::ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult)
{
	if (Msg == WM_SYSKEYDOWN && WParam == VK_F4)
	{
		OnAltF4Pressed.Broadcast();
	}

	return false;
}

#endif
