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

using namespace icinga;

RLock::RLock(const Object::Ptr& object)
    : m_RWLock(object->m_RWLock)
{
	Lock();
}

RLock::RLock(const Object *object)
    : m_RWLock(object->m_RWLock)
{
	Lock();
}

RLock::RLock(rw_spin_lock& rwlock)
    : m_RWLock(rwlock)
{
	Lock();
}

RLock::~RLock(void)
{
	if (m_NeedReadUnlock || m_NeedWriteUnlock)
		Unlock();
}

void RLock::Lock(void)
{
	m_RWLock.acquire_reader();
	m_NeedReadUnlock = true;
}

void RLock::Unlock(void)
{
	ASSERT(m_NeedReadUnlock || m_NeedWriteUnlock);

	if (m_NeedReadUnlock) {
		m_RWLock.release_reader();
		m_NeedReadUnlock = false;
	} else if (m_NeedWriteUnlock) {
		m_RWLock.release_writer();
		m_NeedWriteUnlock = false;
	}
}

bool RLock::TryUpgrade(void)
{
	ASSERT(m_NeedReadUnlock);

	if (m_RWLock.try_upgrade_writer()) {
		m_NeedReadUnlock = false;
		m_NeedWriteUnlock = true;
		return true;
	}

	return false;
}

void RLock::UnlockAndSleep(void)
{
	Unlock();
	boost::thread::yield();
}

WLock::WLock(const Object::Ptr& object)
    : m_RWLock(object->m_RWLock)
{
	Lock();
}

WLock::WLock(const Object *object)
    : m_RWLock(object->m_RWLock)
{
	Lock();
}

WLock::WLock(rw_spin_lock& rwlock)
    : m_RWLock(rwlock)
{
	Lock();
}

WLock::~WLock(void)
{
	if (m_NeedUnlock)
		Unlock();
}

void WLock::Lock(void)
{
	m_RWLock.acquire_writer();
	m_NeedUnlock = true;
}

void WLock::Unlock(void)
{
	m_RWLock.release_writer();
	m_NeedUnlock = false;
}
