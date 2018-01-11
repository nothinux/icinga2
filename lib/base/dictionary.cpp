/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2018 Icinga Development Team (https://www.icinga.com/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "base/dictionary.hpp"
#include "base/objectlock.hpp"
#include "base/debug.hpp"
#include "base/primitivetype.hpp"
#include "base/configwriter.hpp"

using namespace icinga;

template class std::map<String, Value>;

REGISTER_PRIMITIVE_TYPE(Dictionary, Object, Dictionary::GetPrototype());

static boost::shared_ptr<DictionaryData> EmptyDictionaryData = boost::make_shared<DictionaryData>();

Dictionary::Dictionary()
	: m_Data(EmptyDictionaryData)
{ }

Dictionary::Dictionary(DictionaryData data)
{
	std::sort(data.begin(), data.end(), [](const Dictionary::Pair& a, const Dictionary::Pair& b) {
		return a.first < b.first;
	});

	data.erase(std::unique(data.begin(), data.end(), [](const Dictionary::Pair& a, const Dictionary::Pair& b) {
		return a.first == b.first;
	}), data.end());

	m_Data = boost::make_shared<DictionaryData>(std::move(data));
}

Dictionary::Dictionary(std::initializer_list<Pair> init)
	: Dictionary(DictionaryData(init))
{ }

bool Dictionary::KeyLessComparer(const Pair& a, const String& b)
{
	return a.first < b;
}

/*
 * Retrieves a value from a dictionary.
 *
 * @param key The key whose value should be retrieved.
 * @returns The value of an empty value if the key was not found.
 */
Value Dictionary::Get(const String& key) const
{
	auto data = GetView();

	auto end = data->end();

	auto it = std::lower_bound(data->begin(), end, key, KeyLessComparer);

	if (it == end || it->first != key)
		return Empty;
	else
		return it->second;
}

/**
 * Retrieves a value from a dictionary.
 *
 * @param key The key whose value should be retrieved.
 * @param result The value of the dictionary item (only set when the key exists)
 * @returns true if the key exists, false otherwise.
 */
bool Dictionary::Get(const String& key, Value *result) const
{
	auto data = GetView();

	auto end = data->end();

	auto it = std::lower_bound(data->begin(), end, key, KeyLessComparer);

	if (it == end || it->first != key)
		return false;


	*result = it->second;
	return true;
}

/**
 * Sets a value in the dictionary.
 *
 * @param key The key.
 * @param value The value.
 */
void Dictionary::Set(const String& key, Value value)
{
	m_Data.copy_update([&key, &value](DictionaryData *data) {
		auto end = data->end();

		auto it = std::lower_bound(data->begin(), end, key, KeyLessComparer);

		if (it != end && it->first == key)
			it->second = value;
		else
			data->emplace(it, key, value);
	});
}

/**
 * Returns the number of elements in the dictionary.
 *
 * @returns Number of elements.
 */
size_t Dictionary::GetLength() const
{
	auto data = GetView();

	return data->size();
}

/**
 * Checks whether the dictionary contains the specified key.
 *
 * @param key The key.
 * @returns true if the dictionary contains the key, false otherwise.
 */
bool Dictionary::Contains(const String& key) const
{
	auto data = GetView();

	auto end = data->end();

	auto it = std::lower_bound(data->begin(), end, key, KeyLessComparer);

	return (it != end && it->first == key);
}

DictionaryView Dictionary::GetView() const
{
	return m_Data.read();
}

/**
 * Removes the specified key from the dictionary.
 *
 * @param key The key.
 */
void Dictionary::Remove(const String& key)
{
	m_Data.copy_update([&key](DictionaryData *data) {
		auto end = data->end();

		auto it = std::lower_bound(data->begin(), end, key, KeyLessComparer);

		if (it != end && it->first == key)
			data->erase(it);
	});
}

/**
 * Removes all dictionary items.
 */
void Dictionary::Clear()
{
	m_Data = boost::make_shared<DictionaryData>();
}

void Dictionary::CopyTo(const Dictionary::Ptr& dest) const
{
	for (const Dictionary::Pair& kv : GetView()) {
		dest->Set(kv.first, kv.second);
	}
}

/**
 * Makes a shallow copy of a dictionary.
 *
 * @returns a copy of the dictionary.
 */
Dictionary::Ptr Dictionary::ShallowClone() const
{
	return new Dictionary(*GetView());
}

/**
 * Makes a deep clone of a dictionary
 * and its elements.
 *
 * @returns a copy of the dictionary.
 */
Object::Ptr Dictionary::Clone() const
{
	DictionaryData result;

	for (const Dictionary::Pair& kv : GetView()) {
		result.emplace_back(kv.first, kv.second.Clone());
	}

	return new Dictionary(std::move(result));
}

/**
 * Returns an array containing all keys
 * which are currently set in this directory.
 *
 * @returns an array of key names
 */
std::vector<String> Dictionary::GetKeys() const
{
	std::vector<String> keys;

	for (const Dictionary::Pair& kv : GetView()) {
		keys.push_back(kv.first);
	}

	return keys;
}

String Dictionary::ToString() const
{
	std::ostringstream msgbuf;
	ConfigWriter::EmitScope(msgbuf, 1, const_cast<Dictionary *>(this));
	return msgbuf.str();
}

Value Dictionary::GetFieldByName(const String& field, bool, const DebugInfo& debugInfo) const
{
	Value value;

	if (Get(field, &value))
		return value;
	else
		return GetPrototypeField(const_cast<Dictionary *>(this), field, false, debugInfo);
}

void Dictionary::SetFieldByName(const String& field, const Value& value, const DebugInfo&)
{
	Set(field, value);
}

bool Dictionary::HasOwnField(const String& field) const
{
	return Contains(field);
}

bool Dictionary::GetOwnField(const String& field, Value *result) const
{
	return Get(field, result);
}

Dictionary::ConstIterator icinga::begin(const DictionaryView& x)
{
	return x->begin();
}

Dictionary::ConstIterator icinga::end(const DictionaryView& x)
{
	return x->end();
}

