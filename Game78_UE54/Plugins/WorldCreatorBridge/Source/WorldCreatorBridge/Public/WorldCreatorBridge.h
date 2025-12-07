// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "UObject/WeakInterfacePtr.h"
#include "LandscapeStreamingProxy.h"
#include "XmlHelper.h"
#include "LandscapeSubsystem.h"
#include "Templates/SharedPointer.h"


class FToolBarBuilder;
class FMenuBuilder;
#define WORLDPARTITION_MAX UE_OLD_WORLD_MAX // TODO this one changed due to the large world upgrade in unreal 5 so lets see how to fit it 

struct LandscapeImportData
{
  int quatsPerSection;
  float scaleX, scaleY, terrainScale;
  UMaterial* material;
  TArray<FLandscapeImportLayerInfo> layerInfos;
  TArray<uint16> heightData;
};

struct WCLandscapeTile
{
  int width;
  int height;
  int Bpp;
  TArray<uint8> heightmap;
  TArray<TArray<uint8>> splatmaps;
};

class FWorldCreatorBridgeModule : public IModuleInterface
{
public:

  /** IModuleInterface implementation */
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

  /** This function will be bound to Command. */
  void PluginButtonClicked();
  void UpdateTerrainResolution(int terrainsize = -1);

private:

  void RegisterMenus();

  TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);


private:


  const TCHAR HEIGHTMAP_FILEENDING[4] = TEXT("raw");
  const TCHAR COLORMAP_FILEENDING[4] = TEXT("png");
  const TCHAR COLORMAP_FILEENDING_2[4] = TEXT("jpg");
  const TCHAR SPLATMAP_FILEENDING[4] = TEXT("tga");
  static const int UNREAL_TERRAIN_RESOLUTION_XY = 8129; // 4033;

  const TMap<float, int> VERSIONMAP =
  {
      { 1.5f, 3},
      { 1.6f, 3},
      { 4.0f, 3},
      { 3.0f, 3}, // this is a problem
      {2.0f, 1}
  };

  struct FKeyFuncs : public BaseKeyFuncs<TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>, TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>, false>
  {
    static KeyInitType GetSetKey(ElementInitType Entry)
    {
      return Entry;
    }

    static bool Matches(KeyInitType A, KeyInitType B)
    {
      return A == B;
    }

    static uint32 GetKeyHash(KeyInitType Key)
    {
      return GetTypeHash(Key.GetWeakObjectPtr());
    }
  };

  using FLoaderInterface = TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>;
  using FLoaderInterfaceSet = TSet<FLoaderInterface, FKeyFuncs>;


  TSharedPtr<class FUICommandList> PluginCommands;
  FLoaderInterfaceSet SelectedLoaderInterfaces;
  static TSharedRef<SWidget> GetSectionSizeMenu(FWorldCreatorBridgeModule* bridge);

  // UI Elements 
  FButtonStyle* wcButton;
  FButtonStyle* youtubeButton;
  FButtonStyle* twitterButton;
  FButtonStyle* twitchButton;
  FButtonStyle* facebookButton;
  FButtonStyle* discordButton;
  FButtonStyle* artstationButton;
  FButtonStyle* instagramButton;
  FButtonStyle* vimeoButton;

  FString selectedPath;
  FString terrainName;
  FString terrainMaterialName;
  FString syncDir;

  int version;

  // syncVariables
  FXmlNode* root;
  float minHeight;
  float maxHeight;
  int width;
  int length;
  int rescaleFactor;
  int resX;
  int resY;
  float scaleX;
  float scaleY;
  int numTilesX;
  int numTilesY;
  int tileResolution;

  int unrealTerrainResolution;
  int quatsPerSection;
  int unrealNumTilesX = resX / UNREAL_TERRAIN_RESOLUTION_XY + 1;
  int unrealNumTilesY = resY / UNREAL_TERRAIN_RESOLUTION_XY + 1;
  bool bImportTextures;
  bool bImportLayers;  
  bool bUseWorldPartition;
  bool bBuildMinimap;
  float worldScale;
  int worldPartitionGridSize;
  int worldPartitionRegionSize;
  TSharedPtr<SEditableTextBox> selectedPathBox;

private:

  // self written functinos 
  void SetupUIElements();

  // Menu button functions
  FReply SyncButtonClicked();
  FReply BuildMinimapButtonClicked();
  FReply BrowseButtonClicked();

  void ImportTextureFiles();
  UMaterial* CreateLandscapeMaterial(int terrainId, int _numTilesX, int _numTilesY, int startX, int startY, int mappingWidth, int mappingLength);
  void DeletePreviousImportedWorldCreatorLandscape(UWorld* world, FVector* location, FRotator* rotation);
  void ImportHeightMapToLandscape(UWorld* world, TSharedPtr<LandscapeImportData> data, int width, int length, int id, FVector location, FRotator rotation);
  int RecaulculateToUnrealSize(int quadsPerSection, int size);
  bool SetupXmlVariables();
  void AddComponents(ULandscapeInfo* InLandscapeInfo, ULandscapeSubsystem* InLandscapeSubsystem, const TArray<FIntPoint>& InComponentCoordinates, TArray<ALandscapeProxy*>& OutCreatedStreamingProxies);
  bool CreateLandscape(int componentCountX, int componentCountY, int quadsPerSection, FVector location, FVector scale, FRotator rotation);
  WCLandscapeTile* TileToData(FString pathEnding, TArray<FXmlNode*> texturNodes);

  TOptional<float> GetTransformDelta() const;
  TOptional<int> GetGridSizeDelta() const;
  TOptional<int> GetRegionSizeDelta() const;
  TOptional<int> GetCutSizeDelta() const;
  TOptional<int> GetQuatPSSizeDelta() const;
  TOptional<FString> GetSelectedPath() const;

  //TSharedRef<SWidget> GetSectionSizeMenu();
  //void OnChangeSectionSize(int32 NewSize);
};
