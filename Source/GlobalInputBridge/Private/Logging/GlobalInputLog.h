#pragma once

#include "CoreMinimal.h"
#include "Settings/GlobalInputSettings.h"
#include "Templates/Atomic.h"

/** Thread-safe native log policy. Worker threads never access settings UObjects. */
class FGlobalInputLog final
{
public:
	static void SetLevel(EGlobalInputLogLevel InLevel);
	static bool ShouldLog(EGlobalInputLogLevel RequiredLevel);

private:
	static TAtomic<uint8> Level;
};
