#include "PCH.h"
#include "Character.h"
#include "PythonInterface.h"
#include "WatchedImage.h"
#include "WatchedPythonScript.h"

using namespace CharacterControlOBS;

Character::Character()
{
	ImageRoot.CreateChildLayer("default");
}

void Character::AddComponent(std::unique_ptr<Component>&& NewComponent)
{
	Components.emplace_back(std::move(NewComponent))->DeclareAttributes(IntAttributes);
}

void Character::SetupImageControlScript(const std::filesystem::path& Filename)
{
	ImageControlScript = std::make_unique<CharacterControlScript::WatchedPythonScript>(Filename);
}

void Character::UpdateAttributes() const
{
	for(auto& ComponentPtr : Components)
	{
		ComponentPtr->UpdateAttributes(IntAttributes);
	}
}

void Character::UpdateRenderState()
{
	if(ImageControlScript && CharacterControlScript::PythonInterface::TryGet() != nullptr)
	{
		CharacterControlScript::ScopedGil Gil;
		ImageControlScript->Update();

		PyObject* PArgs = PyTuple_New(1);
		PyObject* PDict = PyDict_New();
		if(PDict != nullptr)
		{
			for(auto& [fst, snd] : IntAttributes)
			{
				if(snd != nullptr)
				{
					if(PyObject* PVal = PyLong_FromLong(snd->GetValue()); PVal != nullptr)
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

const Attribute<int>* Character::GetIntAttribute(StrIdRef AttrName) const
{
	if(const auto FoundAttr = IntAttributes.find(AttrName); FoundAttr != IntAttributes.end())
	{
		return FoundAttr->second.get();
	}

	return nullptr;
}

void Character::Render(gs_effect_t* Effect) const
{
	ImageRoot.Render(Effect);
}

CharacterControlRender::ImageLayer& Character::GetImageRoot()
{
	return ImageRoot;
}
