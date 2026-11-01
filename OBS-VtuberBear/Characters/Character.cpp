#include "PCH.h"
#include "Characters/Character.h"
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterControlCore/Overloaded.h"
#include "Python/PythonInterface.h"
#include "Python/WatchedPythonScript.h"
#include "Render/WatchedImage.h"

using namespace CharacterControlOBS;

namespace
{
PyObject* ToPython(const Attribute::Value& Value)
{
	// @formatter:off
	return std::visit(Overloaded{
		[](const bool Held) { return PyBool_FromLong(Held ? 1 : 0); },
		[](const int64_t Held) { return PyLong_FromLongLong(Held); },
		[](const double Held) { return PyFloat_FromDouble(Held); },
		[](const Str& Held) { return PyUnicode_FromStringAndSize(Held.data(), static_cast<Py_ssize_t>(Held.size())); },
		[]<typename T>(const T&) { static_assert(ALWAYS_FALSE<T>); },
	}, Value);
	// @formatter:on
}
}

void CharacterControlOBS::SetAttribute(const AttributeMap& Map, StrIdRef Name, const Attribute::Value& Value)
{
	const auto Found = Map.find(Name);
	if(Found == Map.end() || Found->second == nullptr)
	{
		return;
	}

	if(!Found->second->Set(Value))
	{
		ERROR_("Attribute \"%s\" is of type %s, so it can't be set to a value of type %s.", Name.c_str(), Found->second->GetTypeName(), Attribute::GetTypeName(Value));
	}
}

Character::Character()
{
	ImageRoot.CreateChildLayer("default");
}

void Character::AddComponent(std::unique_ptr<Component>&& NewComponent)
{
	Components.emplace_back(std::move(NewComponent))->DeclareAttributes(Attributes);
}

void Character::SetupImageControlScript(const std::filesystem::path& Filename)
{
	ImageControlScript = std::make_unique<CharacterControlScript::WatchedPythonScript>(Filename);
}

void Character::UpdateAttributes() const
{
	for(const auto& ComponentPtr : Components)
	{
		ComponentPtr->UpdateAttributes(Attributes);
	}
}

void Character::UpdateRenderState()
{
	if(ImageControlScript && CharacterControlScript::PythonInterface::TryGet() != nullptr)
	{
		const CharacterControlScript::ScopedGil Gil;
		ImageControlScript->Update();

		PyObject* PArgs = PyTuple_New(1);
		PyObject* PDict = PyDict_New();
		if(PDict != nullptr)
		{
			for(auto& [fst, snd] : Attributes)
			{
				if(snd != nullptr)
				{
					if(PyObject* PVal = ToPython(snd->Get()); PVal != nullptr)
					{
						// SetItemString takes its own reference, so release ours
						PyDict_SetItemString(PDict, fst.c_str(), PVal);
						Py_DECREF(PVal);
					}
				}
			}
		}
		// SetItem takes over our reference to pDict, so releasing pArgs frees both
		PyTuple_SetItem(PArgs, 0, PDict);
		Py_DECREF(PArgs);
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
