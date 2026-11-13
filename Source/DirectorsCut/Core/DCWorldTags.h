// Director's Cut — world group names shared by the Scene builder (D) and World interaction (E). LEAD-OWNED.
#pragma once

#include "CoreMinimal.h"

namespace DCGroups
{
	// Component groups inside an ADCRoomModule.
	inline const FName Shell    = TEXT("Shell");          // always visible: floor, walls, silhouette (dim when locked)
	inline const FName Locked   = TEXT("State.Locked");   // dust sheets, tape, faint rim light. Visible only when Locked
	inline const FName Building = TEXT("State.Building"); // scaffolding, work lights. Visible only during the reveal
	inline const FName Level1   = TEXT("Level.1");        // visible when Built and Level >= 1
	inline const FName Level2   = TEXT("Level.2");        // visible when Built and Level >= 2
	inline const FName Level3   = TEXT("Level.3");        // visible when Built and Level >= 3
}

namespace DCTags
{
	// Component tag on lights that should fade on during the room reveal.
	inline const FName RoomLight = TEXT("RoomLight");
}
