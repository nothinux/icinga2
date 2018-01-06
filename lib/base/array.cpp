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

#include "base/array.hpp"
#include "base/debug.hpp"
#include "base/primitivetype.hpp"
#include "base/dictionary.hpp"
#include "base/configwriter.hpp"
#include "base/convert.hpp"
#include "base/exception.hpp"

using namespace icinga;

template class std::vector<Value>;

REGISTER_PRIMITIVE_TYPE(Array, Object, Array::GetPrototype());

static boost::shared_ptr<ArrayData> EmptyArrayData = boost::make_shared<ArrayData>();

Array::Array()
	: m_Data(EmptyArrayData)
{ }

Array::Array(const ArrayData& data)
	: m_Data(boost::make_shared<ArrayData>(data))
{ }

Array::Array(ArrayData&& data)
	: m_Data(boost::make_shared<ArrayData>(std::move(data)))
{ }

Array::Array(std::initializer_list<Value> init)
	: m_Data(boost::make_shared<ArrayData>(init))
{ }

/**
 * Restrieves a value from an array.
 *
 * @param index The index.
 * @returns The value.
 */
Value Array::Get(SizeType index) const
{
	auto data = GetView();
	return data->at(index);
}

/**
 * Sets a value in the array.
 *
 * @param index The index.
 * @param value The value.
 */
void Array::Set(SizeType index, const Value& value)
{
	m_Data.copy_update([index, &value](ArrayData *data) {
		data->at(index) = value;
	});
}

/**
 * Sets a value in the array.
 *
 * @param index The index.
 * @param value The value.
 */
void Array::Set(SizeType index, Value&& value)
{
	m_Data.copy_update([index, &value](ArrayData *data) {
		data->at(index).Swap(value);
	});
}

/**
 * Adds a value to the array.
 *
 * @param value The value.
 */
void Array::Add(Value value)
{
	m_Data.copy_update([&value](ArrayData *data) {
		data->push_back(value);
	});
}

ArrayView Array::GetView() const
{
	return m_Data.read();
}

/**
 * Returns the number of elements in the array.
 *
 * @returns Number of elements.
 */
size_t Array::GetLength() const
{
	auto data = GetView();
	return data->size();
}

/**
 * Checks whether the array contains the specified value.
 *
 * @param value The value.
 * @returns true if the array contains the value, false otherwise.
 */
bool Array::Contains(const Value& value) const
{
	auto data = GetView();
	return (std::find(data->begin(), data->end(), value) != data->end());
}

/**
 * Insert the given value at the specified index
 *
 * @param index The index
 * @param value The value to add
 */
void Array::Insert(SizeType index, Value value)
{
	m_Data.copy_update([index, &value](ArrayData *data) {
		ASSERT(index <= data->size());

		data->insert(data->begin() + index, value);
	});
}

/**
 * Removes the specified index from the array.
 *
 * @param index The index.
 */
void Array::Remove(SizeType index)
{
	m_Data.copy_update([index](ArrayData *data) {
		data->erase(data->begin() + index);
	});
}

void Array::Resize(SizeType newSize)
{
	m_Data.copy_update([newSize](ArrayData *data) {
		data->resize(newSize);
	});
}

void Array::Clear()
{
	m_Data = boost::make_shared<ArrayData>();
}

void Array::Reserve(SizeType newSize)
{
	m_Data.copy_update([newSize](ArrayData *data) {
		data->reserve(newSize);
	});
}

void Array::CopyTo(const Array::Ptr& dest) const
{
	auto ourData = GetView();

	dest->m_Data.copy_update([&ourData](ArrayData *data) {
		std::copy(ourData->begin(), ourData->end(), std::back_inserter(*data));
	});
}

/**
 * Makes a shallow copy of an array.
 *
 * @returns a copy of the array.
 */
Array::Ptr Array::ShallowClone() const
{
	return new Array(*GetView());
}

/**
 * Makes a deep clone of an array
 * and its elements.
 *
 * @returns a copy of the array.
 */
Object::Ptr Array::Clone() const
{
	ArrayData result;

	for (const Value& val : GetView()) {
		result.push_back(val.Clone());
	}

	return new Array(std::move(result));
}

Array::Ptr Array::Reverse() const
{
	Array::Ptr result = new Array();

	auto ourData = GetView();

	result->m_Data.copy_update([&ourData](ArrayData *data) {
		std::copy(ourData->rbegin(), ourData->rend(), std::back_inserter(*data));
	});

	return result;
}

void Array::Sort()
{
	m_Data.copy_update([](ArrayData *data) {
		std::sort(data->begin(), data->end());
	});
}

String Array::ToString() const
{
	std::ostringstream msgbuf;
	ConfigWriter::EmitArray(msgbuf, 1, const_cast<Array *>(this));
	return msgbuf.str();
}

Value Array::GetFieldByName(const String& field, bool sandboxed, const DebugInfo& debugInfo) const
{
	int index;

	try {
		index = Convert::ToLong(field);
	} catch (...) {
		return Object::GetFieldByName(field, sandboxed, debugInfo);
	}

	auto data = GetView();

	if (index < 0 || static_cast<size_t>(index) >= data->size())
		BOOST_THROW_EXCEPTION(ScriptError("Array index '" + Convert::ToString(index) + "' is out of bounds.", debugInfo));

	return data->at(index);
}

void Array::SetFieldByName(const String& field, const Value& value, const DebugInfo& debugInfo)
{
	int index = Convert::ToLong(field);

	if (index < 0)
		BOOST_THROW_EXCEPTION(ScriptError("Array index '" + Convert::ToString(index) + "' is out of bounds.", debugInfo));

	m_Data.copy_update([index, &value](ArrayData *data) {
		if (static_cast<size_t>(index) >= data->size())
			data->resize(index + 1);

		data->at(index) = value;
	});
}

Array::ConstIterator icinga::begin(const ArrayView& x)
{
	return x->begin();
}

Array::ConstIterator icinga::end(const ArrayView& x)
{
	return x->end();
}
