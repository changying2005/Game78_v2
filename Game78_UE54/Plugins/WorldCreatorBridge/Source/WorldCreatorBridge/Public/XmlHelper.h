// Copyright BiteTheBytes GmbH
#pragma once

#include "Runtime/XmlParser/Public/XmlParser.h"
#include "Runtime/XmlParser/Public/XmlNode.h"
#include "Math/Vector2D.h"

class XmlHelper
{
public:
	static float GetFloat(FXmlNode* node, FString attributeName)
	{
		auto att = node->GetAttribute(attributeName);
		if (att.IsEmpty())
			return 0.0f;
		return FCString::Atof(att.GetCharArray().GetData());
	}

	static void GetFloat2(FXmlNode* node, FString attributeName, float* tx, float* ty)
	{
		auto att = node->GetAttribute(attributeName);
		if (att.IsEmpty())
			return;
		FString x, y;
		att.Split(TEXT(","), &x, &y);

		*tx = FCString::Atof(x.GetCharArray().GetData());
		*ty = FCString::Atof(y.GetCharArray().GetData());
	}

	static int GetInt(FXmlNode* node, FString attributeName)
	{
		auto att = node->GetAttribute(attributeName);
		if (att.IsEmpty())
			return -1;
		return FCString::Atoi(att.GetCharArray().GetData());
	}

	static FString GetString(FXmlNode* node, FString attributeName)
	{
		auto att = node->GetAttribute(attributeName);
		if (att.IsEmpty())
			return TEXT("");
		return att;
	}
};