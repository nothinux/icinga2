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

#include "base/objectlock.hpp"
#include <boost/thread/recursive_mutex.hpp>

using namespace icinga;

#define I2MUTEX_UNLOCKED 0
#define I2MUTEX_LOCKED 1

ObjectLock::~ObjectLock()
{
	Unlock();
}

ObjectLock::ObjectLock(const Lockable *object)
	: m_Object(object), m_Locked(false)
{
	if (m_Object)
		Lock();
}

void ObjectLock::Lock()
{
	ASSERT(!m_Locked && m_Object);

	m_Object->m_Mutex.lock();
	m_Locked = true;
}

void ObjectLock::Unlock()
{
	if (m_Locked) {
		m_Object->m_Mutex.unlock();
		m_Locked = false;
	}
}
