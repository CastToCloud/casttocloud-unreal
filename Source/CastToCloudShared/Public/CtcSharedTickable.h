// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Tickable.h>

class CASTTOCLOUDSHARED_API FTickableWithDebug : public FTickableGameObject
{
protected:
	virtual void OnTick(float DeltaTime) = 0;
	virtual void OnDebugTick(float DeltaTime) = 0;

private:
	virtual void Tick(float DeltaTime) final override;
};
