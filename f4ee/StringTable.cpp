#include "StringTable.h"
#include "Utilities.h"

extern StringTable g_stringTable;

using namespace Serialization;

void DeleteStringEntry(const F4EEFixedString* string)
{
	g_stringTable.RemoveString(*string);
	delete string;
}

StringTableItem StringTable::GetString(const F4EEFixedString & str)
{
	SimpleLocker locker(&m_lock);
	
	auto it = m_table.find(str);
	if(it != m_table.end()) {
		return it->second.lock();
	} else {
		StringTableItem item = std::shared_ptr<F4EEFixedString>(new F4EEFixedString(str), DeleteStringEntry);
		m_table.emplace(str, item);
		m_tableVector.push_back(item);
		return item;
	}

	return nullptr;
}

void StringTable::RemoveString(const F4EEFixedString & str)
{
	SimpleLocker locker(&m_lock);
	auto it = m_table.find(str);
	if(it != m_table.end())
	{
		for(size_t i = m_tableVector.size(); i > 0; --i)
		{
			if(m_tableVector[i - 1].lock() == it->second.lock())
				m_tableVector.erase(m_tableVector.begin() + (i - 1));
		}

		m_table.erase(it);
	}
}

UInt32 StringTable::GetStringID(const StringTableItem & str)
{
	for(size_t i = m_tableVector.size(); i > 0; --i)
	{
		auto item = m_tableVector[i - 1].lock();
		if(item == str)
			return (UInt32)i - 1;
	}

	return -1;
}

void StringTable::Save(const F4SE::SerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('STTB', kVersion);

	UInt32 totalStrings = (UInt32)m_tableVector.size();
	WriteData<UInt32>(intfc, &totalStrings);

	for (auto & str : m_tableVector)
	{
		auto data = str.lock();
		UInt16 length = 0;
		if(!data) {
			WriteData<UInt16>(intfc, &length);
		} else {
			WriteData<F4EEFixedString>(intfc, data.get());
		}
	}
}

bool StringTable::Load(const F4SE::SerializationInterface * intfc, UInt32 kVersion, std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	bool error = false;
	UInt32 totalStrings = 0;

	if (!ReadData<UInt32>(intfc, &totalStrings))
	{
		_ERROR("%s - Error loading total strings from table", __FUNCTION__);
		error = true;
		return error;
	}

	for(UInt32 i = 0; i < totalStrings; i++)
	{
		F4EEFixedString str;
		if(!ReadData<F4EEFixedString>(intfc, &str)) {
			_ERROR("%s - Error loading string", __FUNCTION__);
			error = true;
			return error;
		}

		StringTableItem item = GetString(str);
		stringTable.emplace(i, item);
	}

	return error;
}

void StringTable::Revert()
{
	SimpleLocker locker(&m_lock);
	m_table.clear();
	m_tableVector.clear();
}
