#pragma once
#include "CharacterRig/Attribute.h"
#include "Python/WatchedPythonScript.h"
#include "Render/ImageLayer.h"
#include "Render/WatchedImage.h"

namespace CharacterControlOBS
{
using AttributeMap = std::unordered_map<StrId, std::unique_ptr<Attribute>>;

void SetAttribute(const AttributeMap& Map, StrIdRef Name, const Attribute::Value& Value);

class Component
{
public:
	Component() = default;
	Component(const Component&) = delete;
	Component(Component&&) = delete;
	Component& operator=(const Component&) = delete;
	Component& operator=(Component&&) = delete;
	virtual ~Component() = default;
	virtual void DeclareAttributes(AttributeMap& Map) = 0;
	virtual void UpdateAttributes(const AttributeMap& Map) = 0;
};

class Character
{
public:
	Character();

	void AddComponent(std::unique_ptr<Component>&& NewComponent);

	void SetupImageControlScript(const std::filesystem::path& Filename);

	void UpdateAttributes() const;

	void UpdateRenderState();

	[[nodiscard]] const Attribute* GetAttribute(StrIdRef AttrName) const;

	void Render(const gs_effect_t* Effect) const;

	CharacterControlRender::ImageLayer& GetImageRoot();

private:
	std::vector<std::unique_ptr<Component>> Components;

	AttributeMap Attributes;

	CharacterControlRender::ImageLayer ImageRoot;

	std::unique_ptr<CharacterControlScript::WatchedPythonScript> ImageControlScript;
};
}
