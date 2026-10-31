#pragma once
#include <obs-module.h>
#include "Attribute.h"
#include "ImageLayer.h"
#include "WatchedImage.h"
#include "WatchedPythonScript.h"

namespace CharacterControlOBS
{
using AttributeMap = std::unordered_map<std::string, std::unique_ptr<Attribute<int>>>;
class Component
{
public:
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

	[[nodiscard]] const Attribute<int>* GetIntAttribute(StrIdRef AttrName) const;

	void Render(gs_effect_t* Effect) const;

	CharacterControlRender::ImageLayer& GetImageRoot();

private:
	std::vector<std::unique_ptr<Component>> Components;

	AttributeMap IntAttributes;

	CharacterControlRender::ImageLayer ImageRoot;

	std::unique_ptr<CharacterControlScript::WatchedPythonScript> ImageControlScript;
};
}
