// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedTickable.h"

void FTickableWithDebug::Tick(float DeltaTime)
{
	OnTick(DeltaTime);
	OnDebugTick(DeltaTime);
}