// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldCreatorBridgeCommands.h"

#define LOCTEXT_NAMESPACE "FWorldCreatorBridgeModule"

void FWorldCreatorBridgeCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "WorldCreatorBridge", "Execute WorldCreatorBridge action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
