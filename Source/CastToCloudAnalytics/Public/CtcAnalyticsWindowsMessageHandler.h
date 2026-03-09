// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <HAL/Platform.h>

#if PLATFORM_WINDOWS

#include <Windows/WindowsApplication.h>

class CASTTOCLOUDANALYTICS_API FCtcWindowsMessageHandler : public IWindowsMessageHandler
{
public:
	virtual ~FCtcWindowsMessageHandler() = default;

	FSimpleMulticastDelegate OnAltF4Pressed;

private:
	// ~Begin IWindowsMessageHandler interface
	virtual bool ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult) override;
	// ~End IWindowsMessageHandler interface
};

#endif
