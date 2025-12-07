// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldCreatorBridge : ModuleRules
{
  public WorldCreatorBridge(ReadOnlyTargetRules Target) : base(Target)
  {
    PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

    PublicIncludePaths.AddRange(
      new string[] {
				// ... add public include paths required here ...
			}
      );


    PrivateIncludePaths.AddRange(
      new string[] {
				// ... add other private include paths required here ...
			}
      );


    PublicDependencyModuleNames.AddRange(
        new string[]
        {
                "Core",
                "CoreUObject",
                "Engine",
                "SlateCore",
                "InputCore",
                "XmlParser",
                "Landscape",
                "EditorScriptingUtilities",
                "Foliage",
                "LandscapeEditor",
                "WorldPartitionEditor"
          // ... add other public dependencies that you statically link with here ...
  }
        );


    PrivateDependencyModuleNames.AddRange(
        new string[]
        {
                ///"UnrealEd",
				"Projects",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ToolMenus",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AssetTools",
                "AssetRegistry",
                "LevelEditor"
          // ... add private dependencies that you statically link with here ...	
  }
        );


    DynamicallyLoadedModuleNames.AddRange(
  new string[]
  {
    // ... add any modules that your module loads dynamically here ...
  }
  );
  }
}
