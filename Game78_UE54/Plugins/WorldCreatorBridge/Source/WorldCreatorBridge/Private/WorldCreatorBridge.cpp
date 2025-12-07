
// Copyright Epic Games, Inc. All Rights Reserved.
#include "ComponentReregisterContext.h"
#include "WorldCreatorBridge.h"
#include "WorldCreatorBridgeStyle.h"
#include "WorldCreatorBridgeCommands.h"
#include "Misc/MessageDialog.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "GameFramework/WorldSettings.h"

// Slate
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Runtime/SlateCore/Public/Widgets/Images/SImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SNumericEntryBox.h"         
#include "Widgets/Input/SComboButton.h"

// menu
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"

// #include "WorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartitionConverter.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

// Landscape
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Landscape.h"
#include "LandscapeGizmoActiveActor.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "LandscapeConfigHelper.h"
#include "Editor/LandscapeEditor/Public/LandscapeEditorObject.h" // dannach is eher fragwürdig
#include "Editor/LandscapeEditor/Private/LandscapeRegionUtils.h"
#include "Editor/LandscapeEditor/Public/LandscapeImportHelper.h"
#include "Editor/LandscapeEditor/Public/LandscapeEditorUtils.h"
#include "Editor/LandscapeEditor/Public/LandscapeFileFormatInterface.h"
#include "Editor/LandscapeEditor/Private/LandscapeTiledImage.h"

#include "LandscapeSettings.h"
#include "ScopedTransaction.h"
#include "FileHelpers.h"

// engine 
#include "Misc/FileHelper.h"
#include "EngineUtils.h"          
#include "AutomatedAssetImportData.h"
#include "AssetToolsModule.h"
#include "LevelEditorActions.h"
#include "Engine/Selection.h"
#include "EditorBuildUtils.h"
#include "HAL/FileManager.h"
#include "ObjectTools.h"	
#include "LocationVolume.h"
#include "EditorModeManager.h"
#include "EditorModes.h"	
#include "Containers/Array.h"

// materials
#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"

// File System
#include "HAL/FileManagerGeneric.h"

// cpp stuff
#include <filesystem>


using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

static const FName WorldCreatorBridgeTabName("WorldCreatorBridge");
static const float SCALE_FACTOR = 100.0f;
static const float TERRAIN_BASE_SCALE = 512.0f;
static const FBox REGIONBOX(FVector(-WORLDPARTITION_MAX, -WORLDPARTITION_MAX, -WORLDPARTITION_MAX), FVector(WORLDPARTITION_MAX, WORLDPARTITION_MAX, WORLDPARTITION_MAX));
static const FString MATERIAL_PACKAGE_NAME_PREFIX = "/WorldCreatorBridge/";
static const FString COLORMAP_NAME = "colormap";
static const FString TEXTUREMAP_NAME = "texturemap";
static const int UNREAL_TERRAIN_RESOLUTION = 16654561;
static const int WC_TILE_RESOLUTION = 4096;
static const int WC_BASE_RESOLUTION = 1024;
static const int UNREAL_MIN_TILE_RESOLUTION = 64;
static const float UNREAL_TERRAIN_SCALE_FACTOR = 0.1953125f;
#define LOCTEXT_NAMESPACE "FWorldCreatorBridgeModule"


void FWorldCreatorBridgeModule::StartupModule()
{
  // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

  FWorldCreatorBridgeStyle::Initialize();
  FWorldCreatorBridgeStyle::ReloadTextures();

  FWorldCreatorBridgeCommands::Register();

  PluginCommands = MakeShareable(new FUICommandList);

  PluginCommands->MapAction(
    FWorldCreatorBridgeCommands::Get().PluginAction,
    FExecuteAction::CreateRaw(this, &FWorldCreatorBridgeModule::PluginButtonClicked),
    FCanExecuteAction());

  UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWorldCreatorBridgeModule::RegisterMenus));

  FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WorldCreatorBridgeTabName, FOnSpawnTab::CreateRaw(this, &FWorldCreatorBridgeModule::OnSpawnPluginTab))
    .SetDisplayName(LOCTEXT("FWorldCreatorBridgeTabTitle", "WorldCreatorBridge"))
    .SetMenuType(ETabSpawnerMenuType::Hidden);

  SetupUIElements();
}

void FWorldCreatorBridgeModule::ShutdownModule()
{
  // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
  // we call this function before unloading the module.

  UToolMenus::UnRegisterStartupCallback(this);

  UToolMenus::UnregisterOwner(this);

  FWorldCreatorBridgeStyle::Shutdown();

  FWorldCreatorBridgeCommands::Unregister();

  FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WorldCreatorBridgeTabName);
}

TSharedRef<SDockTab> FWorldCreatorBridgeModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
  return SNew(SDockTab)
    .TabRole(ETabRole::NomadTab)
    [
      SNew(SScrollBox)
        // SNew(SVerticalBox)
        // + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        + SScrollBox::Slot().HAlign(HAlign_Center)
        [
          SNew(SButton).ButtonStyle(wcButton).OnClicked(
            FOnClicked::CreateLambda([]
              {
                system("start https://www.world-creator.com/");
                return FReply::Handled();
              }))
        ]
        + SScrollBox::Slot().HAlign(HAlign_Center)
        [
          SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(facebookButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://www.facebook.com/worldcreator3d/");
                    return FReply::Handled();
                  }))
            ]

            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(twitterButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://twitter.com/worldcreator3d/");
                    return FReply::Handled();
                  }))
            ]

            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(instagramButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://www.instagram.com/worldcreator3d/");
                    return FReply::Handled();
                  }))
            ]

            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(youtubeButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://www.youtube.com/channel/UClabqa6PHVjXzR2Y7s1MP0Q/");
                    return FReply::Handled();
                  }))
            ]
        ]
        + SScrollBox::Slot().HAlign(HAlign_Center)
        [
          SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(vimeoButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://vimeo.com/user82114310/");
                    return FReply::Handled();
                  }))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(twitchButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://www.twitch.tv/worldcreator3d/");
                    return FReply::Handled();
                  }))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(artstationButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://www.artstation.com/worldcreator/");
                    return FReply::Handled();
                  }))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
              SNew(SButton).ButtonStyle(discordButton).OnClicked(
                FOnClicked::CreateLambda([]
                  {
                    system("start https://discordapp.com/invite/bjMteus");
                    return FReply::Handled();
                  }))
            ]
        ] + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("If you moved the sync folder, please select your \nBridge.xml file here. Its default location is \n[USER]/Documents/World Creator/Sync/bridge.xml"))))
                    .ToolTipText(FText::FromString("By default the path points to the World Creator Sync Folder. You only need to change it if you want to sync a file from a different location."))
                ]
            ]
            + SScrollBox::Slot().Padding(10, 0, 10, 5)
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(SButton)
                    .Text(FText::FromString("Select Bridge File")).OnClicked(
                      FOnClicked::CreateRaw(this, &FWorldCreatorBridgeModule::BrowseButtonClicked))
                    .ToolTipText(FText::FromString("By default the path points to the World Creator Sync Folder. You only need to change it if you want to sync a file from a different location."))
                ]
            ]
            + SScrollBox::Slot().Padding(10, 0, 10, 35)
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SAssignNew(selectedPathBox, SEditableTextBox)
                    .Text(FText::FromString(selectedPath))
                    .IsReadOnly(true)

                ]
            ]
            + SScrollBox::Slot().Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Actor Name"))
                        .ToolTipText(FText::FromString("Name of the Actor that holds your terrain Object"))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(SEditableTextBox).Text(FText::FromString("WC_Terrain")).OnTextChanged(
                    FOnTextChanged::CreateLambda([this](const FText& InText)
                      {
                        this->terrainName = InText.ToString();
                      }
                    )
                  )
                ]
            ]
            + SScrollBox::Slot().Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Material Name"))
                        .ToolTipText(FText::FromString("Name of the material created for the Landscape"))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(SEditableTextBox).Text(FText::FromString("M_Terrain")).OnTextChanged(
                    FOnTextChanged::CreateLambda([this](const FText& InText)
                      {
                        this->terrainMaterialName = InText.ToString();
                      }
                    )
                  )
                ]
            ]
            + SScrollBox::Slot().Padding(FMargin(0.0f, 10.0f, 10.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SComboButton)
                    .OnGetMenuContent_Static(&FWorldCreatorBridgeModule::GetSectionSizeMenu, this)
                    .ContentPadding(2)
                    .ButtonContent()
                    [
                      SNew(STextBlock)
                        .Text(FText::FromString("Quads / Section"))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(SNumericEntryBox<int>) // research SNumericDropDown
                    .Value_Raw(this, &FWorldCreatorBridgeModule::GetQuatPSSizeDelta)
                    .IsEnabled(false)
                    .OnValueChanged(FOnInt32ValueChanged::CreateLambda([this](int value)
                      {
                        this->unrealTerrainResolution = RecaulculateToUnrealSize(value, this->unrealTerrainResolution);
                      }))
                ]
            ]
            +SScrollBox::Slot().Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Terrain Size"))
                        .ToolTipText(FText::FromString("Size that the terrain is imported in, keep in mind that with the size the memory consumption while importing expands exponentially. Recommended sizes are: Quats / Section 63 & Terrain Size 4033, Quats / Section 127 & Terrain Size 8192. If the terrain is smaller than the specified Terrain size, the size will be adopted to the smaller size on import"))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(SNumericEntryBox<int>)
                    .MinValue(63)
                    .Value_Raw(this, &FWorldCreatorBridgeModule::GetCutSizeDelta)
                    .OnValueChanged(FOnInt32ValueChanged::CreateLambda([this](int value)
                      {
                        value = RecaulculateToUnrealSize(quatsPerSection, value);
                        this->unrealTerrainResolution = value;
                      }))
                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [
                  SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("The value above relates to the size individual\nTerrains can have as a max size. If the heightmap is larger\nthan the specified value, multiple terrains will be created.\n! A size above 8129 can cause long loding times !"))))
                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(10.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Import Textures"))
                        .ToolTipText(FText::FromString("Choose whether textures are automatically imported."))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(
                    FOnCheckStateChanged::CreateLambda([this](const ECheckBoxState& state)
                      {
                        this->bImportTextures = state == ECheckBoxState::Checked;
                      })
                  )
                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(10.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Import Layers"))
                        .ToolTipText(FText::FromString("Choose whether Layers are imported."))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(
                    FOnCheckStateChanged::CreateLambda([this](const ECheckBoxState& state)
                      {
                        this->bImportLayers = state == ECheckBoxState::Checked;
                      })
                  )
                ]
            ]

            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(10.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("World Partition"))
                        .ToolTipText(FText::FromString("Choose whether to import the entire heightmap or to split it to enable world partition"))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SCheckBox).IsChecked(ECheckBoxState::Unchecked).OnCheckStateChanged(
                    FOnCheckStateChanged::CreateLambda([this](const ECheckBoxState& state)
                      {
                        this->bUseWorldPartition = state == ECheckBoxState::Checked;
                      })
                  )
                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0, 0, 0)
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Grid Size"))
                        .ToolTipText(FText::FromString("Set the world partition grid size"))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SNumericEntryBox<int>)
                    .AllowSpin(true)
                    .MinValue(1)
                    .MaxValue(64)
                    .Value_Raw(this, &FWorldCreatorBridgeModule::GetGridSizeDelta)
                    .MinSliderValue(1)
                    .MaxSliderValue(64)
                    .OnValueChanged(FOnInt32ValueChanged::CreateLambda([this](int value)
                      {
                        this->worldPartitionGridSize = value;
                      }))

                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0, 0, 0)
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("Region Size"))
                        .ToolTipText(FText::FromString("Set the world partition region size"))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SNumericEntryBox<int>)
                    .AllowSpin(true)
                    .MinValue(1)
                    .MaxValue(64)
                    .Value_Raw(this, &FWorldCreatorBridgeModule::GetRegionSizeDelta)
                    .MinSliderValue(1)
                    .MaxSliderValue(64)
                    .OnValueChanged(FOnInt32ValueChanged::CreateLambda([this](int value)
                      {
                        this->worldPartitionRegionSize = value;
                      }))

                ]
            ]
            +SScrollBox::Slot().HAlign(HAlign_Left).Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
            [
              SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0, 0, 0)
                [
                  SNew(SBox).WidthOverride(100)
                    [
                      SNew(STextBlock).Text(FText::FromString("World Scale"))
                        .ToolTipText(FText::FromString("Allows you to scale the terrain to a different value."))
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                  SNew(SNumericEntryBox<float>)
                    .AllowSpin(true)
                    .MinValue(0)
                    .MaxValue(1000)
                    .Value_Raw(this, &FWorldCreatorBridgeModule::GetTransformDelta)
                    .MinSliderValue(0)
                    .MaxSliderValue(1000)
                    .OnValueChanged(FOnFloatValueChanged::CreateLambda([this](float value)
                      {
                        this->worldScale = value;
                      }))

                ]
            ]


            + SScrollBox::Slot()
            [
              SNew(SBox).HeightOverride(60.0f)
                [
                  SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(10, 20, 10, 0)
                    [
                      SNew(SButton)
                        .OnClicked(FOnClicked::CreateRaw(
                          this, &FWorldCreatorBridgeModule::SyncButtonClicked))
                        [
                          SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [
                              SNew(STextBlock)
                                .Text(FText::FromString("Synchronize"))
                            ]
                        ]
                    ]
                ]
            ]
            + SScrollBox::Slot()
            [
              SNew(SBox).HeightOverride(60.0f)
                [
                  SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(10, 20, 10, 0)
                    [
                      SNew(SButton)
                        .OnClicked(FOnClicked::CreateRaw(
                          this, &FWorldCreatorBridgeModule::BuildMinimapButtonClicked))
                        [
                          SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [
                              SNew(STextBlock)
                                .Text(FText::FromString("Build Minimap"))
                            ]
                        ]
                    ]
                ]
            ]
    ];
}

TSharedRef<SWidget> FWorldCreatorBridgeModule::GetSectionSizeMenu(FWorldCreatorBridgeModule* bridge)
{
  FMenuBuilder MenuBuilder(true, nullptr);

  for (int32 i = 0; i < UE_ARRAY_COUNT(FLandscapeConfig::SubsectionSizeQuadsValues); i++)
  {
    MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(FLandscapeConfig::SubsectionSizeQuadsValues[i])), FText::GetEmpty(),
      FSlateIcon(), FExecuteAction::CreateLambda([bridge, i]()
        {
          bridge->quatsPerSection = FLandscapeConfig::SubsectionSizeQuadsValues[i];
          bridge->UpdateTerrainResolution();
        }));
  }

  return MenuBuilder.MakeWidget();
}

void FWorldCreatorBridgeModule::PluginButtonClicked()
{
  FGlobalTabmanager::Get()->TryInvokeTab(WorldCreatorBridgeTabName);
}

void FWorldCreatorBridgeModule::RegisterMenus()
{
  // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
  FToolMenuOwnerScoped OwnerScoped(this);

  {
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    {
      FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
      Section.AddMenuEntryWithCommandList(FWorldCreatorBridgeCommands::Get().PluginAction, PluginCommands);
    }
  }

  {
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    {
      FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
      {
        FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWorldCreatorBridgeCommands::Get().PluginAction));
        Entry.SetCommandList(PluginCommands);
      }
    }
  }
}

void FWorldCreatorBridgeModule::SetupUIElements()
{
  FString teststring = FPlatformProcess::UserDir();
  this->selectedPath = FString::Printf(TEXT("%sWorld Creator/Sync/Bridge.xml"), teststring.GetCharArray().GetData());
  this->terrainName = TEXT("WC_Terrain");
  this->terrainMaterialName = TEXT("M_Terrain");
  this->bImportTextures = true;
  this->bImportLayers = true;
  this->bBuildMinimap = false;
  this->worldScale = 1.0f;
  this->worldPartitionGridSize = 2;
  this->worldPartitionRegionSize = 16;
  this->unrealTerrainResolution = 4033;
  quatsPerSection = 63;
  // Init Brushes
  ///////////////
  auto wcbrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.WorldCreator");
  auto facebookBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Facebook");
  auto twitterBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Twitter");
  auto instagramBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Instagram");
  auto youtubeBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Youtube");
  auto vimeoBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Vimeo");
  auto twitchBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Twitch");
  auto artstationBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Artstation");
  auto discordBrush = FWorldCreatorBridgeStyle::Get().GetBrush("WorldCreatorBridge.Discord");

  wcButton = new FButtonStyle();
  wcButton->SetDisabled(*wcbrush);
  wcButton->SetHovered(*wcbrush);
  wcButton->SetNormal(*wcbrush);
  wcButton->SetPressed(*wcbrush);

  facebookButton = new FButtonStyle();
  facebookButton->SetDisabled(*facebookBrush);
  facebookButton->SetHovered(*facebookBrush);
  facebookButton->SetNormal(*facebookBrush);
  facebookButton->SetPressed(*facebookBrush);

  twitterButton = new FButtonStyle();
  twitterButton->SetDisabled(*twitterBrush);
  twitterButton->SetHovered(*twitterBrush);
  twitterButton->SetNormal(*twitterBrush);
  twitterButton->SetPressed(*twitterBrush);

  instagramButton = new FButtonStyle();
  instagramButton->SetDisabled(*instagramBrush);
  instagramButton->SetHovered(*instagramBrush);
  instagramButton->SetNormal(*instagramBrush);
  instagramButton->SetPressed(*instagramBrush);

  youtubeButton = new FButtonStyle();
  youtubeButton->SetDisabled(*youtubeBrush);
  youtubeButton->SetHovered(*youtubeBrush);
  youtubeButton->SetNormal(*youtubeBrush);
  youtubeButton->SetPressed(*youtubeBrush);

  vimeoButton = new FButtonStyle();
  vimeoButton->SetDisabled(*vimeoBrush);
  vimeoButton->SetHovered(*vimeoBrush);
  vimeoButton->SetNormal(*vimeoBrush);
  vimeoButton->SetPressed(*vimeoBrush);

  twitchButton = new FButtonStyle();
  twitchButton->SetDisabled(*twitchBrush);
  twitchButton->SetHovered(*twitchBrush);
  twitchButton->SetNormal(*twitchBrush);
  twitchButton->SetPressed(*twitchBrush);

  artstationButton = new FButtonStyle();
  artstationButton->SetDisabled(*artstationBrush);
  artstationButton->SetHovered(*artstationBrush);
  artstationButton->SetNormal(*artstationBrush);
  artstationButton->SetPressed(*artstationBrush);

  discordButton = new FButtonStyle();
  discordButton->SetDisabled(*discordBrush);
  discordButton->SetHovered(*discordBrush);
  discordButton->SetNormal(*discordBrush);
  discordButton->SetPressed(*discordBrush);
}

FEdModeLandscape* GetEditorMode()
{
  return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

//void AddComponents(ULandscapeInfo* InLandscapeInfo, ULandscapeSubsystem* InLandscapeSubsystem, const TArray<FIntPoint>& InComponentCoordinates, TArray<ALandscapeProxy*>& OutCreatedStreamingProxies)
//{
//  TRACE_CPUPROFILER_EVENT_SCOPE(AddComponents);
//  TArray<ULandscapeComponent*> NewComponents;
//  InLandscapeInfo->Modify();
//  for (const FIntPoint& ComponentCoordinate : InComponentCoordinates)
//  {
//    ULandscapeComponent* LandscapeComponent = InLandscapeInfo->XYtoComponentMap.FindRef(ComponentCoordinate);
//    if (LandscapeComponent)
//    {
//      continue;
//    }
//
//    // Add New component...
//    FIntPoint ComponentBase = ComponentCoordinate * InLandscapeInfo->ComponentSizeQuads;
//
//    ALandscapeProxy* LandscapeProxy = InLandscapeSubsystem->FindOrAddLandscapeProxy(InLandscapeInfo, ComponentBase);
//    if (!LandscapeProxy)
//    {
//      continue;
//    }
//
//    OutCreatedStreamingProxies.Add(LandscapeProxy);
//
//    LandscapeComponent = NewObject<ULandscapeComponent>(LandscapeProxy, NAME_None, RF_Transactional);
//    NewComponents.Add(LandscapeComponent);
//    LandscapeComponent->Init(
//      ComponentBase.X, ComponentBase.Y,
//      LandscapeProxy->ComponentSizeQuads,
//      LandscapeProxy->NumSubsections,
//      LandscapeProxy->SubsectionSizeQuads
//    );
//
//    TArray<FColor> HeightData;
//    const int32 ComponentVerts = (LandscapeComponent->SubsectionSizeQuads + 1) * LandscapeComponent->NumSubsections;
//    const FColor PackedMidpoint = LandscapeDataAccess::PackHeight(LandscapeDataAccess::GetTexHeight(0.0f));
//    HeightData.Init(PackedMidpoint, FMath::Square(ComponentVerts));
//
//    LandscapeComponent->InitHeightmapData(HeightData, true);
//    LandscapeComponent->UpdateMaterialInstances();
//
//    InLandscapeInfo->XYtoComponentMap.Add(ComponentCoordinate, LandscapeComponent);
//    InLandscapeInfo->XYtoAddCollisionMap.Remove(ComponentCoordinate);
//  }
//
//  // Need to register to use general height/xyoffset data update
//  for (int32 Idx = 0; Idx < NewComponents.Num(); Idx++)
//  {
//    NewComponents[Idx]->RegisterComponent();
//  }
//
//  const bool bHasXYOffset = false;
//  ALandscape* Landscape = InLandscapeInfo->LandscapeActor.Get();
//
//  bool bHasLandscapeLayersContent = Landscape && Landscape->HasLayersContent();
//
//  for (ULandscapeComponent* NewComponent : NewComponents)
//  {
//    if (bHasLandscapeLayersContent)
//    {
//      TArray<ULandscapeComponent*> ComponentsUsingHeightmap;
//      ComponentsUsingHeightmap.Add(NewComponent);
//
//      for (const FLandscapeLayer& Layer : Landscape->LandscapeLayers)
//      {
//        // Since we do not share heightmap when adding new component, we will provided the required array, but they will only be used for 1 component
//        TMap<UTexture2D*, UTexture2D*> CreatedHeightmapTextures;
//        NewComponent->AddDefaultLayerData(Layer.Guid, ComponentsUsingHeightmap, CreatedHeightmapTextures);
//      }
//    }
//
//    // Update Collision
//    NewComponent->UpdateCachedBounds();
//    NewComponent->UpdateBounds();
//    NewComponent->MarkRenderStateDirty();
//
//    if (!bHasLandscapeLayersContent)
//    {
//      ULandscapeHeightfieldCollisionComponent* CollisionComp = NewComponent->GetCollisionComponent();
//      if (CollisionComp && !bHasXYOffset)
//      {
//        CollisionComp->MarkRenderStateDirty();
//        CollisionComp->RecreateCollision();
//      }
//    }
//  }
//
//
//  if (Landscape)
//  {
//    GEngine->BroadcastOnActorMoved(Landscape);
//  }
//}


bool FWorldCreatorBridgeModule::CreateLandscape(int componentCountX, int componentCountY, int quadsPerSection, FVector location, FVector scale, FRotator rotation)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked);

  UPROPERTY() FEdModeLandscape* LandscapeEdMode = GetEditorMode();
  if (!LandscapeEdMode)
  {
    return false;
  }

  const bool bIsNewLandscape = LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape;

  UWorld* World = LandscapeEdMode->GetWorld();

  const bool bCreateLandscape = LandscapeEdMode != nullptr &&
    World != nullptr &&
    World->GetCurrentLevel()->bIsVisible;

  if (!bCreateLandscape)
  {
    return false;
  }

  const bool bIsTempPackage = FPackageName::IsTempPackage(World->GetPackage()->GetName());
  ULandscapeEditorObject* UISettings = LandscapeEdMode->UISettings;

  const bool bIsWorldPartition = World->GetSubsystem<ULandscapeSubsystem>()->IsGridBased();
  const bool bLandscapeLargerThanRegion = static_cast<int32>(UISettings->WorldPartitionRegionSize) < componentCountX || static_cast<int32>(UISettings->WorldPartitionRegionSize) < componentCountY;
  const bool bNeedsLandscapeRegions = bIsWorldPartition && bLandscapeLargerThanRegion;

  // If we need to ensure the map is saved before proceeding to create a landscape with regions 
  if (bIsTempPackage && bNeedsLandscapeRegions)
  {
    FString NewMapPackageName;
    if (!FEditorFileUtils::SaveCurrentLevel())
    {
      UE_LOG(LogTemp, Error, TEXT("Unable to save current level"));
      return false;
    }
  }


  const FIntPoint TotalLandscapeComponentSize{ componentCountX, componentCountY };

  const int32 ComponentCountX = bNeedsLandscapeRegions ? UISettings->WorldPartitionRegionSize : TotalLandscapeComponentSize.X;
  const int32 ComponentCountY = bNeedsLandscapeRegions ? UISettings->WorldPartitionRegionSize : TotalLandscapeComponentSize.Y;
  const int32 QuadsPerComponent = 1 * quadsPerSection; // the 1 is temporary
  const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
  const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

  
  return true;
}


FReply FWorldCreatorBridgeModule::SyncButtonClicked()
{


  GEditor->GetSelectedActors()->DeselectAll();
  if (selectedPath.Len() <= 0)
    return FReply::Handled();

  FLevelEditorActionCallbacks::Save();

  // set start values
  if (terrainName.Len() <= 0)
  {
    terrainName = TEXT("WC_Terrain");
  }
  if (terrainMaterialName.Len() <= 0)
  {
    terrainMaterialName = TEXT("M_Terrain");
  }

  // setup variables 
  auto context = GEditor->GetEditorWorldContext();
  auto world = context.World();
  auto level = world->GetCurrentLevel();
  float scaleFactor = SCALE_FACTOR * worldScale;

  // check stuff TODO check if still relevant 
  if (level)
  {
    if (bUseWorldPartition && !level->bIsPartitioned)
    {
      FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("World Creator Sync", "this map is not configured for world partition, therefore the terrain will be imported as a basic terrain. To enable world partition for this map go to tools->Convert Level..."));
    }
  }

  // import sync stuff
  if (!SetupXmlVariables())
  {
    return FReply::Handled();
  }



  if (bImportTextures)
    ImportTextureFiles();
  FXmlFile configFile(selectedPath);
  root = configFile.GetRootNode();
  if (root == NULL)
    return FReply::Handled();

  FXmlNode* texturingNode = nullptr;
  texturingNode = root->FindChildNode(TEXT("Texturing"));
  if (texturingNode == nullptr)
  {
    bImportLayers = false;
  }

  float m_scaleX = 100;
  float m_scaleY = 100;

  float terrainScale = 0.0f;
  if (version >= 3)
  {
    terrainScale = (maxHeight - minHeight) * UNREAL_TERRAIN_SCALE_FACTOR * rescaleFactor;
    m_scaleX = scaleX * scaleFactor;
    m_scaleY = scaleY * scaleFactor;
  }
  else
  {
    float height = XmlHelper::GetFloat(root->FindChildNode(TEXT("Surface")), "Height");
    terrainScale = ((maxHeight - minHeight) * height) * UNREAL_TERRAIN_SCALE_FACTOR * rescaleFactor;
  }

    //// Load Heightmap & splatmap
    //////////////////////////////

  int startX = 0;
  int startY = 0;
  int landscapeId = 0;
  int heightDataWidth = width < unrealTerrainResolution ? width : unrealTerrainResolution;
  int heightDataLength = length < unrealTerrainResolution ? length : unrealTerrainResolution;
  heightDataWidth = RecaulculateToUnrealSize(quatsPerSection, heightDataWidth);
  heightDataLength = RecaulculateToUnrealSize(quatsPerSection, heightDataLength);


  int lengthCopy = length;
  int widthCopy = width;


  TArray<FXmlNode*> splatmapNodes;
  int numSplatChannels = 0;
  if (texturingNode != nullptr)
  {
    splatmapNodes = texturingNode->GetChildrenNodes();
    for (FXmlNode* child : splatmapNodes)
    {
      numSplatChannels += child->GetChildrenNodes().Num();
    }
  }
  

  // create vectors to save the base location and rotation and remove the previously imported terrain
  FVector* location = new FVector(0, 0, 0);
  FRotator* rotation = new FRotator(0, 0, 0);
  DeletePreviousImportedWorldCreatorLandscape(world, location, rotation);

  for (int tileX = 0; tileX < unrealNumTilesX; tileX++)
  {

    heightDataLength = length < unrealTerrainResolution ? length : unrealTerrainResolution;
    startY = 0;
    // originalHeightDataLength = heightDataLength;
    heightDataLength = RecaulculateToUnrealSize(quatsPerSection, heightDataLength);
    for (int tileY = 0; tileY < unrealNumTilesY; tileY++)
    {
      TArray<FLandscapeImportLayerInfo> layerInfos;
      // reverse that in case of length reversal
      location->Y = (startX * scaleX - tileX) * 100;
      location->X = (startY * scaleY - tileY) * 100;

      TArray<uint16> heightData;
      UE_LOG(LogTemp, Log, TEXT("%d, %d"), heightDataWidth, heightDataLength);
      heightData.Init(0, heightDataWidth * heightDataLength);
      TArray<TArray<uint8>> splatData;
      int numSplatmaps = bImportLayers ? numSplatChannels : 1;
      int initSplatmapValue = bImportLayers ? 0 : 1;
      splatData.Init(TArray<uint8>(), numSplatmaps);
      for (int sp = 0; sp < splatData.Num(); sp++)
      {
        splatData[sp].Init(initSplatmapValue, heightDataWidth * heightDataLength);
      }
      // here i have to load in all maps, order y prioritized 
      TArray<WCLandscapeTile> loadedTiles;

      // the addition of UNREAL_MIN_TILE_RESOLUTION serves as a preventation of zero sized terrains 
      int numLoadedXTiles = 1 + ((startX % WC_TILE_RESOLUTION + heightDataWidth) / (WC_TILE_RESOLUTION + UNREAL_MIN_TILE_RESOLUTION + 1));
      int numLoadedYTiles = 1 + ((startY % WC_TILE_RESOLUTION + heightDataLength) / (WC_TILE_RESOLUTION + UNREAL_MIN_TILE_RESOLUTION + 1));

      int startTileX = (startX / WC_TILE_RESOLUTION);
      int startTileY = (startY / WC_TILE_RESOLUTION);

      int constraintX = 0;
      int constraintY = 0;
      int mappingWidth, mappingLength;
      int lengthLeft = heightDataLength;
      int widthLeft = heightDataWidth;
      int tmpStartX, tmpStartY;
      int heightStartX = 0;
      int heightStartY = 0;
      if (version >= 3)
      {
        tmpStartX = startX % WC_TILE_RESOLUTION;
        tmpStartY = startY % WC_TILE_RESOLUTION;
      }
      else
      {
        tmpStartX = startX;
        tmpStartY = startY;
      }

      for (int x = 0; x < numLoadedXTiles; x++)
      {
        lengthLeft = heightDataLength;
        heightStartY = 0;
        for (int y = 0; y < numLoadedYTiles; y++)
        {
          FString pathEnding = FString::Printf(TEXT("_%d_%d"), startTileX + x, startTileY + y);
          // loadedTiles.Add(TileToData(pathEnding, splatmapNodes));

          WCLandscapeTile* tile = TileToData(pathEnding, splatmapNodes);
          //FString heightmapFileName = GetHeightmapPath(syncDir, pathEnding, version, HEIGHTMAP_FILEENDING);
          if (tile == nullptr)
          {
            continue;
          }

          uint16* currentHeightMap = (uint16*)tile->heightmap.GetData();
          mappingWidth = tile->width;
          mappingLength = tile->height;

          int xLeftOnTile = mappingWidth - tmpStartX;
          int yLeftOnTile = mappingLength - tmpStartY;
          constraintX = xLeftOnTile < widthLeft ? xLeftOnTile : widthLeft;
          constraintY = yLeftOnTile < lengthLeft ? yLeftOnTile : lengthLeft;
          for (int y2 = 0; y2 < constraintY; y2++)
          {
            for (int x2 = 0; x2 < constraintX; x2++)
            {
              int extractX = x2 + tmpStartX;
              int extractY = y2 + tmpStartY;
              int heightX = x2 + heightStartX;
              int heightY = y2 + heightStartY;

              int extractIdx = 0;
              if (version >= 3)
              {
                extractIdx = (mappingLength * mappingWidth - 1) - ((mappingWidth - extractX - 1) + extractY * mappingWidth);
              }
              else
              {
                extractIdx = (extractX + (extractY)*mappingWidth);
              }

              int insertIdx = 0;
              insertIdx = heightX * heightDataLength + heightY;// this hole thing is a idear for optimization

              heightData[insertIdx] = currentHeightMap[extractIdx];

              if (bImportLayers)
              {
                for (int j = 0; j < tile->splatmaps.Num(); j++)
                {
                  uint8* fileData = tile->splatmaps[j].GetData();
                  FXmlNode* curretnSplatMap = splatmapNodes[j];
                  int multV = multV = ((extractX)+extractY * mappingWidth) * tile->Bpp;
                  // int multV = ((currentTile.width * currentTile.height) - ((currentTile.height - 1 - extractY) * currentTile.width + extractX) - 1) * currentTile.Bpp;
                  int numCurrentSplatChilds = curretnSplatMap->GetChildrenNodes().Num();

                  for (int k = 0; k < numCurrentSplatChilds; k++)
                  {
                    int cuurentDataMapIdx = j * 4 + k;
                    uint8* currentDataMap = splatData[cuurentDataMapIdx].GetData();
                    switch (k)
                    {
                    case 0:
                      currentDataMap[insertIdx] = (fileData)[multV + 2];
                      break;
                    case 1:
                      currentDataMap[insertIdx] = (fileData)[multV + 1];
                      break;
                    case 2:
                      currentDataMap[insertIdx] = (fileData)[multV];
                      break;
                    case 3:
                      currentDataMap[insertIdx] = (fileData)[multV + 3];
                      break;
                    }
                  }
                }
              }
            }
          }
          lengthLeft -= constraintY;
          heightStartY += constraintY;
          tmpStartY = 0;
        }
        widthLeft -= constraintX;
        heightStartX += constraintX;
        tmpStartX = 0;
        tmpStartY = startY % WC_TILE_RESOLUTION;
      }
      int mapIdx = 0;
      TSharedPtr<LandscapeImportData> data = MakeShared<LandscapeImportData>();
      for (int i = 0; i < splatData.Num(); i++)
      {
        FString tmpPath = FString::Printf(TEXT("%s%s_layerinfo_%d_%d_%d"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), terrainName.GetCharArray().GetData(), i, tileX, tileY);
        FSoftObjectPath tmpSoftPath(tmpPath);
        ULandscapeLayerInfoObject* tmpLI = Cast<ULandscapeLayerInfoObject>(tmpSoftPath.TryLoad());
        if (tmpLI != nullptr)
        {
          ObjectTools::DeleteSingleObject(tmpLI);
        }
        UPackage* infoPackage = CreatePackage(*tmpPath);
        FName LayerObjectName = FName(*FString::Printf(TEXT("%s_layerinfo_%d_%d_%d"), terrainName.GetCharArray().GetData(), i, tileX, tileY));

        ULandscapeLayerInfoObject* layerInfoObject = NewObject<ULandscapeLayerInfoObject>(infoPackage, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
        FLandscapeImportLayerInfo layerInfo;
        FString layerInfoNameString = XmlHelper::GetString(splatmapNodes[i / 4]->GetChildrenNodes()[i % 4], "Name");
        FName layerInfoName = FName(*FString::Printf(TEXT("%d: %s"), i, layerInfoNameString.GetCharArray().GetData()));
        layerInfo.LayerData = splatData[i];
        layerInfo.LayerName = layerInfoName;// FString::Printf(TEXT("Texture%d"), i).GetCharArray().GetData();//FName(FString::Printf(TEXT("%s"), textures[i]->GetAttribute("Name").GetCharArray().GetData()).GetCharArray().GetData());

        layerInfo.LayerInfo = layerInfoObject;
        layerInfo.LayerInfo->bNoWeightBlend = 0;
        layerInfo.LayerInfo->Hardness = 1;
        layerInfo.LayerInfo->IsReferencedFromLoadedData = false;
        layerInfo.LayerInfo->LayerName = layerInfo.LayerName;
        layerInfos.Add(layerInfo);

        infoPackage->FullyLoad();
        infoPackage->SetDirtyFlag(true);
        FAssetRegistryModule::AssetCreated(layerInfoObject);

      }
      data->material = CreateLandscapeMaterial(landscapeId, numLoadedXTiles, numLoadedYTiles, startX, startY, mappingWidth, mappingLength);
      //data->material = CreateLandscapeMaterial(landscapeId, numLoadedXTiles, numLoadedYTiles, startX, startY, currentTile.width, currentTile.height);
      data->heightData = heightData;
      data->scaleX = m_scaleX;
      data->scaleY = m_scaleY;
      data->terrainScale = terrainScale;
      //data.material = nullptr; // TODO remove for mat
      data->layerInfos = layerInfos;
      data->quatsPerSection = quatsPerSection;
      ImportHeightMapToLandscape(world, data, heightDataLength, heightDataWidth, landscapeId, *location, *rotation);
      landscapeId++;

      startY += heightDataLength;
      int tmpLength = length - (tileY + 1) * unrealTerrainResolution;
      heightDataLength = tmpLength < unrealTerrainResolution ? tmpLength : unrealTerrainResolution;

      // originalHeightDataLength = heightDataLength;
      heightDataLength = RecaulculateToUnrealSize(quatsPerSection, heightDataLength);
    }

    startY = 0;
    startX += heightDataWidth;
    int tmpWidth = width - (tileX + 1) * unrealTerrainResolution;
    heightDataWidth = tmpWidth < unrealTerrainResolution ? tmpWidth : unrealTerrainResolution;
    // originalHeightDataWidth = heightDataWidth;
    heightDataWidth = RecaulculateToUnrealSize(quatsPerSection, heightDataWidth);
  }


  if (bBuildMinimap)// && bUseWorldPartition) // TODO fix it. try running it on a seperate frame or try running it with allowing unreal to run in between 
  {
    FEditorBuildUtils::EditorBuild(world, FBuildOptions::BuildMinimap);
    //FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("World Creator Sync", "Due to a current bug in the building of a minimap via code the building on the minimap cannot currently be handled by the World Creator Bridge tools syncronization. Please use the button below to Build the minimap for your project"));
  }

  //    // Cleanup memory  
   //    ////////////////
  configFile.Clear();
  return FReply::Handled();
}

WCLandscapeTile* FWorldCreatorBridgeModule::TileToData(FString pathEnding, TArray<FXmlNode*> texturNodes)
{
  TArray<TArray<uint8>> splatmaps;
  TArray<uint8> heightmap;
  WCLandscapeTile* tile = new WCLandscapeTile();
  for (int i = 0; i < texturNodes.Num(); i++)
  {
    FXmlNode* child = texturNodes[i];
    int splatmapIndex = XmlHelper::GetInt(child, "Index");
    splatmapIndex = FMath::Max(0, splatmapIndex);
    FString filePath;
    if (version >= 3 && splatmapIndex >= 0) // THE && condition can be removed in future versions, it is a temporary solution for the 1.1 version (2023.1.1b) of the beta
    {
      filePath = FString::Printf(TEXT("%s/splatmap_%d%s.%s"), syncDir.GetCharArray().GetData(), splatmapIndex, pathEnding.GetCharArray().GetData(), SPLATMAP_FILEENDING);
    }
    else
    {
      FString fileName = XmlHelper::GetString(child, "Name");
      filePath = FString::Printf(TEXT("%s/%s"), syncDir.GetCharArray().GetData(), fileName.GetCharArray().GetData());
    }

    int fileWidth, fileHeight, fileBpp;
    TArray<uint8> fileData;

    FFileHelper::LoadFileToArray(fileData, filePath.GetCharArray().GetData());

    uint8* dataPtr = (fileData).GetData();
    if (dataPtr == nullptr)
    {
      return nullptr;
    }

    fileWidth = *(short*)(&dataPtr[12]);
    fileHeight = *(short*)(&dataPtr[14]);
    fileBpp = (*(uint8*)(&dataPtr[16])) / 8;
    if (i == 0)
    {
      tile->height = fileHeight;
      tile->width = fileWidth;
      tile->Bpp = fileBpp;
    }
    (fileData).RemoveAt(0, 18);
    splatmaps.Add(fileData);
  }
  tile->splatmaps = splatmaps;

  FString heightmapPath;
  if (version >= 3)
  {
    heightmapPath = FString::Printf(TEXT("%s/heightmap%s.%s"), syncDir.GetCharArray().GetData(), pathEnding.GetCharArray().GetData(), &HEIGHTMAP_FILEENDING);
  }
  else
  {
    heightmapPath = FString::Printf(TEXT("%s/heightmap.%s"), syncDir.GetCharArray().GetData(), &HEIGHTMAP_FILEENDING);
    if (texturNodes.Num() > 0)
    {
      width = tile->width;
      length = tile->height;
    }
    else
    {
      width = resX;
      length = resY;
      tile->width = width;
      tile->height = length;
    }
  }
  FFileHelper::LoadFileToArray(heightmap, heightmapPath.GetCharArray().GetData());
  tile->heightmap = heightmap;
  return tile;
}

FReply FWorldCreatorBridgeModule::BuildMinimapButtonClicked()
{
  auto context = GEditor->GetEditorWorldContext();
  auto world = context.World();
  FEditorBuildUtils::EditorBuild(world, FBuildOptions::BuildMinimap);
  return FReply::Handled();
}

FReply FWorldCreatorBridgeModule::BrowseButtonClicked()
{
  TArray<FString> filenames;
  if (FDesktopPlatformModule::Get()->OpenFileDialog(nullptr,
    TEXT("Select World Creator Bridge .xml file "), TEXT(""), TEXT(""), TEXT("World Creator Bridge File | *.xml"), 0, filenames))
  {
    this->selectedPath = filenames[0];
    selectedPathBox->SetText(FText::FromString(selectedPath));
  }
  return FReply::Handled();
}

UMaterial* FWorldCreatorBridgeModule::CreateLandscapeMaterial(int terrainId, int _numTilesX, int _numTilesY, int startX, int startY, int mappingWidth, int mappingLength)
{
  /*FString tmpMatPath = FString::Printf(TEXT("%s%s_%d"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), this->terrainMaterialName.GetCharArray().GetData(), terrainId);
  FSoftObjectPath matPath(tmpMatPath);
  UMaterial* tmpMat = Cast<UMaterial>(matPath.TryLoad());
  if (tmpMat != nullptr)
  {
    ObjectTools::DeleteSingleObject(tmpMat);
  }*/
  /// Create Texture
  /////////////////////
  FString MaterialPackageName = MATERIAL_PACKAGE_NAME_PREFIX + this->terrainMaterialName;
  UPackage* materialPackage = CreatePackage(*MaterialPackageName);
  auto MaterialFactory = NewObject<UMaterialFactoryNew>();
  // UMaterial* material = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), materialPackage, *(FString::Printf(TEXT("%s_%d"), this->terrainMaterialName.GetCharArray().GetData(), terrainId)), RF_Standalone | RF_Public, NULL, GWarn);
  UMaterial* material = NewObject<UMaterial>(materialPackage, *(this->terrainMaterialName), RF_Public | RF_Standalone | RF_Transactional);
  FAssetRegistryModule::AssetCreated(material);
  materialPackage->FullyLoad();
  materialPackage->SetDirtyFlag(true);  

  FMaterialExpressionCollection* expressionCollection = &material->GetExpressionCollection();
  auto expressioncopy = expressionCollection->Expressions;
  if (bImportTextures)
  {
    for (UMaterialExpression* v : expressioncopy)
    {
      expressionCollection->RemoveExpression(v);
    }
  }
  expressioncopy.Empty();

  /// create layerblend nodes in the material
  /////////////////////////////////////////////
  UMaterialExpressionLandscapeLayerBlend* AlbedoLayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(material);
  AlbedoLayerBlend->MaterialExpressionEditorX = -200.0f;
  AlbedoLayerBlend->MaterialExpressionEditorY = 50.0f;
  expressionCollection->AddExpression(AlbedoLayerBlend);


  UMaterialExpressionLandscapeLayerBlend* NormalLayerBlend = nullptr;
  UMaterialExpressionLandscapeLayerBlend* AOLayerBlend = nullptr;
  UMaterialExpressionLandscapeLayerBlend* DisplacementLayerBlend = nullptr;
  UMaterialExpressionLandscapeLayerBlend* RoughnessLayerBlend = nullptr;

  if (bImportLayers)
  {
    NormalLayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(material);
    NormalLayerBlend->MaterialExpressionEditorX = -200.0f;
    NormalLayerBlend->MaterialExpressionEditorY = 250.0f;
    expressionCollection->AddExpression(NormalLayerBlend);

    AOLayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(material);
    AOLayerBlend->MaterialExpressionEditorX = -200.0f;
    AOLayerBlend->MaterialExpressionEditorY = 450.0f;
    expressionCollection->AddExpression(AOLayerBlend);

    DisplacementLayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(material);
    DisplacementLayerBlend->MaterialExpressionEditorX = -200.0f;
    DisplacementLayerBlend->MaterialExpressionEditorY = 350.0f;
    expressionCollection->AddExpression(DisplacementLayerBlend);

    RoughnessLayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(material);
    RoughnessLayerBlend->MaterialExpressionEditorX = -200.0f;
    RoughnessLayerBlend->MaterialExpressionEditorY = 150.0f;
    expressionCollection->AddExpression(RoughnessLayerBlend);
  }

  /// add and remap landscape coordinates to the material
  /////////////////////////////////////////////////////////
  UMaterialExpressionLandscapeLayerCoords* LandscapeCoords = NewObject<UMaterialExpressionLandscapeLayerCoords>(material);

  //LandscapeCoords->MappingRotation = 180.0f; // TODO this is a test because i rotated the terrain 
  float originalLandscapeCoordX = -2600.0f;
  LandscapeCoords->MaterialExpressionEditorX = originalLandscapeCoordX;
  LandscapeCoords->MaterialExpressionEditorY = -350.0f;
  expressionCollection->AddExpression(LandscapeCoords);
  /// Create empty texture nodes for the cases where a layer only has colordata. Leaving layers in a layerblend emplty has resulted in render issues
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  UMaterialExpressionVectorParameter* emptyNormalVectorParam = NewObject<UMaterialExpressionVectorParameter>(material);
  emptyNormalVectorParam->ParameterName = FName(FString::Printf(TEXT("emptyNormal")));
  emptyNormalVectorParam->MaterialExpressionEditorX = -1500.0f;
  emptyNormalVectorParam->MaterialExpressionEditorY = -400.0f;
  expressionCollection->AddExpression(emptyNormalVectorParam);
  material->SetVectorParameterValueEditorOnly(FString::Printf(TEXT("emptyNormal")).GetCharArray().GetData(), FColor(128, 128, 255, 255));
  UMaterialExpressionVectorParameter* emptyAOVectorParam = NewObject<UMaterialExpressionVectorParameter>(material);
  emptyAOVectorParam->ParameterName = FName(FString::Printf(TEXT("emptyAO")));
  emptyAOVectorParam->MaterialExpressionEditorX = -1500.0f;
  emptyAOVectorParam->MaterialExpressionEditorY = -1000.0f;
  expressionCollection->AddExpression(emptyAOVectorParam);
  material->SetVectorParameterValueEditorOnly(FString::Printf(TEXT("emptyAO")).GetCharArray().GetData(), FColor(255, 255, 255, 255));
  UMaterialExpressionVectorParameter* emptyDisplacementVectorParam = NewObject<UMaterialExpressionVectorParameter>(material);
  emptyDisplacementVectorParam->ParameterName = FName(FString::Printf(TEXT("emptyDisplacement")));
  emptyDisplacementVectorParam->MaterialExpressionEditorX = -1500.0f;
  emptyDisplacementVectorParam->MaterialExpressionEditorY = -800.0f;
  expressionCollection->AddExpression(emptyDisplacementVectorParam);
  material->SetVectorParameterValueEditorOnly(FString::Printf(TEXT("emptyDisplacement")).GetCharArray().GetData(), FColor(0, 0, 0, 255));
  UMaterialExpressionVectorParameter* emptyRoughnessVectorParam = NewObject<UMaterialExpressionVectorParameter>(material);
  emptyRoughnessVectorParam->ParameterName = FName(FString::Printf(TEXT("emptyRoughness")));
  emptyRoughnessVectorParam->MaterialExpressionEditorX = -1500.0f;
  emptyRoughnessVectorParam->MaterialExpressionEditorY = -600.0f;
  expressionCollection->AddExpression(emptyRoughnessVectorParam);
  material->SetVectorParameterValueEditorOnly(FString::Printf(TEXT("emptyRoughness")).GetCharArray().GetData(), FColor(255, 255, 255, 255));
  UMaterialExpressionConstant* zeroConstant = NewObject<UMaterialExpressionConstant>(material);
  zeroConstant->R = 0.0f;
  zeroConstant->MaterialExpressionEditorX = -1000.0f;
  zeroConstant->MaterialExpressionEditorY = -600;
  expressionCollection->AddExpression(zeroConstant);

  int startTileX = startX / WC_TILE_RESOLUTION;
  int startTileY = startY / WC_TILE_RESOLUTION;

  UMaterialExpressionAdd* addedTextrueExpression = nullptr;
  int tileMatXPos = -500;
  int tileMatYPos = 0;


  UMaterialExpressionConstant* MO = NewObject<UMaterialExpressionConstant>(material);
  MO->R = -1;
  expressionCollection->AddExpression(MO);
  MO->MaterialExpressionEditorX = -1000.0f;
  MO->MaterialExpressionEditorY = -200;

  UMaterialExpressionConstant* oneHundred = NewObject<UMaterialExpressionConstant>(material);
  oneHundred->R = 100.0f;
  oneHundred->MaterialExpressionEditorX = -1000.0f;
  oneHundred->MaterialExpressionEditorY = -300;
  expressionCollection->AddExpression(oneHundred);
  UMaterialExpressionConstant* widthConstant = NewObject<UMaterialExpressionConstant>(material);
  widthConstant->R = mappingWidth;
  widthConstant->MaterialExpressionEditorX = -1000.0f;
  widthConstant->MaterialExpressionEditorY = -400;
  expressionCollection->AddExpression(widthConstant);
  UMaterialExpressionConstant* lengthConstant = NewObject<UMaterialExpressionConstant>(material);
  lengthConstant->R = mappingLength;
  lengthConstant->MaterialExpressionEditorX = -1000.0f;
  lengthConstant->MaterialExpressionEditorY = -500;
  expressionCollection->AddExpression(lengthConstant);

  UMaterialExpressionComponentMask* maskLandscapeCoordX = NewObject<UMaterialExpressionComponentMask>(material);
  maskLandscapeCoordX->R = 1;
  maskLandscapeCoordX->G = 0;
  maskLandscapeCoordX->B = 0;
  maskLandscapeCoordX->A = 0;
  maskLandscapeCoordX->Input.Expression = LandscapeCoords;
  maskLandscapeCoordX->MaterialExpressionEditorX = -2400.0f;
  maskLandscapeCoordX->MaterialExpressionEditorY = -400;
  expressionCollection->AddExpression(maskLandscapeCoordX);
  UMaterialExpressionComponentMask* maskLandscapeCoordY = NewObject<UMaterialExpressionComponentMask>(material);
  maskLandscapeCoordY->R = 0;
  maskLandscapeCoordY->G = 1;
  maskLandscapeCoordY->B = 0;
  maskLandscapeCoordY->A = 0;
  maskLandscapeCoordY->Input.Expression = LandscapeCoords;
  maskLandscapeCoordY->MaterialExpressionEditorX = -2400.0f;
  maskLandscapeCoordY->MaterialExpressionEditorY = -300;
  expressionCollection->AddExpression(maskLandscapeCoordY);

  UMaterialExpressionDivide* landscapeCoordXDivide = NewObject <UMaterialExpressionDivide>(material);
  UMaterialExpressionDivide* landscapeCoordYDivide = NewObject <UMaterialExpressionDivide>(material);
  landscapeCoordXDivide->A.Expression = maskLandscapeCoordX;
  landscapeCoordYDivide->A.Expression = maskLandscapeCoordY;
  landscapeCoordXDivide->B.Expression = lengthConstant;
  landscapeCoordYDivide->B.Expression = widthConstant;
  landscapeCoordYDivide->MaterialExpressionEditorX = -2200.0f;
  landscapeCoordYDivide->MaterialExpressionEditorY = -300;
  landscapeCoordXDivide->MaterialExpressionEditorX = -2200.0f;
  landscapeCoordXDivide->MaterialExpressionEditorY = -400;
  expressionCollection->AddExpression(landscapeCoordXDivide);
  expressionCollection->AddExpression(landscapeCoordYDivide);

  UMaterialExpressionAppendVector* fittetLandscapeCoord = NewObject<UMaterialExpressionAppendVector>(material);
  fittetLandscapeCoord->A.Expression = landscapeCoordXDivide;
  fittetLandscapeCoord->B.Expression = landscapeCoordYDivide;
  fittetLandscapeCoord->MaterialExpressionEditorX = -2000.0f;
  fittetLandscapeCoord->MaterialExpressionEditorY = -350;
  expressionCollection->AddExpression(fittetLandscapeCoord);


  for (int x = 0; x < _numTilesX; x++)
  {
    for (int y = 0; y < _numTilesY; y++)
    {
      int tmpx = x + startTileX;
      int tmpy = y + startTileY;
      FString baseMapName = bImportLayers ? COLORMAP_NAME : TEXTUREMAP_NAME;
      FString diffuseAssetPath;
      if (version >= 3)
      {
        diffuseAssetPath = FString::Printf(TEXT("%s%s_%d_%d"),
          MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(),
          baseMapName.GetCharArray().GetData(),
          tmpx, tmpy);
      }
      else
      {
        diffuseAssetPath = FString::Printf(TEXT("%s%s"),
          MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(),
          baseMapName.GetCharArray().GetData());
      }

      // add a vector parametarization here 
      UMaterialExpressionConstant2Vector* landscapeUVOffset = NewObject<UMaterialExpressionConstant2Vector>(material);
      landscapeUVOffset->G = startX % WC_TILE_RESOLUTION - (mappingWidth * x);
      landscapeUVOffset->R = startY % WC_TILE_RESOLUTION - (mappingLength * y);
      landscapeUVOffset->MaterialExpressionEditorX = originalLandscapeCoordX + 100 + tileMatXPos;
      landscapeUVOffset->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(landscapeUVOffset);

      UMaterialExpressionAdd* addExpression = NewObject<UMaterialExpressionAdd>(material);
      expressionCollection->AddExpression(addExpression);
      addExpression->A.Expression = LandscapeCoords;
      addExpression->B.Expression = landscapeUVOffset;
      addExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 250 + tileMatXPos;
      addExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      LandscapeCoords->MaterialExpressionEditorX = originalLandscapeCoordX + tileMatXPos;

      UMaterialExpressionComponentMask* maskLandscapeX = NewObject<UMaterialExpressionComponentMask>(material);
      maskLandscapeX->R = 1;
      maskLandscapeX->G = 0;
      maskLandscapeX->B = 0;
      maskLandscapeX->A = 0;
      maskLandscapeX->Input.Expression = addExpression;
      expressionCollection->AddExpression(maskLandscapeX);
      UMaterialExpressionComponentMask* maskLandscapeY = NewObject<UMaterialExpressionComponentMask>(material);
      maskLandscapeY->R = 0;
      maskLandscapeY->G = 1;
      maskLandscapeY->B = 0;
      maskLandscapeY->A = 0;
      maskLandscapeY->Input.Expression = addExpression;
      expressionCollection->AddExpression(maskLandscapeY);
      maskLandscapeX->MaterialExpressionEditorX = originalLandscapeCoordX + 400 + tileMatXPos;
      maskLandscapeX->MaterialExpressionEditorY = tileMatYPos + 150.0f;
      maskLandscapeY->MaterialExpressionEditorX = originalLandscapeCoordX + 400 + tileMatXPos;
      maskLandscapeY->MaterialExpressionEditorY = tileMatYPos + 250.0f;

      UMaterialExpressionMin* startMin = NewObject<UMaterialExpressionMin>(material);
      startMin->A.Expression = maskLandscapeY;
      startMin->B.Expression = maskLandscapeX;
      startMin->MaterialExpressionEditorX = originalLandscapeCoordX + 550 + tileMatXPos;
      startMin->MaterialExpressionEditorY = tileMatYPos + 150.0f;
      expressionCollection->AddExpression(startMin);

      UMaterialExpressionDivide* widthDivider = NewObject < UMaterialExpressionDivide>(material);
      widthDivider->A.Expression = maskLandscapeY;
      widthDivider->B.Expression = widthConstant;
      widthDivider->MaterialExpressionEditorX = originalLandscapeCoordX + 650 + tileMatXPos;
      widthDivider->MaterialExpressionEditorY = tileMatYPos + 250.0f;
      expressionCollection->AddExpression(widthDivider);

      UMaterialExpressionMultiply* MOmultLandscapeX = NewObject<UMaterialExpressionMultiply>(material);
      MOmultLandscapeX->A.Expression = maskLandscapeX;
      MOmultLandscapeX->B.Expression = MO;
      MOmultLandscapeX->MaterialExpressionEditorX = originalLandscapeCoordX + 550 + tileMatXPos;
      MOmultLandscapeX->MaterialExpressionEditorY = tileMatYPos + 0.0f;
      expressionCollection->AddExpression(MOmultLandscapeX);

      UMaterialExpressionAdd* newLandscapeX = NewObject<UMaterialExpressionAdd>(material);
      newLandscapeX->A.Expression = lengthConstant;
      newLandscapeX->B.Expression = MOmultLandscapeX;
      newLandscapeX->MaterialExpressionEditorX = originalLandscapeCoordX + 700 + tileMatXPos;
      newLandscapeX->MaterialExpressionEditorY = tileMatYPos + 0.0f;
      expressionCollection->AddExpression(newLandscapeX);

      UMaterialExpressionDivide* lengthDivider = NewObject < UMaterialExpressionDivide>(material);
      lengthDivider->A.Expression = newLandscapeX;
      lengthDivider->B.Expression = lengthConstant;
      lengthDivider->MaterialExpressionEditorX = originalLandscapeCoordX + 850 + tileMatXPos;
      lengthDivider->MaterialExpressionEditorY = tileMatYPos + 150.0f;
      expressionCollection->AddExpression(lengthDivider);

      UMaterialExpressionAppendVector* newLandscapeUV = NewObject<UMaterialExpressionAppendVector>(material);
      newLandscapeUV->A.Expression = widthDivider;
      newLandscapeUV->B.Expression = lengthDivider;
      newLandscapeUV->MaterialExpressionEditorX = originalLandscapeCoordX + 1000 + tileMatXPos;
      newLandscapeUV->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(newLandscapeUV);

      FSoftObjectPath DiffuseAssetPath(diffuseAssetPath);
      UTexture* DiffuseTexture = Cast<UTexture>(DiffuseAssetPath.TryLoad());
      UMaterialExpressionTextureSample* TextureExpression = NewObject<UMaterialExpressionTextureSample>(material);
      TextureExpression->Texture = DiffuseTexture;
      TextureExpression->SamplerType = SAMPLERTYPE_Color;
      expressionCollection->AddExpression(TextureExpression);
      TextureExpression->Coordinates.Expression = newLandscapeUV;
      TextureExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 1150.0f + tileMatXPos;
      TextureExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;


      UMaterialExpressionMultiply* multByMO = NewObject<UMaterialExpressionMultiply>(material);

      multByMO->A.Expression = addExpression;
      multByMO->B.Expression = MO;
      multByMO->MaterialExpressionEditorX = originalLandscapeCoordX + 1300.0f + tileMatXPos;
      multByMO->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(multByMO);

      UMaterialExpressionAppendVector* mappingVec = NewObject<UMaterialExpressionAppendVector>(material);
      mappingVec->A.Expression = lengthConstant;
      mappingVec->B.Expression = widthConstant;
      mappingVec->MaterialExpressionEditorX = originalLandscapeCoordX + 1450.0f + tileMatXPos;
      mappingVec->MaterialExpressionEditorY = tileMatYPos + 100.0f;
      expressionCollection->AddExpression(mappingVec);

      UMaterialExpressionAdd* clipValueExpression = NewObject<UMaterialExpressionAdd>(material);
      clipValueExpression->A.Expression = mappingVec;
      clipValueExpression->B.Expression = multByMO;

      clipValueExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 1600.0f + tileMatXPos;
      clipValueExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(clipValueExpression);

      UMaterialExpressionComponentMask* maskR = NewObject<UMaterialExpressionComponentMask>(material);
      maskR->R = 1;
      maskR->G = 0;
      maskR->B = 0;
      maskR->A = 0;
      maskR->Input.Expression = clipValueExpression;
      expressionCollection->AddExpression(maskR);
      UMaterialExpressionComponentMask* maskG = NewObject<UMaterialExpressionComponentMask>(material);
      maskG->R = 0;
      maskG->G = 1;
      maskG->B = 0;
      maskG->A = 0;
      maskG->Input.Expression = clipValueExpression;
      expressionCollection->AddExpression(maskG);

      maskR->MaterialExpressionEditorX = originalLandscapeCoordX + 1750.0f + tileMatXPos;
      maskR->MaterialExpressionEditorY = tileMatYPos + 150.0f;
      maskG->MaterialExpressionEditorX = originalLandscapeCoordX + 1750.0f + tileMatXPos;
      maskG->MaterialExpressionEditorY = tileMatYPos + 250.0f;

      UMaterialExpressionMin* endMin = NewObject<UMaterialExpressionMin>(material);
      endMin->A.Expression = maskR;
      endMin->B.Expression = maskG;
      endMin->MaterialExpressionEditorX = originalLandscapeCoordX + 1900.0f + tileMatXPos;
      endMin->MaterialExpressionEditorY = tileMatYPos + 250.0f;
      expressionCollection->AddExpression(endMin);

      UMaterialExpressionMin* minExpression = NewObject<UMaterialExpressionMin>(material);
      minExpression->A.Expression = endMin;
      minExpression->B.Expression = startMin;
      minExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 2050.0f + tileMatXPos;
      minExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(minExpression);

      UMaterialExpressionMultiply* multiplyerExpression = NewObject<UMaterialExpressionMultiply>(material);
      multiplyerExpression->A.Expression = minExpression;
      multiplyerExpression->B.Expression = oneHundred;
      multiplyerExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 2200.0f + tileMatXPos;
      multiplyerExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(multiplyerExpression);

      UMaterialExpressionClamp* clampExpression = NewObject<UMaterialExpressionClamp>(material);
      clampExpression->Input.Expression = multiplyerExpression;
      clampExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 2350.0f + tileMatXPos;
      clampExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;
      expressionCollection->AddExpression(clampExpression);


      UMaterialExpressionMultiply* finalTextureExpression = NewObject<UMaterialExpressionMultiply>(material);
      finalTextureExpression->A.Expression = TextureExpression;
      finalTextureExpression->B.Expression = clampExpression;
      finalTextureExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 2300.0f + tileMatXPos;
      finalTextureExpression->MaterialExpressionEditorY = tileMatYPos + 0.0f;
      expressionCollection->AddExpression(finalTextureExpression);

      UMaterialExpressionAdd* tmpAddedTextureExpression = NewObject<UMaterialExpressionAdd>(material);
      tmpAddedTextureExpression->A.Expression = finalTextureExpression;
      tmpAddedTextureExpression->B.Expression = zeroConstant;
      tmpAddedTextureExpression->MaterialExpressionEditorX = originalLandscapeCoordX + 2650 + tileMatXPos;
      tmpAddedTextureExpression->MaterialExpressionEditorY = tileMatYPos + 200.0f;

      if (addedTextrueExpression != nullptr)
      {
        tmpAddedTextureExpression->B.Expression = addedTextrueExpression;
      }
      expressionCollection->AddExpression(tmpAddedTextureExpression);
      addedTextrueExpression = tmpAddedTextureExpression;
      tileMatXPos -= 700.0f;
      tileMatYPos += 400.0f;
    }
  }

  //// Load Splatmap & Textures
  ///////////////////////////////


  //// TODO for Tomorrow this can be put into a function and added to the loop below
  if (bImportLayers)
  {
    auto texturingNode = root->FindChildNode(TEXT("Texturing"));
    if (texturingNode != nullptr)
    {
      //Load From File
      auto childNodes = texturingNode->GetChildrenNodes();
      int textureCount = 0;

      int currentlayerindex = 0;
      for (auto child : childNodes)
      {
        auto textures = child->GetChildrenNodes();
        for (int i = 0; i < textures.Num(); i++)
        {
          FString texturename = XmlHelper::GetString(textures[i], "Name");
          float tilescaleX;
          float tilescaleY;
          float tileoffsetX;
          float tileoffsetY;
          XmlHelper::GetFloat2(textures[i], "TileSize", &tilescaleX, &tilescaleY);
          XmlHelper::GetFloat2(textures[i], "TileOffset", &tileoffsetX, &tileoffsetY);
          tilescaleX = mappingLength / tilescaleX;
          tilescaleY = mappingWidth / tilescaleY;
          tileoffsetX /= mappingLength;
          tileoffsetY /= mappingWidth;

          FLayerBlendInput albedoLayerBlendInput;
          albedoLayerBlendInput.LayerName = FName(*FString::Printf(TEXT("%d: %s"), textureCount, texturename.GetCharArray().GetData())); // FString::Printf(TEXT("Texture%d"), textureCount).GetCharArray().GetData();
          AlbedoLayerBlend->Layers.Add(albedoLayerBlendInput);
          AlbedoLayerBlend->GetInput(currentlayerindex)->Expression = addedTextrueExpression;

          FLayerBlendInput normalLayerBlendInput;
          normalLayerBlendInput.LayerName = FName(*FString::Printf(TEXT("%d: %s"), textureCount, texturename.GetCharArray().GetData())); // FString::Printf(TEXT("Texture%d"), textureCount).GetCharArray().GetData();
          NormalLayerBlend->Layers.Add(normalLayerBlendInput);
          NormalLayerBlend->GetInput(currentlayerindex)->Expression = emptyNormalVectorParam;
          FLayerBlendInput aoLayerBlendInput;
          aoLayerBlendInput.LayerName = FName(*FString::Printf(TEXT("%d: %s"), textureCount, texturename.GetCharArray().GetData())); // FString::Printf(TEXT("Texture%d"), textureCount).GetCharArray().GetData();
          AOLayerBlend->Layers.Add(aoLayerBlendInput);
          AOLayerBlend->GetInput(currentlayerindex)->Expression = emptyAOVectorParam;
          FLayerBlendInput displacementLayerBlendInput;
          displacementLayerBlendInput.LayerName = FName(*FString::Printf(TEXT("%d: %s"), textureCount, texturename.GetCharArray().GetData())); // FString::Printf(TEXT("Texture%d"), textureCount).GetCharArray().GetData();
          DisplacementLayerBlend->Layers.Add(displacementLayerBlendInput);
          DisplacementLayerBlend->GetInput(currentlayerindex)->Expression = emptyDisplacementVectorParam;
          FLayerBlendInput roughnessLayerBlendInput;
          roughnessLayerBlendInput.LayerName = FName(*FString::Printf(TEXT("%d: %s"), textureCount, texturename.GetCharArray().GetData())); // FString::Printf(TEXT("Texture%d"), textureCount).GetCharArray().GetData();
          RoughnessLayerBlend->Layers.Add(roughnessLayerBlendInput);
          RoughnessLayerBlend->GetInput(currentlayerindex)->Expression = emptyRoughnessVectorParam;


          auto textureNode = textures[i];

          FString colorAtt = textureNode->GetAttribute(TEXT("Color")).GetCharArray().GetData();

          colorAtt.RemoveFromStart(TEXT("#ff"));
          FLinearColor color = FLinearColor(FColor::FromHex(colorAtt));


          UMaterialExpressionVectorParameter* vectorParam = NewObject<UMaterialExpressionVectorParameter>(material);
          vectorParam->ParameterName = FName(FString::Printf(TEXT("Color%d"), textureCount));
          expressionCollection->AddExpression(vectorParam);
          material->SetVectorParameterValueEditorOnly(FString::Printf(TEXT("Color%d"), textureCount).GetCharArray().GetData(), color);

          auto connectTextureLambda = [expressionCollection, material, fittetLandscapeCoord, vectorParam, tilescaleX, tilescaleY, tileoffsetX, tileoffsetY](UTexture2D* currentTexture)
            {
              UMaterialExpressionTextureSample* currentExpression = NewObject<UMaterialExpressionTextureSample>(material);
              currentExpression->Texture = currentTexture;
              expressionCollection->Expressions.Add(currentExpression);
              UMaterialExpressionMultiply* multExp = NewObject<UMaterialExpressionMultiply>(material);
              UMaterialExpressionConstant2Vector* vec2Exp = NewObject<UMaterialExpressionConstant2Vector>(material);
              UMaterialExpressionConstant2Vector* vec2off = NewObject<UMaterialExpressionConstant2Vector>(material);
              expressionCollection->Expressions.Add(multExp);
              expressionCollection->Expressions.Add(vec2Exp);
              expressionCollection->Expressions.Add(vec2off);
              vec2Exp->R = tilescaleX;
              vec2Exp->G = tilescaleY;
              vec2off->R = tileoffsetX;
              vec2off->G = tileoffsetY;
              UMaterialExpressionAdd* offsetLandscapeCoords = NewObject<UMaterialExpressionAdd>(material);
              offsetLandscapeCoords->A.Expression = vec2off;
              offsetLandscapeCoords->B.Expression = fittetLandscapeCoord;
              expressionCollection->Expressions.Add(offsetLandscapeCoords);
              multExp->A.Expression = offsetLandscapeCoords;
              multExp->B.Expression = vec2Exp;
              currentExpression->Coordinates.Expression = multExp;
              return currentExpression;
            };

          TArray<FString> TexturePaths;
          auto albedoFile = textureNode->GetAttribute(TEXT("AlbedoFile"));
          auto normalFile = textureNode->GetAttribute(TEXT("NormalFile"));
          auto aoFile = textureNode->GetAttribute(TEXT("AoFile"));
          auto displacementFile = textureNode->GetAttribute(TEXT("DisplacementFile"));
          auto roughnessFile = textureNode->GetAttribute(TEXT("RoughnessFile"));

          // has to be done this way because FFileManagerGeneric::FileExists does not work
          if (!albedoFile.IsEmpty())
          {
            albedoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
            albedoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
            FSoftObjectPath currentAssetPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), albedoFile.GetCharArray().GetData()));
            UTexture2D* currentTex = Cast<UTexture2D>(currentAssetPath.TryLoad());
            if (currentTex)
            {
              UMaterialExpressionMultiply* multEx = NewObject<UMaterialExpressionMultiply>(material);
              multEx->A.Expression = connectTextureLambda(currentTex);
              multEx->B.Expression = vectorParam;

              AlbedoLayerBlend->GetInput(currentlayerindex)->Expression = multEx;
            }
          }
          if (!normalFile.IsEmpty())
          {
            normalFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
            normalFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
            FSoftObjectPath currentAssetPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), normalFile.GetCharArray().GetData()));
            UTexture2D* currentTex = Cast<UTexture2D>(currentAssetPath.TryLoad());
            if (currentTex)
            {
              currentTex->SRGB = 0;
              currentTex->CompressionSettings = TC_Normalmap;
              auto ne = connectTextureLambda(currentTex);
              NormalLayerBlend->GetInput(currentlayerindex)->Expression = ne;
              ne->SamplerType = SAMPLERTYPE_Normal;
            }
          }
          if (!aoFile.IsEmpty())
          {
            aoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
            aoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
            FSoftObjectPath currentAssetPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), aoFile.GetCharArray().GetData()));
            UTexture2D* currentTex = Cast<UTexture2D>(currentAssetPath.TryLoad());
            if (currentTex)
              AOLayerBlend->GetInput(currentlayerindex)->Expression = connectTextureLambda(currentTex);
          }
          if (!displacementFile.IsEmpty())
          {
            displacementFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
            displacementFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
            FSoftObjectPath currentAssetPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), displacementFile.GetCharArray().GetData()));
            UTexture2D* currentTex = Cast<UTexture2D>(currentAssetPath.TryLoad());
            if (currentTex)
            {
              UMaterialExpressionTextureSample* nd = connectTextureLambda(currentTex);
              nd->SamplerType = SAMPLERTYPE_LinearColor;
              DisplacementLayerBlend->GetInput(currentlayerindex)->Expression = nd;
            }
          }
          if (!roughnessFile.IsEmpty())
          {
            roughnessFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
            roughnessFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
            FSoftObjectPath currentAssetPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), roughnessFile.GetCharArray().GetData()));
            UTexture2D* currentTex = Cast<UTexture2D>(currentAssetPath.TryLoad());
            if (currentTex)
              RoughnessLayerBlend->GetInput(currentlayerindex)->Expression = connectTextureLambda(currentTex);
          }

          textureCount++;
          currentlayerindex++;
        }
      }
    }
  }
  else
  {
    FLayerBlendInput albedoLayerBlendInput;
    albedoLayerBlendInput.LayerName = "Texture0";
    AlbedoLayerBlend->Layers.Add(albedoLayerBlendInput);
    AlbedoLayerBlend->GetInput(0)->Expression = addedTextrueExpression;
  }


  //// Assign Color Expressions to result node
  /////////////////////////////////////////////////
  material->AssignExpressionCollection(*expressionCollection);
  material->GetEditorOnlyData()->BaseColor.Expression = AlbedoLayerBlend;
  material->GetEditorOnlyData()->Normal.Expression = NormalLayerBlend;
  material->GetEditorOnlyData()->AmbientOcclusion.Expression = AOLayerBlend;
  material->GetEditorOnlyData()->WorldPositionOffset.Expression = DisplacementLayerBlend;
  material->GetEditorOnlyData()->Roughness.Expression = RoughnessLayerBlend;

  expressioncopy = material->GetExpressionCollection().Expressions;
  for (UMaterialExpression* v : expressioncopy)
  {
    if (!v->HasConnectedOutputs())
      material->GetExpressionCollection().RemoveExpression(v);
  }
  expressioncopy.Empty();

  material->PreEditChange(NULL);
  material->PostEditChange();
  // make sure that any static meshes, etc using this material will stop using the FMaterialResource of the original
  // material, and will use the new FMaterialResource created when we make a new UMaterial in place
  FGlobalComponentReregisterContext RecreateComponents;
  return material;
}


void FWorldCreatorBridgeModule::ImportTextureFiles()
{
  syncDir = FPaths::GetPath(selectedPath);
  FXmlFile configFile(selectedPath);
  root = configFile.GetRootNode();
  auto texturingNode = root->FindChildNode(TEXT("Texturing"));


  TArray<FString> colormapPaths;
  TArray<FString> colormapPathsCopy;
  TArray<FString> colormapPaths_2;
  TArray<FString> TexturePaths;
  FFileManagerGeneric::Get().FindFiles(colormapPaths, syncDir.GetCharArray().GetData(), COLORMAP_FILEENDING);
  FFileManagerGeneric::Get().FindFiles(colormapPaths_2, syncDir.GetCharArray().GetData(), COLORMAP_FILEENDING_2);
  colormapPaths.Append(colormapPaths_2);
  colormapPathsCopy = colormapPaths;
  FString nameToRemove = bImportLayers ? TEXTUREMAP_NAME : COLORMAP_NAME;
  for (FString s : colormapPathsCopy)
  {
    if (s.Contains(nameToRemove))
    {
      colormapPaths.Remove(s);
    }
  }
  colormapPathsCopy.Empty();

  for (FString name : colormapPaths)
  {
    TexturePaths.Add(FString::Printf(TEXT("%s/%s"), syncDir.GetCharArray().GetData(), name.GetCharArray().GetData()));
  }

  if (texturingNode != nullptr && bImportLayers)
  {
    //Load From File
    auto childNodes = texturingNode->GetChildrenNodes();
    int textureCount = 0;

    int currentlayerindex = 0;
    for (auto child : childNodes)
    {
      auto textures = child->GetChildrenNodes();


      for (int i = 0; i < textures.Num(); i++)
      {

        auto textureNode = textures[i];

        // Import Textures 
        TArray<FString> TexturePathNames;
        auto albedoFile = textureNode->GetAttribute(TEXT("AlbedoFile"));
        auto normalFile = textureNode->GetAttribute(TEXT("NormalFile"));
        auto aoFile = textureNode->GetAttribute(TEXT("AoFile"));
        auto displacementFile = textureNode->GetAttribute(TEXT("DisplacementFile"));
        auto roughnessFile = textureNode->GetAttribute(TEXT("RoughnessFile"));

        // has to be done this way because FFileManagerGeneric::FileExists does not work



        // i think there is a bug here, the if condidion encloser is to short 
        // what are the names doing
        if (!albedoFile.IsEmpty())
        {
          auto albedoPath = FString::Printf(TEXT("%s/Assets/%s"), syncDir.GetCharArray().GetData(), albedoFile.GetCharArray().GetData());
          albedoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
          albedoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
          FSoftObjectPath tmpPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), albedoFile.GetCharArray().GetData()));
          if (tmpPath.TryLoad() != nullptr)
          {
            FFileManagerGeneric::Get().Delete(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), albedoFile.GetCharArray().GetData()).GetCharArray().GetData());
          }
          TexturePaths.Add(albedoPath);
          TexturePathNames.Add(albedoFile);
        }
        if (!normalFile.IsEmpty())
        {
          auto normalPath = FString::Printf(TEXT("%s/Assets/%s"), syncDir.GetCharArray().GetData(), normalFile.GetCharArray().GetData());
          normalFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
          normalFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
          FSoftObjectPath tmpPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), normalFile.GetCharArray().GetData()));
          if (tmpPath.TryLoad() != nullptr)
          {
            FFileManagerGeneric::Get().Delete(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), normalFile.GetCharArray().GetData()).GetCharArray().GetData());
          }
          TexturePaths.Add(normalPath);
          TexturePathNames.Add(normalFile);
        }
        if (!aoFile.IsEmpty())
        {
          auto aoPath = FString::Printf(TEXT("%s/Assets/%s"), syncDir.GetCharArray().GetData(), aoFile.GetCharArray().GetData());
          aoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
          aoFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
          FSoftObjectPath tmpPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), aoFile.GetCharArray().GetData()));
          if (tmpPath.TryLoad() != nullptr)
          {
            FFileManagerGeneric::Get().Delete(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), aoFile.GetCharArray().GetData()).GetCharArray().GetData());
          }
          TexturePaths.Add(aoPath);
          TexturePathNames.Add(aoFile);
        }
        if (!displacementFile.IsEmpty())
        {
          auto displacementPath = FString::Printf(TEXT("%s/Assets/%s"), syncDir.GetCharArray().GetData(), displacementFile.GetCharArray().GetData());
          displacementFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
          displacementFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
          FSoftObjectPath tmpPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), displacementFile.GetCharArray().GetData()));
          if (tmpPath.TryLoad() != nullptr)
          {
            FFileManagerGeneric::Get().Delete(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), displacementFile.GetCharArray().GetData()).GetCharArray().GetData());
          }
          TexturePaths.Add(displacementPath);
          TexturePathNames.Add(displacementFile);
        }
        if (!roughnessFile.IsEmpty())
        {
          auto roughnessPath = FString::Printf(TEXT("%s/Assets/%s"), syncDir.GetCharArray().GetData(), roughnessFile.GetCharArray().GetData());
          roughnessFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING));
          roughnessFile.RemoveFromEnd(FString::Printf(TEXT(".%s"), COLORMAP_FILEENDING_2));
          FSoftObjectPath tmpPath(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), roughnessFile.GetCharArray().GetData()));
          if (tmpPath.TryLoad() != nullptr)
          {
            FFileManagerGeneric::Get().Delete(FString::Printf(TEXT("%s%s"), MATERIAL_PACKAGE_NAME_PREFIX.GetCharArray().GetData(), roughnessFile.GetCharArray().GetData()).GetCharArray().GetData());
          }
          TexturePaths.Add(roughnessPath);
          TexturePathNames.Add(roughnessFile);
        }
      }
    }
  }

  UAutomatedAssetImportData* TextureImportData = NewObject<UAutomatedAssetImportData>();
  FAssetRegistryModule::AssetCreated(TextureImportData);
  TextureImportData->bReplaceExisting = true;
  TextureImportData->DestinationPath = MATERIAL_PACKAGE_NAME_PREFIX;
  TextureImportData->Filenames = TexturePaths;
  FAssetToolsModule& TextureAssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

  auto importedTextureFiles = TextureAssetToolsModule.Get().ImportAssetsAutomated(TextureImportData);
  for (UObject* obj : importedTextureFiles)
  {
    obj->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(obj);
  }

  configFile.Clear();
}

void FWorldCreatorBridgeModule::DeletePreviousImportedWorldCreatorLandscape(UWorld* world, FVector* location, FRotator* rotation)
{
  // declaer landscape and gizmo pointer
  ALandscape* landscapeActor = nullptr;
  ALandscapeGizmoActiveActor* landscapeGizmo = nullptr;
  ULevel* level = world->GetCurrentLevel();


  //// Find existing landscape
  ////////////////////////////
  auto actorIter = FActorIterator(world);
  bool actorfound = false;
  while (actorIter)
  {
    if (actorfound) {
      break;
    }
    if (actorIter->GetClass() == ALandscape::StaticClass() && actorIter->GetActorLabel().StartsWith(terrainName))
    {
      landscapeActor = (ALandscape*)*actorIter;
      actorfound = true;
    }
    ++actorIter;
  }
  //// Find existing gizmo
  ////////////////////////
  if (landscapeActor != nullptr)
  {
    actorIter = FActorIterator(world);
    while (actorIter)
    {
      if (actorIter->GetClass() == ALandscapeGizmoActiveActor::StaticClass())
      {
        auto gizmo = (ALandscapeGizmoActiveActor*)*actorIter;
        if (gizmo->TargetLandscapeInfo == landscapeActor->GetLandscapeInfo())
        {
          landscapeGizmo = gizmo;
          break;
        }
      }
      ++actorIter;
    }

    if (landscapeGizmo != nullptr)
    {
      landscapeGizmo->Destroy();
      landscapeGizmo = nullptr;
    }

    // set the previous location to spawn the new terrain at 
    *location = landscapeActor->GetActorLocation();
    *rotation = landscapeActor->GetActorRotation();

    if (level)
    {
      if (level->bIsPartitioned)
      {
        UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = world->GetWorldPartition()->CreateEditorLoaderAdapter<FLoaderAdapterShape>(world, REGIONBOX, TEXT("Loaded Region"));
        EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
        EditorLoaderAdapter->GetLoaderAdapter()->Load();

        SelectedLoaderInterfaces.Empty();
        SelectedLoaderInterfaces.Add(EditorLoaderAdapter);

        auto actorIterforProxy = FActorIterator(world);
        ALandscapeStreamingProxy* streamingProxy = nullptr;
        auto streamProxyClass = ALandscapeStreamingProxy::StaticClass();

        while (actorIterforProxy)
        {
          if (actorIterforProxy->GetClass() == streamProxyClass)
          {
            streamingProxy = (ALandscapeStreamingProxy*)*actorIterforProxy;
            if (streamingProxy->GetLandscapeActor()->GetActorLabel().StartsWith(terrainName))
            {
              streamingProxy->Destroy();
              streamingProxy = nullptr;
            }
          }
          ++actorIterforProxy;
        }

        EditorLoaderAdapter->GetLoaderAdapter()->Unload();
        SelectedLoaderInterfaces.Empty();
      }
    }

    landscapeActor->Destroy();

    //actorIter = FActorIterator(world);
    //while (actorIter)
    //{
    //  if (actorIter->GetClass() == ALandscape::StaticClass() && actorIter->GetActorLabel().StartsWith(terrainName))
    //  {
    //    landscapeActor = Cast<ALandscape>(*actorIter);
    //    landscapeActor->Destroy();
    //  }
    //  ++actorIter;
    //}

    GEditor->RedrawLevelEditingViewports();
  }
}

//ALandscapeProxy* FindOrAddLandscapeStreamingProxy(UActorPartitionSubsystem* InActorPartitionSubsystem, ULandscapeInfo* InLandscapeInfo, const UActorPartitionSubsystem::FCellCoord& InCellCoord)
//{
//  ALandscape* Landscape = InLandscapeInfo->LandscapeActor.Get();
//  check(Landscape);
//
//  auto LandscapeProxyCreated = [InCellCoord, Landscape](APartitionActor* PartitionActor)
//    {
//      const FIntPoint CellLocation(static_cast<int32>(InCellCoord.X) * Landscape->GetGridSize(), static_cast<int32>(InCellCoord.Y) * Landscape->GetGridSize());
//
//      ALandscapeProxy* LandscapeProxy = CastChecked<ALandscapeProxy>(PartitionActor);
//      // copy shared properties to this new proxy
//      LandscapeProxy->SynchronizeSharedProperties(Landscape);
//      const FVector ProxyLocation = Landscape->GetActorLocation() + FVector(CellLocation.X * Landscape->GetActorRelativeScale3D().X, CellLocation.Y * Landscape->GetActorRelativeScale3D().Y, 0.0f);
//
//      LandscapeProxy->CreateLandscapeInfo();
//      LandscapeProxy->SetActorLocationAndRotation(ProxyLocation, Landscape->GetActorRotation());
//      LandscapeProxy->LandscapeSectionOffset = FIntPoint(CellLocation.X, CellLocation.Y);
//      LandscapeProxy->SetIsSpatiallyLoaded(LandscapeProxy->GetLandscapeInfo()->AreNewLandscapeActorsSpatiallyLoaded());
//    };
//
//  const bool bCreate = true;
//  const bool bBoundsSearch = false;
//
//  ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActorPartitionSubsystem->GetActor(ALandscapeStreamingProxy::StaticClass(), InCellCoord, bCreate, InLandscapeInfo->LandscapeGuid, Landscape->GetGridSize(), bBoundsSearch, LandscapeProxyCreated));
//  check(!LandscapeProxy || LandscapeProxy->GetGridSize() == Landscape->GetGridSize());
//  return LandscapeProxy;
//
//}


//ALandscapeProxy* MoveComponentsToProxy(ULandscapeInfo* info, const TArray<ULandscapeComponent*>& InComponents, ALandscapeProxy* LandscapeProxy, bool bSetPositionAndOffset, ULevel* TargetLevel)
//{
//  TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeInfo::MoveComponentsToProxy);
//
//  ALandscape* Landscape = info->LandscapeActor.Get();
//  check(Landscape != nullptr);
//
//  struct FCompareULandscapeComponentBySectionBase
//  {
//    FORCEINLINE bool operator()(const ULandscapeComponent& A, const ULandscapeComponent& B) const
//    {
//      return (A.GetSectionBase().X == B.GetSectionBase().X) ? (A.GetSectionBase().Y < B.GetSectionBase().Y) : (A.GetSectionBase().X < B.GetSectionBase().X);
//    }
//  };
//  TArray<ULandscapeComponent*> ComponentsToMove(InComponents);
//  ComponentsToMove.Sort(FCompareULandscapeComponentBySectionBase());
//
//  const int32 ComponentSizeVerts = Landscape->NumSubsections * (Landscape->SubsectionSizeQuads + 1);
//  const int32 NeedHeightmapSize = 1 << FMath::CeilLogTwo(ComponentSizeVerts);
//
//  TSet<ALandscapeProxy*> SelectProxies;
//  TSet<ULandscapeComponent*> TargetSelectedComponents;
//  TArray<ULandscapeHeightfieldCollisionComponent*> TargetSelectedCollisionComponents;
//  for (ULandscapeComponent* Component : ComponentsToMove)
//  {
//    SelectProxies.Add(Component->GetLandscapeProxy());
//    if (Component->GetLandscapeProxy() != LandscapeProxy && (!TargetLevel || Component->GetLandscapeProxy()->GetOuter() != TargetLevel))
//    {
//      TargetSelectedComponents.Add(Component);
//    }
//
//    ULandscapeHeightfieldCollisionComponent* CollisionComp = Component->GetCollisionComponent();
//    SelectProxies.Add(CollisionComp->GetLandscapeProxy());
//    if (CollisionComp->GetLandscapeProxy() != LandscapeProxy && (!TargetLevel || CollisionComp->GetLandscapeProxy()->GetOuter() != TargetLevel))
//    {
//      TargetSelectedCollisionComponents.Add(CollisionComp);
//    }
//  }
//
//  // Check which heightmap will need to be renewed :
//  TSet<UTexture2D*> OldHeightmapTextures;
//  for (ULandscapeComponent* Component : TargetSelectedComponents)
//  {
//    Component->Modify();
//    OldHeightmapTextures.Add(Component->GetHeightmap());
//    // Also process all edit layers heightmaps :
//    Component->ForEachLayer([&](const FGuid& LayerGuid, FLandscapeLayerComponentData& LayerData)
//      {
//        OldHeightmapTextures.Add(Component->GetHeightmap(LayerGuid));
//      });
//  }
//
//  // Need to split all the component which share Heightmap with selected components
//  TMap<ULandscapeComponent*, bool> HeightmapUpdateComponents;
//  HeightmapUpdateComponents.Reserve(TargetSelectedComponents.Num() * 4); // worst case
//  for (ULandscapeComponent* Component : TargetSelectedComponents)
//  {
//    // Search neighbor only
//    const int32 needX = Component->GetHeightmap()->Source.GetSizeX();
//    const int32 needY = Component->GetHeightmap()->Source.GetSizeY();
//    const int32 SearchX = Component->GetHeightmap()->Source.GetSizeX() / NeedHeightmapSize - 1;
//    const int32 SearchY = Component->GetHeightmap()->Source.GetSizeY() / NeedHeightmapSize - 1;
//    const FIntPoint ComponentBase = Component->GetSectionBase() / Component->ComponentSizeQuads;
//    UE_LOG(LogTemp, Log, TEXT("%d, %d, %d, %d"), needX, needY, SearchX, SearchY);
//
//    for (int32 Y = -SearchY; Y <= SearchY; ++Y)
//    {
//      for (int32 X = -SearchX; X <= SearchX; ++X)
//      {
//        ULandscapeComponent* const Neighbor = info->XYtoComponentMap.FindRef(ComponentBase + FIntPoint(X, Y));
//        if (Neighbor && Neighbor->GetHeightmap() == Component->GetHeightmap() && !HeightmapUpdateComponents.Contains(Neighbor))
//        {
//          Neighbor->Modify();
//          bool bNeedsMoveToCurrentLevel = TargetSelectedComponents.Contains(Neighbor);
//          HeightmapUpdateComponents.Add(Neighbor, bNeedsMoveToCurrentLevel);
//        }
//      }
//    }
//  }
//
//  // Proxy position/offset needs to be set
//  if (bSetPositionAndOffset)
//  {
//    // set proxy location
//    // by default first component location
//    ULandscapeComponent* FirstComponent = *TargetSelectedComponents.CreateConstIterator();
//    LandscapeProxy->GetRootComponent()->SetWorldLocationAndRotation(FirstComponent->GetComponentLocation(), FirstComponent->GetComponentRotation());
//    LandscapeProxy->LandscapeSectionOffset = FirstComponent->GetSectionBase();
//  }
//
//  // Hide(unregister) the new landscape if owning level currently in hidden state
//  if (LandscapeProxy->GetLevel()->bIsVisible == false)
//  {
//    LandscapeProxy->UnregisterAllComponents();
//  }
//
//  // Changing Heightmap format for selected components
//
//  for (const auto& HeightmapUpdateComponentPair : HeightmapUpdateComponents)
//  {
//    ALandscape::SplitHeightmap(HeightmapUpdateComponentPair.Key, HeightmapUpdateComponentPair.Value ? LandscapeProxy : nullptr);
//  }
//
//  // Delete if textures are not referenced anymore...
//  for (UTexture2D* Texture : OldHeightmapTextures)
//  {
//    check(Texture != nullptr);
//    Texture->SetFlags(RF_Transactional);
//    Texture->Modify();
//    Texture->MarkPackageDirty();
//    Texture->ClearFlags(RF_Standalone);
//  }
//
//  for (ALandscapeProxy* Proxy : SelectProxies)
//  {
//    Proxy->Modify();
//  }
//
//  LandscapeProxy->Modify();
//  LandscapeProxy->MarkPackageDirty();
//
//  // Handle XY-offset textures (these don't need splitting, as they aren't currently shared between components like heightmaps/weightmaps can be)
//  for (ULandscapeComponent* Component : TargetSelectedComponents)
//  {
//    if (Component->XYOffsetmapTexture)
//    {
//      Component->XYOffsetmapTexture->Modify();
//      Component->XYOffsetmapTexture->Rename(nullptr, LandscapeProxy);
//    }
//  }
//
//  // Change Weight maps...
//  {
//    FLandscapeEditDataInterface LandscapeEdit(info);
//    for (ULandscapeComponent* Component : TargetSelectedComponents)
//    {
//      Component->ReallocateWeightmaps(&LandscapeEdit, false, true, true, LandscapeProxy);
//      Component->ForEachLayer([&](const FGuid& LayerGuid, FLandscapeLayerComponentData& LayerData)
//        {
//          FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid);
//          Component->ReallocateWeightmaps(&LandscapeEdit, true, true, true, LandscapeProxy);
//        });
//      Landscape->RequestLayersContentUpdateForceAll();
//    }
//
//    // Need to re-pack all the Weight map (to have it optimally re-packed...)
//    for (ALandscapeProxy* Proxy : SelectProxies)
//    {
//      Proxy->RemoveInvalidWeightmaps();
//    }
//  }
//
//  // Move the components to the Proxy actor
//  // This does not use the MoveSelectedActorsToCurrentLevel path as there is no support to only move certain components.
//  for (ULandscapeComponent* Component : TargetSelectedComponents)
//  {
//    // Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
//    Component->GetLandscapeProxy()->LandscapeComponents.Remove(Component);
//    Component->UnregisterComponent();
//    Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
//    Component->InvalidateLightingCache();
//    Component->Rename(nullptr, LandscapeProxy);
//    LandscapeProxy->LandscapeComponents.Add(Component);
//    Component->AttachToComponent(LandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
//
//    // clear transient mobile data
//    Component->MobileDataSourceHash.Invalidate();
//    Component->MobileMaterialInterfaces.Reset();
//    Component->MobileWeightmapTextures.Reset();
//
//    Component->UpdateMaterialInstances();
//  }
//  LandscapeProxy->UpdateCachedHasLayersContent();
//
//  for (ULandscapeHeightfieldCollisionComponent* Component : TargetSelectedCollisionComponents)
//  {
//    // Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
//
//    Component->GetLandscapeProxy()->CollisionComponents.Remove(Component);
//    Component->UnregisterComponent();
//    Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
//    Component->Rename(nullptr, LandscapeProxy);
//    LandscapeProxy->CollisionComponents.Add(Component);
//    Component->AttachToComponent(LandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
//
//    // Move any foliage associated
//    // AInstancedFoliageActor::MoveInstancesForComponentToLevel(Component, LandscapeProxy->GetLevel());
//  }
//
//  // Register our new components if destination landscape is registered in scene 
//  if (LandscapeProxy->GetRootComponent()->IsRegistered())
//  {
//    LandscapeProxy->RegisterAllComponents();
//  }
//
//  for (ALandscapeProxy* Proxy : SelectProxies)
//  {
//    if (Proxy->GetRootComponent()->IsRegistered())
//    {
//      Proxy->RegisterAllComponents();
//    }
//  }
//
//  return LandscapeProxy;
//}

//bool ChangeGridSize(ULandscapeInfo* InLandscapeInfo, uint32 InNewGridSizeInComponents, TSet<AActor*>& OutActorsToDelete)
//{
//  check(InLandscapeInfo);
//
//  const uint32 GridSize = InLandscapeInfo->GetGridSize(InNewGridSizeInComponents);
//
//  InLandscapeInfo->LandscapeActor->Modify();
//  InLandscapeInfo->LandscapeActor->SetGridSize(GridSize);
//
//  // This needs to be done before moving components
//  InLandscapeInfo->LandscapeActor->InitializeLandscapeLayersWeightmapUsage();
//
//  // Make sure if actor didn't include grid size in name it now does. This will avoid recycling 
//  // LandscapeStreamingProxy actors and create new ones with the proper name.
//  InLandscapeInfo->LandscapeActor->bIncludeGridSizeInNameForLandscapeActors = true;
//
//  FIntRect Extent;
//  InLandscapeInfo->GetLandscapeExtent(Extent.Min.X, Extent.Min.Y, Extent.Max.X, Extent.Max.Y);
//  const FBox Bounds(FVector(Extent.Min), FVector(Extent.Max));
//
//  UWorld* World = InLandscapeInfo->LandscapeActor->GetWorld();
//  UActorPartitionSubsystem* ActorPartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
//
//  TArray<ULandscapeComponent*> LandscapeComponents;
//  LandscapeComponents.Reserve(InLandscapeInfo->XYtoComponentMap.Num());
//  InLandscapeInfo->ForAllLandscapeComponents([&LandscapeComponents](ULandscapeComponent* LandscapeComponent)
//    {
//      LandscapeComponents.Add(LandscapeComponent);
//    });
//
//  TSet<ALandscapeProxy*> ProxiesToDelete;
//  int testcount = 0;
//
//  FActorPartitionGridHelper::ForEachIntersectingCell(ALandscapeStreamingProxy::StaticClass(), Extent, World->PersistentLevel, [ActorPartitionSubsystem, InLandscapeInfo, InNewGridSizeInComponents, &LandscapeComponents, &ProxiesToDelete, &testcount](const UActorPartitionSubsystem::FCellCoord& CellCoord, const FIntRect& CellBounds)
//    {
//      TMap<ULandscapeComponent*, UMaterialInterface*> ComponentMaterials;
//      TMap<ULandscapeComponent*, UMaterialInterface*> ComponentHoleMaterials;
//      TMap <ULandscapeComponent*, TMap<int32, UMaterialInterface*>> ComponentLODMaterials;
//
//      TArray<ULandscapeComponent*> ComponentsToMove;
//      const int32 MaxComponents = (int32)(InNewGridSizeInComponents * InNewGridSizeInComponents);
//      ComponentsToMove.Reserve(MaxComponents);
//      for (int32 i = 0; i < LandscapeComponents.Num();)
//      {
//        ULandscapeComponent* LandscapeComponent = LandscapeComponents[i];
//        if (CellBounds.Contains(LandscapeComponent->GetSectionBase()))
//        {
//          ComponentMaterials.FindOrAdd(LandscapeComponent, LandscapeComponent->GetLandscapeMaterial());
//          ComponentHoleMaterials.FindOrAdd(LandscapeComponent, LandscapeComponent->GetLandscapeHoleMaterial());
//          TMap<int32, UMaterialInterface*>& LODMaterials = ComponentLODMaterials.FindOrAdd(LandscapeComponent);
//          for (int8 LODIndex = 0; LODIndex <= 8; ++LODIndex)
//          {
//            LODMaterials.Add(LODIndex, LandscapeComponent->GetLandscapeMaterial(LODIndex));
//          }
//
//          ComponentsToMove.Add(LandscapeComponent);
//          LandscapeComponents.RemoveAtSwap(i);
//          ProxiesToDelete.Add(LandscapeComponent->GetTypedOuter<ALandscapeProxy>());
//        }
//        else
//        {
//          i++;
//        }
//      }
//      FEdModeLandscape* LandscapeMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
//      check(ComponentsToMove.Num() <= MaxComponents);
//      if (ComponentsToMove.Num())
//      {
//        ALandscapeProxy* LandscapeProxy = FindOrAddLandscapeStreamingProxy(ActorPartitionSubsystem, InLandscapeInfo, CellCoord);
//        check(LandscapeProxy);
//        MoveComponentsToProxy(InLandscapeInfo, ComponentsToMove, LandscapeProxy, false, nullptr);
//        UE_LOG(LogTemp, Log, TEXT("---------------------------------------------------------------- %d"), testcount);
//        testcount++;
//
//        // Make sure components retain their Materials if they don't match with their parent proxy
//        for (ULandscapeComponent* MovedComponent : ComponentsToMove)
//        {
//          UMaterialInterface* PreviousLandscapeMaterial = ComponentMaterials.FindChecked(MovedComponent);
//          UMaterialInterface* PreviousLandscapeHoleMaterial = ComponentHoleMaterials.FindChecked(MovedComponent);
//          TMap<int32, UMaterialInterface*> PreviousLandscapeLODMaterials = ComponentLODMaterials.FindChecked(MovedComponent);
//
//          MovedComponent->OverrideMaterial = nullptr;
//          if (PreviousLandscapeMaterial != nullptr && PreviousLandscapeMaterial != MovedComponent->GetLandscapeMaterial())
//          {
//            // If Proxy doesn't differ from Landscape override material there first
//            if (LandscapeProxy->GetLandscapeMaterial() == LandscapeProxy->GetLandscapeActor()->GetLandscapeMaterial())
//            {
//              LandscapeProxy->LandscapeMaterial = PreviousLandscapeMaterial;
//            }
//            else // If it already differs it means that the component differs from it, override on component
//            {
//              MovedComponent->OverrideMaterial = PreviousLandscapeMaterial;
//            }
//          }
//
//          MovedComponent->OverrideHoleMaterial = nullptr;
//          if (PreviousLandscapeHoleMaterial != nullptr && PreviousLandscapeHoleMaterial != MovedComponent->GetLandscapeHoleMaterial())
//          {
//            // If Proxy doesn't differ from Landscape override material there first
//            if (LandscapeProxy->GetLandscapeHoleMaterial() == LandscapeProxy->GetLandscapeActor()->GetLandscapeHoleMaterial())
//            {
//              LandscapeProxy->LandscapeHoleMaterial = PreviousLandscapeHoleMaterial;
//            }
//            else // If it already differs it means that the component differs from it, override on component
//            {
//              MovedComponent->OverrideHoleMaterial = PreviousLandscapeHoleMaterial;
//            }
//          }
//
//          TArray<FLandscapePerLODMaterialOverride> PerLODOverrideMaterialsForComponent;
//          TArray<FLandscapePerLODMaterialOverride> PerLODOverrideMaterialsForProxy = LandscapeProxy->GetPerLODOverrideMaterials();
//          for (int8 LODIndex = 0; LODIndex <= 8; ++LODIndex)
//          {
//            UMaterialInterface* PreviousLODMaterial = PreviousLandscapeLODMaterials.FindChecked(LODIndex);
//            // If Proxy doesn't differ from Landscape override material there first
//            if (PreviousLODMaterial != nullptr && PreviousLODMaterial != MovedComponent->GetLandscapeMaterial(LODIndex))
//            {
//              if (LandscapeProxy->GetLandscapeMaterial(LODIndex) == LandscapeProxy->GetLandscapeActor()->GetLandscapeMaterial(LODIndex))
//              {
//                PerLODOverrideMaterialsForProxy.Add({ LODIndex, TObjectPtr<UMaterialInterface>(PreviousLODMaterial) });
//              }
//              else // If it already differs it means that the component differs from it, override on component
//              {
//                PerLODOverrideMaterialsForComponent.Add({ LODIndex, TObjectPtr<UMaterialInterface>(PreviousLODMaterial) });
//              }
//            }
//          }
//          MovedComponent->SetPerLODOverrideMaterials(PerLODOverrideMaterialsForComponent);
//          LandscapeProxy->SetPerLODOverrideMaterials(PerLODOverrideMaterialsForProxy);
//        }
//      }
//
//      return true;
//    }, GridSize);
//
//  // Only delete Proxies that where not reused
//  for (ALandscapeProxy* ProxyToDelete : ProxiesToDelete)
//  {
//    if (ProxyToDelete->LandscapeComponents.Num() > 0 || ProxyToDelete->IsA<ALandscape>())
//    {
//      check(ProxyToDelete->GetGridSize() == GridSize);
//      continue;
//    }
//
//    OutActorsToDelete.Add(ProxyToDelete);
//  }
//
//  if (InLandscapeInfo->CanHaveLayersContent())
//  {
//    InLandscapeInfo->ForceLayersFullUpdate();
//  }
//
//  return true;
//}

//TArray<uint16> ReadRawHeightmap(const FString& FilePath, int& OutWidth, int& OutHeight)
//{
//  TArray<uint16> HeightData;
//
//  IFileManager& FileManager = IFileManager::Get();
//  if (FileManager.FileExists(*FilePath))
//  {
//    TArray<uint8> RawData;
//    if (FFileHelper::LoadFileToArray(RawData, L"C:/Users/Chris/Documents/World Creator/Sync/heightmap_0_0.raw"))
//    {
//      OutWidth = OutHeight = FMath::Sqrt((RawData.Num() / (float)sizeof(uint16)));
//      HeightData.SetNum(OutWidth * OutHeight);
//      FMemory::Memcpy(HeightData.GetData(), RawData.GetData(), RawData.Num());
//    }
//  }
//
//  return HeightData;
//}

void FWorldCreatorBridgeModule::ImportHeightMapToLandscape(UWorld* world, TSharedPtr<LandscapeImportData> data, int _width, int _length, int id = 0, FVector location = FVector(0, 0, 0), FRotator rotation = FRotator(0, 0, 0))
{
  // declaer landscape and gizmo pointer
  ALandscape* landscapeActor = nullptr;
  ALandscapeGizmoActiveActor* landscapeGizmo = nullptr;
  int _inNumSections = 1;

  const UPROPERTY() FGuid landscapeGuid = FGuid::FGuid();
  UPROPERTY() TMap<FGuid, TArray<uint16>> heightDataMap;
  UPROPERTY() TMap<FGuid, TArray<FLandscapeImportLayerInfo>> layerInfosMap;
  heightDataMap.Add(landscapeGuid, data->heightData);
  layerInfosMap.Add(landscapeGuid, data->layerInfos);

  landscapeActor = world->SpawnActor<ALandscape>(location, rotation);
  if (data->material != nullptr)
    landscapeActor->LandscapeMaterial = (UMaterialInterface*)data->material;
  landscapeActor->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((_width * _length) / (2048 * 2048) + 1), (uint32)2);
  // landscapeActor->SetLandscapeGuid(FGuid::NewGuid());

  int componentCountX = floor(_width / quatsPerSection * _inNumSections);
  int componentCountY = floor(_length / quatsPerSection * _inNumSections);
  const bool bIsWorldPartition = world->GetSubsystem<ULandscapeSubsystem>()->IsGridBased();
  const bool bLandscapeLargerThanRegion = worldPartitionRegionSize < componentCountX || worldPartitionRegionSize < componentCountY;
  const bool bNeedsLandscapeRegions = bIsWorldPartition && bLandscapeLargerThanRegion;

  UE_LOG(LogTemp, Log, TEXT("%d"), _inNumSections);

  int a = 0;
  int b = 0;
  //TArray<uint16>heightmap = ReadRawHeightmap(L"", a, b);
  TArray<FLandscapeLayer> landscapeLayers;
  landscapeLayers.Init(FLandscapeLayer(), 1);
  landscapeActor->Import(FGuid::NewGuid(), 0, 0, _width - 1, _length - 1, _inNumSections, data->quatsPerSection,
    heightDataMap, L"", layerInfosMap, ELandscapeImportAlphamapType::Additive);
  
  
  UPROPERTY() ULandscapeInfo* info = landscapeActor->GetLandscapeInfo();
  info->UpdateLayerInfoMap(landscapeActor);
  landscapeActor->SetActorScale3D(FVector(data->scaleX, data->scaleY, data->terrainScale));
  // landscapeActor->RegisterAllComponents();
  landscapeActor->SetActorRotation(rotation);
  landscapeActor->SetActorLocation(location);
  UE_LOG(LogTemp, Log, TEXT("%s"), *terrainName);
  landscapeActor->SetActorLabel(terrainName + FString::Printf(TEXT("_%d"), id).GetCharArray().GetData(), true);
  // landscapeGizmo = world->SpawnActor<ALandscapeGizmoActiveActor>(location, rotation);
  // landscapeGizmo->SetTargetLandscape(info);

  if (bUseWorldPartition)
  {
    ULandscapeSubsystem* landscapeSubSystem = world->GetSubsystem<ULandscapeSubsystem>();
    landscapeSubSystem->ChangeGridSize(info, (int32)worldPartitionGridSize);


    // TSet<AActor*> ActorsToDelete;
    // ChangeGridSize(info, worldPartitionGridSize, ActorsToDelete);
    if (bNeedsLandscapeRegions)
    {
      ALandscapeProxy* landscapeProxy = info->GetLandscapeProxy();
      ULevel* level = landscapeProxy->GetLevel();
      UPackage* levelPackage = level->GetPackage();

      // copied from source code 
      TArray<FIntPoint> NewComponents;
      NewComponents.Empty(componentCountX * componentCountY);
      for (int32 Y = 0; Y < componentCountY; Y++)
      {
        for (int32 X = 0; X < componentCountX; X++)
        {
          NewComponents.Add(FIntPoint(X, Y));
        }
      }

      int32 NumRegions = FMath::DivideAndRoundUp(componentCountX, static_cast<int32>(worldPartitionRegionSize)) * FMath::DivideAndRoundUp(componentCountY, static_cast<int32>(worldPartitionRegionSize));


      FScopedSlowTask Progress(static_cast<float>(NumRegions), LOCTEXT("CreateLandscapeRegions", "Creating Landscape Editor Regions..."));
      Progress.MakeDialog();

      //TArray<ALocationVolume*> RegionVolumes;
      //FBox LandscapeBounds;
      //UWorldPartition* WorldPartition = world->GetWorldPartition();
      //ULandscapeSubsystem* LandscapeSubsystem = world->GetSubsystem<ULandscapeSubsystem>();

    }
  }
  for (int li = 0; li < data->layerInfos.Num(); li++)
  {
    landscapeActor->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(data->layerInfos[li].LayerInfo));
  }
}



//void FWorldCreatorBridgeModule::AddComponents(ULandscapeInfo* InLandscapeInfo, ULandscapeSubsystem* InLandscapeSubsystem, const TArray<FIntPoint>& InComponentCoordinates, TArray<ALandscapeProxy*>& OutCreatedStreamingProxies)
//{
//  TRACE_CPUPROFILER_EVENT_SCOPE(AddComponents);
//  TArray<ULandscapeComponent*> NewComponents;
//  InLandscapeInfo->Modify();
//  for (const FIntPoint& ComponentCoordinate : InComponentCoordinates)
//  {
//    ULandscapeComponent* LandscapeComponent = InLandscapeInfo->XYtoComponentMap.FindRef(ComponentCoordinate);
//    if (LandscapeComponent)
//    {
//      continue;
//    }
//
//    // Add New component...
//    FIntPoint ComponentBase = ComponentCoordinate * InLandscapeInfo->ComponentSizeQuads;
//
//    ALandscapeProxy* LandscapeProxy = InLandscapeSubsystem->FindOrAddLandscapeProxy(InLandscapeInfo, ComponentBase);
//    if (!LandscapeProxy)
//    {
//      continue;
//    }
//
//    OutCreatedStreamingProxies.Add(LandscapeProxy);
//
//    LandscapeComponent = NewObject<ULandscapeComponent>(LandscapeProxy, NAME_None, RF_Transactional);
//    NewComponents.Add(LandscapeComponent);
//    LandscapeComponent->Init(
//      ComponentBase.X, ComponentBase.Y,
//      LandscapeProxy->ComponentSizeQuads,
//      LandscapeProxy->NumSubsections,
//      LandscapeProxy->SubsectionSizeQuads
//    );
//
//    TArray<FColor> HeightData;
//    const int32 ComponentVerts = (LandscapeComponent->SubsectionSizeQuads + 1) * LandscapeComponent->NumSubsections;
//    const FColor PackedMidpoint = LandscapeDataAccess::PackHeight(LandscapeDataAccess::GetTexHeight(0.0f));
//    HeightData.Init(PackedMidpoint, FMath::Square(ComponentVerts));
//
//    LandscapeComponent->InitHeightmapData(HeightData, true);
//    LandscapeComponent->UpdateMaterialInstances();
//
//    InLandscapeInfo->XYtoComponentMap.Add(ComponentCoordinate, LandscapeComponent);
//    InLandscapeInfo->XYtoAddCollisionMap.Remove(ComponentCoordinate);
//  }
//
//  // Need to register to use general height/xyoffset data update
//  for (int32 Idx = 0; Idx < NewComponents.Num(); Idx++)
//  {
//    NewComponents[Idx]->RegisterComponent();
//  }
//
//  const bool bHasXYOffset = false;
//  ALandscape* Landscape = InLandscapeInfo->LandscapeActor.Get();
//
//  bool bHasLandscapeLayersContent = Landscape && Landscape->HasLayersContent();
//
//  for (ULandscapeComponent* NewComponent : NewComponents)
//  {
//    if (bHasLandscapeLayersContent)
//    {
//      TArray<ULandscapeComponent*> ComponentsUsingHeightmap;
//      ComponentsUsingHeightmap.Add(NewComponent);
//
//      for (const FLandscapeLayer& Layer : Landscape->LandscapeLayers)
//      {
//        // Since we do not share heightmap when adding new component, we will provided the required array, but they will only be used for 1 component
//        TMap<UTexture2D*, UTexture2D*> CreatedHeightmapTextures;
//        NewComponent->AddDefaultLayerData(Layer.Guid, ComponentsUsingHeightmap, CreatedHeightmapTextures);
//      }
//    }
//
//    // Update Collision
//    NewComponent->UpdateCachedBounds();
//    NewComponent->UpdateBounds();
//    NewComponent->MarkRenderStateDirty();
//
//    if (!bHasLandscapeLayersContent)
//    {
//      ULandscapeHeightfieldCollisionComponent* CollisionComp = NewComponent->GetCollisionComponent();
//      if (CollisionComp && !bHasXYOffset)
//      {
//        CollisionComp->MarkRenderStateDirty();
//        CollisionComp->RecreateCollision();
//      }
//    }
//  }
//
//
//  if (Landscape)
//  {
//    GEngine->BroadcastOnActorMoved(Landscape);
//  }
//}

int FWorldCreatorBridgeModule::RecaulculateToUnrealSize(int quadsPerSection, int size)
{
  int numberOfComponents = size / quadsPerSection;
  return (size - 1) % quadsPerSection == 0 ? size : numberOfComponents * quadsPerSection + 1;
}

bool FWorldCreatorBridgeModule::SetupXmlVariables()
{
  syncDir = FPaths::GetPath(selectedPath);
  FXmlFile configFile(selectedPath);
  root = configFile.GetRootNode();
  if (root == NULL)
    return false;
  FXmlNode* node = root->FindChildNode(TEXT("Surface"));
  if (node == nullptr)
  {
    configFile.Clear();
    return false;
  }
  // assign values 
  // commented out values are currently not in use 

  minHeight = XmlHelper::GetFloat(node, "MinHeight");
  maxHeight = XmlHelper::GetFloat(node, "MaxHeight");
  width = XmlHelper::GetInt(node, "Width");
  length = XmlHelper::GetInt(node, "Length");
  resX = XmlHelper::GetInt(node, "ResolutionX");
  resY = XmlHelper::GetInt(node, "ResolutionY");
  rescaleFactor = 1; // (resX / width + resY / length) / 2;
  scaleX = (float)width / resX;
  scaleY = (float)length / resY;
  width = (resX);
  length = (resY);
  numTilesX = XmlHelper::GetInt(node, "TilesX");
  numTilesY = XmlHelper::GetInt(node, "TilesY");
  tileResolution = XmlHelper::GetInt(node, "TileResolution");

  float tmpVersion = XmlHelper::GetFloat(root, "Version");
  if (VERSIONMAP.Contains(tmpVersion))
  {
    version = VERSIONMAP[tmpVersion];
  }
  else
  {
    version = 3;
  }


  if (version == 1 && XmlHelper::GetInt(root->FindChildNode(TEXT("Surface")), "HeightCenter") < 0)
  {
    version = 3;
  }

  if (version == 2)
  {

    FXmlNode* tmpNode = root->FindChildNode(TEXT("Texturing"))->GetChildrenNodes()[0];
    FString fileName = XmlHelper::GetString(tmpNode, "Name");
    FString filePath = FString::Printf(TEXT("%s/%s"), syncDir.GetCharArray().GetData(), fileName.GetCharArray().GetData());

    TArray<uint8> fileData;

    FFileHelper::LoadFileToArray(fileData, filePath.GetCharArray().GetData());

    uint8* dataPtr = (fileData).GetData();
    int tmpWidth = *(short*)(&dataPtr[12]);
    int tmpLength = *(short*)(&dataPtr[14]);
    rescaleFactor = (tmpWidth / width + tmpLength / length) / 2;
    width = tmpWidth;
    length = tmpLength;
  }
  if (version == 1)
  {
    rescaleFactor = 1;
  }

  unrealNumTilesX = 1 + (resX / (unrealTerrainResolution + UNREAL_MIN_TILE_RESOLUTION));
  unrealNumTilesY = 1 + (resY / (unrealTerrainResolution + UNREAL_MIN_TILE_RESOLUTION));
  configFile.Clear();
  return true;
}

void FWorldCreatorBridgeModule::UpdateTerrainResolution(int terrainsize)
{
  if (terrainsize <= 0)
  {
    terrainsize = unrealTerrainResolution;
  }
  unrealTerrainResolution = RecaulculateToUnrealSize(quatsPerSection, terrainsize);
}

TOptional<float> FWorldCreatorBridgeModule::GetTransformDelta() const
{
  return worldScale;
}

TOptional<int> FWorldCreatorBridgeModule::GetGridSizeDelta() const
{
  return worldPartitionGridSize;
}
TOptional<int> FWorldCreatorBridgeModule::GetRegionSizeDelta() const
{
  return worldPartitionRegionSize;
}
TOptional<int> FWorldCreatorBridgeModule::GetCutSizeDelta() const
{
  return unrealTerrainResolution;
}
TOptional<int> FWorldCreatorBridgeModule::GetQuatPSSizeDelta() const
{
  return quatsPerSection;
}

TOptional<FString> FWorldCreatorBridgeModule::GetSelectedPath() const
{
  return selectedPath;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWorldCreatorBridgeModule, WorldCreatorBridge)
