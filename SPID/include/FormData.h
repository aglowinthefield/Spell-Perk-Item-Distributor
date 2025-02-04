#pragma once

#include "LookupConfigs.h"
#include "LookupFilters.h"

namespace Forms
{
	namespace detail
	{
		inline void get_merged_IDs(std::optional<RE::FormID>& a_formID, std::optional<std::string>& a_modName)
		{
			const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(a_modName.value_or("").c_str(), a_formID.value_or(0));
			std::string conversion_log{};
			if (a_formID.value_or(0) && mergedFormID && a_formID.value_or(0) != mergedFormID) {
				conversion_log = std::format("0x{:X}->0x{:X}", a_formID.value_or(0), mergedFormID);
				a_formID.emplace(mergedFormID);
			}
			const std::string mergedModString{ mergedModName };
			if (!a_modName.value_or("").empty() && !mergedModString.empty() && a_modName.value_or("") != mergedModString) {
				if (conversion_log.empty()) {
					conversion_log = std::format("{}->{}", a_modName.value_or(""), mergedModString);
				} else {
					conversion_log = std::format("{}~{}->{}", conversion_log, a_modName.value_or(""), mergedModString);
				}
				a_modName.emplace(mergedModName);
			}
			if (!conversion_log.empty()) {
				buffered_logger::info("\t\tFound merged: {}", conversion_log);
			}
		}

		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, RawFormVec& a_rawFormVec, FormVec& a_formVec, const std::string& a_path, bool a_all = false)
		{
			if (a_rawFormVec.empty()) {
				return true;
			}
			for (auto& formOrEditorID : a_rawFormVec) {
				if (const auto formModPair(std::get_if<FormModPair>(&formOrEditorID)); formModPair) {
					auto& [formID, modName] = *formModPair;
					if (g_mergeMapperInterface) {
						get_merged_IDs(formID, modName);
					}
					if (modName && !formID) {
						if (const RE::TESFile* filterMod = a_dataHandler->LookupModByName(*modName); filterMod) {
							a_formVec.emplace_back(filterMod);
						} else {
							buffered_logger::error("\t\t[{}] Filter ({}) SKIP - mod cannot be found", a_path, *modName);
						}
					} else if (formID) {
						if (auto filterForm = modName ?
						                          a_dataHandler->LookupForm(*formID, *modName) :
						                          RE::TESForm::LookupByID(*formID)) {
							const auto formType = filterForm->GetFormType();
							if (Cache::FormType::GetWhitelisted(formType)) {
								a_formVec.emplace_back(filterForm);
							} else {
								buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", a_path, *formID, modName.value_or(""), formType);
							}
						} else {
							buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - form doesn't exist", a_path, *formID, modName.value_or(""));
						}
					}
				} else if (std::holds_alternative<std::string>(formOrEditorID)) {
					if (auto editorID = std::get<std::string>(formOrEditorID); !editorID.empty()) {
						if (auto filterForm = RE::TESForm::LookupByEditorID(editorID); filterForm) {
							const auto formType = filterForm->GetFormType();
							if (Cache::FormType::GetWhitelisted(formType)) {
								a_formVec.emplace_back(filterForm);
							} else {
								buffered_logger::error("\t\t[{}] Filter ({}) SKIP - invalid formtype ({})", a_path, editorID, formType);
							}
						} else {
							buffered_logger::error("\t\t[{}] Filter ({}) SKIP - form doesn't exist", a_path, editorID);
						}
					}
				}
			}
			return !a_all && !a_formVec.empty() || a_formVec.size() == a_rawFormVec.size();
		}
	}

	template <class Form>
	struct Data
	{
		std::uint32_t index{ 0 };
		Form*         form{ nullptr };
		IdxOrCount    idxOrCount{ 1 };
		FilterData    filters{};
		std::string   path{};

		bool operator==(const Data& a_rhs) const;
	};

	template <class Form>
	using DataVec = std::vector<Data<Form>>;

	template <class Form>
	struct Distributables
	{
		Distributables(RECORD::TYPE a_type) :
			type(a_type)
		{}

		explicit operator bool();

		std::size_t GetSize() const;
		std::size_t GetLeveledSize() const;

		RECORD::TYPE GetType() const;

		const DataVec<Form>& GetForms(bool a_onlyLevelEntries);
		DataVec<Form>&       GetForms();

		void LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec);

		// Init formsWithLevels and formsNoLevels
		void FinishLookupForms();

	private:
		RECORD::TYPE  type;
		DataVec<Form> forms{};
		DataVec<Form> formsWithLevels{};
	};

	inline Distributables<RE::SpellItem>      spells{ RECORD::kSpell };
	inline Distributables<RE::BGSPerk>        perks{ RECORD::kPerk };
	inline Distributables<RE::TESBoundObject> items{ RECORD::kItem };
	inline Distributables<RE::TESShout>       shouts{ RECORD::kShout };
	inline Distributables<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
	inline Distributables<RE::TESForm>        packages{ RECORD::kPackage };
	inline Distributables<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
	inline Distributables<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
	inline Distributables<RE::TESBoundObject> deathItems{ RECORD::kDeathItem };
	inline Distributables<RE::TESFaction>     factions{ RECORD::kFaction };
	inline Distributables<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
	inline Distributables<RE::TESObjectARMO>  skins{ RECORD::kSkin };

	std::size_t GetTotalEntries();
	std::size_t GetTotalLeveledEntries();

	template <typename Func, typename... Args>
	void ForEachDistributable(Func&& a_func, Args&&... args)
	{
		a_func(keywords, std::forward<Args>(args)...);
		a_func(spells, std::forward<Args>(args)...);
		a_func(levSpells, std::forward<Args>(args)...);
		a_func(perks, std::forward<Args>(args)...);
		a_func(shouts, std::forward<Args>(args)...);
		a_func(items, std::forward<Args>(args)...);
		a_func(deathItems, std::forward<Args>(args)...);
		a_func(outfits, std::forward<Args>(args)...);
		a_func(sleepOutfits, std::forward<Args>(args)...);
		a_func(factions, std::forward<Args>(args)...);
		a_func(packages, std::forward<Args>(args)...);
		a_func(skins, std::forward<Args>(args)...);
	}
}

template <class Form>
bool Forms::Data<Form>::operator==(const Data& a_rhs) const
{
	if (!form || !a_rhs.form) {
		return false;
	}
	return form->GetFormID() == a_rhs.form->GetFormID();
}

template <class Form>
Forms::Distributables<Form>::operator bool()
{
	return !forms.empty();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetSize() const
{
	return forms.size();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetLeveledSize() const
{
	return formsWithLevels.size();
}

template <class Form>
RECORD::TYPE Forms::Distributables<Form>::GetType() const
{
	return type;
}

template <class Form>
Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms()
{
	return forms;
}

template <class Form>
const Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms(bool a_onlyLevelEntries)
{
	if (a_onlyLevelEntries) {
		return formsWithLevels;
	}
	return forms;
}

template <class Form>
void Forms::Distributables<Form>::LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec)
{
	if (a_INIDataVec.empty()) {
		return;
	}

	logger::info("{}", a_type);

	forms.reserve(a_INIDataVec.size());
	std::uint32_t index = 0;

	for (auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] : a_INIDataVec) {
		Form* form = nullptr;

		if (std::holds_alternative<FormModPair>(formOrEditorID)) {
			if (auto [formID, modName] = std::get<FormModPair>(formOrEditorID); formID) {
				if (g_mergeMapperInterface) {
					detail::get_merged_IDs(formID, modName);
				}
				if (modName) {
					form = a_dataHandler->LookupForm<Form>(*formID, *modName);
					if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
						if (!form) {
							const auto base = a_dataHandler->LookupForm(*formID, *modName);
							form = base ? static_cast<Form*>(base) : nullptr;
						}
					}
				} else {
					form = RE::TESForm::LookupByID<Form>(*formID);
					if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
						if (!form) {
							const auto base = RE::TESForm::LookupByID(*formID);
							form = base ? static_cast<Form*>(base) : nullptr;
						}
					}
				}
				if (!form) {
					buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - formID doesn't exist", path, *formID, modName.value_or(""));
				} else {
					if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
						if (string::is_empty(form->GetFormEditorID())) {
							form = nullptr;
							buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - keyword does not have a valid editorID", path, *formID, modName.value_or(""));
						}
					}
				}
			}
		} else if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
			if (!std::holds_alternative<std::string>(formOrEditorID)) {
				continue;
			}

			if (auto keywordName = std::get<std::string>(formOrEditorID); !keywordName.empty()) {
				auto& keywordArray = a_dataHandler->GetFormArray<RE::BGSKeyword>();

				auto result = std::find_if(keywordArray.begin(), keywordArray.end(), [&](const auto& keyword) {
					return keyword && keyword->formEditorID == keywordName.c_str();
				});

				if (result != keywordArray.end()) {
					if (const auto keyword = *result; keyword) {
						form = keyword;
					} else {
						buffered_logger::critical("\t[{}] {} FAIL - couldn't get existing keyword", path, keywordName);
						continue;
					}
				} else {
					const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
					if (auto keyword = factory ? factory->Create() : nullptr; keyword) {
						keyword->formEditorID = keywordName;
						keywordArray.push_back(keyword);

						form = keyword;
					} else {
						buffered_logger::critical("\t[{}] {} FAIL - couldn't create keyword", path, keywordName);
					}
				}
			}
		} else if (std::holds_alternative<std::string>(formOrEditorID)) {
			if (auto editorID = std::get<std::string>(formOrEditorID); !editorID.empty()) {
				form = RE::TESForm::LookupByEditorID<Form>(editorID);
				if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
					if (!form) {
						const auto base = RE::TESForm::LookupByEditorID(editorID);
						form = base ? static_cast<Form*>(base) : nullptr;
					}
				}
				if (!form) {
					buffered_logger::error("\t[{}] {} FAIL - editorID doesn't exist", path, editorID);
				}
			}
		}

		if (!form) {
			continue;
		}

		FormFilters filterForms{};

		bool validEntry = detail::formID_to_form(a_dataHandler, filterIDs.ALL, filterForms.ALL, path, true);
		if (validEntry) {
			validEntry = detail::formID_to_form(a_dataHandler, filterIDs.NOT, filterForms.NOT, path);
		}
		if (validEntry) {
			validEntry = detail::formID_to_form(a_dataHandler, filterIDs.MATCH, filterForms.MATCH, path);
		}

		if (!validEntry) {
			continue;
		}

		forms.emplace_back(index, form, idxOrCount, FilterData(strings, filterForms, level, traits, chance), path);
		index++;
	}
}

template <class Form>
void Forms::Distributables<Form>::FinishLookupForms()
{
	if (forms.empty()) {
		return;
	}

	// reverse overridable form vectors, winning configs first (Zzzz -> Aaaa)
	// entry order within config is preserved
	// thanks, chatGPT!
	if constexpr (std::is_same_v<RE::BGSOutfit, Form> || std::is_same_v<RE::TESObjectARMO, Form>) {
		std::stable_sort(forms.begin(), forms.end(), [](const auto& a_form1, const auto& a_form2) {
			return a_form1.path > a_form2.path;  // Compare paths in reverse order
		});
	}

	formsWithLevels.reserve(forms.size());

	std::copy_if(forms.begin(), forms.end(),
		std::back_inserter(formsWithLevels),
		[](const auto& formData) { return formData.filters.HasLevelFilters(); });
}
