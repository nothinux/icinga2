/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2017 Icinga Development Team (https://www.icinga.com/)  *
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

#include "base/rw_spin_lock.hpp"

using namespace icinga;

rw_spin_lock::rw_spin_lock(void)
    : _readers(0), _recursion(0)
{ }

void rw_spin_lock::acquire_reader(void)
{
	int retry = 0;

	for (;;) {
		uint16_t prev_readers = _readers;
		if (prev_readers != HAS_WRITER) {
			uint16_t new_readers = prev_readers + 1;
			if (_readers.compare_exchange_weak(prev_readers, new_readers)) {
				// we've won the race
				return;
			}
		}

		retry++;

		if (retry > RETRY_THRESHOLD) {
			retry = 0;
			boost::this_thread::yield();
		}
	}
}

void rw_spin_lock::release_reader(void)
{
	_readers--;
}

void rw_spin_lock::acquire_writer(void)
{
	uint16_t prev_readers = _readers;
	if (prev_readers == 0) {
		if (_readers.compare_exchange_weak(prev_readers, HAS_WRITER)) {
			_recursion++;
			_owner = boost::this_thread::get_id();

			// we've won the race
			return;
		}
	} else if (prev_readers == HAS_WRITER && _owner == boost::this_thread::get_id()) {
		_recursion++;
		return;
	}

	acquire_writer_slow();
}

void rw_spin_lock::acquire_writer_slow(void)
{
	boost::thread::id tid = boost::this_thread::get_id();

	int retry = 0;
	for (;;) {
		uint16_t prev_readers = _readers;
		if (prev_readers == 0) {
			if (_readers.compare_exchange_weak(prev_readers, HAS_WRITER)) {
				_recursion++;
				_owner = tid;

				// we've won the race
				return;
			}
		} else if (prev_readers == HAS_WRITER && _owner == tid) { // XXX: race condition for reading _owner
			_recursion++;
			return;
		}

		retry++;
		if (retry > RETRY_THRESHOLD) {
			// save some cpu cycles
			retry = 0;
			boost::this_thread::yield();
		}
	}
}

bool rw_spin_lock::try_upgrade_writer(void)
{
	uint16_t prev_readers = _readers;
	if (prev_readers == 1) {
		if (_readers.compare_exchange_weak(prev_readers, HAS_WRITER)) {
			_recursion = 1;
			_owner = boost::this_thread::get_id();

			// we've won the race
			return true;
		}
	}

	return false;
}

void rw_spin_lock::release_writer(void)
{
	if (--_recursion == 0) {
		_owner = boost::thread::id();
		_readers = 0;
	}
}
