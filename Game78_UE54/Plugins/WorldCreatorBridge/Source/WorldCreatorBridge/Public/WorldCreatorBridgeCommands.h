// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "WorldCreatorBridgeStyle.h"

class FWorldCreatorBridgeCommands : public TCommands<FWorldCreatorBridgeCommands>
{
public:

	FWorldCreatorBridgeCommands()
		: TCommands<FWorldCreatorBridgeCommands>(TEXT("WorldCreatorBridge"), NSLOCTEXT("Contexts", "WorldCreatorBridge", "WorldCreatorBridge Plugin"), NAME_None, FWorldCreatorBridgeStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
