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

#ifndef ARRAY_H
#define ARRAY_H

#include "base/i2-base.hpp"
#include "base/value.hpp"
#include "base/rcu_ptr.hpp"
#include <boost/range/iterator.hpp>
#include <vector>
#include <set>

namespace icinga
{

using ArrayData = std::vector<Value>;
using ArrayView = boost::shared_ptr<const ArrayData>;

/**
 * An array of Value items.
 *
 * @ingroup base
 */
class Array final : public Object
{
public:
	DECLARE_OBJECT(Array);

	/**
	 * An iterator that can be used to iterate over array elements.
	 */
	typedef ArrayData::const_iterator ConstIterator;

	typedef ArrayData::size_type SizeType;

	Array();
	Array(const ArrayData& other);
	Array(ArrayData&& other);
	Array(std::initializer_list<Value> init);

	Value Get(SizeType index) const;
	void Set(SizeType index, const Value& value);
	void Set(SizeType index, Value&& value);
	void Add(Value value);

	ArrayView GetView() const;

	size_t GetLength() const;
	bool Contains(const Value& value) const;

	void Insert(SizeType index, Value value);
	void Remove(SizeType index);

	void Resize(SizeType newSize);
	void Clear();

	void Reserve(SizeType newSize);

	void CopyTo(const Array::Ptr& dest) const;
	Array::Ptr ShallowClone() const;

	static Object::Ptr GetPrototype();

	template<typename T>
	static Array::Ptr FromVector(const std::vector<T>& v)
	{
		static_assert(!std::is_same<T, Value>::value, "T must not be Value");

		std::vector<Value> result;
		result.reserve(v.size());
		std::copy(v.begin(), v.end(), std::back_inserter(result));
		return new Array(std::move(result));
	}

	template<typename T>
	std::set<T> ToSet()
	{
		auto data = GetView();
		return std::set<T>(data->begin(), data->end());
	}

	template<typename T>
	static Array::Ptr FromSet(const std::set<T>& v)
	{
		Array::Ptr result = new Array();
		result->m_Data.copy_update([&v](ArrayData *data) {
			std::copy(v.begin(), v.end(), std::back_inserter(*data));
		});
		return result;
	}

	Object::Ptr Clone() const override;

	Array::Ptr Reverse() const;

	void Sort();

	String ToString() const override;

	Value GetFieldByName(const String& field, bool sandboxed, const DebugInfo& debugInfo) const override;
	void SetFieldByName(const String& field, const Value& value, const DebugInfo& debugInfo) override;

private:
	rcu_ptr<ArrayData> m_Data; /**< The data for the array. */
};

Array::ConstIterator begin(const ArrayView& x);
Array::ConstIterator end(const ArrayView& x);

}

extern template class std::vector<icinga::Value>;

#endif /* ARRAY_H */
