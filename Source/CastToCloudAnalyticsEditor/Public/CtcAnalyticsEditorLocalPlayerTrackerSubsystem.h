// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcAnalyticsLocalPlayerTrackerSubsystem.h"

#include "CtcAnalyticsEditorLocalPlayerTrackerSubsystem.generated.h"

UCLASS()
class CASTTOCLOUDANALYTICSEDITOR_API UCtcAnalyticsEditorLocalPlayerTrackerSubsystem : public UCtcAnalyticsLocalPlayerTrackerSubsystem
{
	GENERATED_BODY()

	//~Begin UCtcAnalyticsEditorLocalPlayerTrackerSubsystem interface
	virtual void RegisterSessionDelegates() override;
	virtual void UnregisterSessionDelegates() override;
	//~End UCtcAnalyticsEditorLocalPlayerTrackerSubsystem interface

	/**
	 * Callback executed when the Play In Editor (PIE) session starts
	 */
	void OnPIEStarted(bool bIsSimulating);
	/**
	 * Callback executed when the Play In Editor (PIE) session ends
	 */
	void OnPIEEnded(bool bIsSimulating);
};
