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

#ifndef OBJECTLOCK_H
#define OBJECTLOCK_H

#include "base/object.hpp"

namespace icinga
{

struct RLock
{
public:
	RLock(const Object::Ptr& object);
	RLock(const Object *object);
	RLock(rw_spin_lock& rwlock);

	~RLock(void);

	void Lock(void);
	void Unlock(void);
	bool TryUpgrade(void);
	void UnlockAndSleep(void);

private:
	rw_spin_lock &m_RWLock;
	bool m_NeedReadUnlock;
	bool m_NeedWriteUnlock;
};

struct WLock
{
public:
	WLock(const Object::Ptr& object);
	WLock(const Object *object);
	WLock(rw_spin_lock& rwlock);

	~WLock(void);

	void Lock(void);
	void Unlock(void);

private:
	rw_spin_lock& m_RWLock;
	bool m_NeedUnlock;
};

}

#endif /* OBJECTLOCK_H */
