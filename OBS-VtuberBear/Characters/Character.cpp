// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Characters/Character.h"
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "Render/WatchedImage.h"

using namespace CharacterControlOBS;

void CharacterControlOBS::SetAttribute(const AttributeMap& Map, StrIdRef Name, const Attribute::Value& Value)
{
	const auto Found = Map.find(Name);
	if(Found == Map.end() || Found->second == nullptr)
	{
		return;
	}

	if(!Found->second->Set(Value))
	{
		ERROR_("Cannot set attribute \"%s\" of type %s to %s.", reinterpret_cast<const char*>(Name.c_str()), Found->second->GetTypeName(), Attribute::GetTypeName(Value));
	}
}

Character::Character()
{
	ImageRoot.CreateChildLayer(u8"default");
}

void Character::AddComponent(std::unique_ptr<Component>&& NewComponent)
{
	Components.emplace_back(std::move(NewComponent))->DeclareAttributes(Attributes);
}

void Character::UpdateAttributes() const
{
	for(const auto& ComponentPtr : Components)
	{
		ComponentPtr->UpdateAttributes(Attributes);
	}
}

const Attribute* Character::GetAttribute(StrIdRef AttrName) const
{
	if(const auto FoundAttr = Attributes.find(AttrName); FoundAttr != Attributes.end())
	{
		return FoundAttr->second.get();
	}

	return nullptr;
}

void Character::Render(const gs_effect_t* Effect) const
{
	ImageRoot.Render(Effect);
}

CharacterControlRender::ImageLayer& Character::GetImageRoot()
{
	return ImageRoot;
}
