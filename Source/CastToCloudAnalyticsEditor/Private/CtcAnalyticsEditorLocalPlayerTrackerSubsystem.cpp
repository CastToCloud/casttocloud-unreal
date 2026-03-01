// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsEditorLocalPlayerTrackerSubsystem.h"

#include <Editor.h>

void UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::RegisterSessionDelegates()
{
	//NOTE: Intentionally skipping super call to avoid the other delegates

	FEditorDelegates::StartPIE.AddUObject(this, &UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::OnPIEStarted);
	FEditorDelegates::ShutdownPIE.AddUObject(this, &UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::OnPIEEnded);
}

void UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::UnregisterSessionDelegates()
{
	//NOTE: Intentionally skipping super call to avoid the other delegates

	FEditorDelegates::StartPIE.RemoveAll(this);
	FEditorDelegates::ShutdownPIE.RemoveAll(this);
}

void UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::OnPIEStarted(bool bIsSimulating)
{
	OnSessionStart();
}

void UCtcAnalyticsEditorLocalPlayerTrackerSubsystem::OnPIEEnded(bool bIsSimulating)
{
	OnSessionEnd();
}