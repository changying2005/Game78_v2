// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldCreatorBridgeStyle.h"
#include "WorldCreatorBridge.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FWorldCreatorBridgeStyle::StyleInstance = nullptr;

void FWorldCreatorBridgeStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FWorldCreatorBridgeStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FWorldCreatorBridgeStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WorldCreatorBridgeStyle"));
	return StyleSetName;
}

#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const FVector2D Icon40x40(40.0f, 40.0f);

const FVector2D WorldCreator(380.0f, 72.0f);
const FVector2D LinkIcon(60.0f, 60.0f);

TSharedRef< FSlateStyleSet > FWorldCreatorBridgeStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("WorldCreatorBridgeStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("WorldCreatorBridge")->GetBaseDir() / TEXT("Resources"));

	Style->Set("WorldCreatorBridge.PluginAction", new IMAGE_BRUSH(TEXT("icon_wc"), Icon40x40));

	Style->Set("WorldCreatorBridge.WorldCreator", new IMAGE_BRUSH(TEXT("banner_wc"), WorldCreator));
	Style->Set("WorldCreatorBridge.Twitter", new IMAGE_BRUSH(TEXT("icon_twitter"), LinkIcon));
	Style->Set("WorldCreatorBridge.Youtube", new IMAGE_BRUSH(TEXT("icon_youtube"), LinkIcon));
	Style->Set("WorldCreatorBridge.Instagram", new IMAGE_BRUSH(TEXT("icon_instagram"), LinkIcon));
	Style->Set("WorldCreatorBridge.Facebook", new IMAGE_BRUSH(TEXT("icon_facebook"), LinkIcon));
	Style->Set("WorldCreatorBridge.Discord", new IMAGE_BRUSH(TEXT("icon_discord"), LinkIcon));
	Style->Set("WorldCreatorBridge.Twitch", new IMAGE_BRUSH(TEXT("icon_twitch"), LinkIcon));
	Style->Set("WorldCreatorBridge.Vimeo", new IMAGE_BRUSH(TEXT("icon_vimeo"), LinkIcon));
	Style->Set("WorldCreatorBridge.Artstation", new IMAGE_BRUSH(TEXT("icon_artstation"), LinkIcon));

	return Style;
}

void FWorldCreatorBridgeStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FWorldCreatorBridgeStyle::Get()
{
	return *StyleInstance;
}
