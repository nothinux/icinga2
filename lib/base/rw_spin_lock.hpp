#ifndef RW_SPIN_LOCK_H
#define RW_SPIN_LOCK_H

#include "base/i2-base.hpp"
#include <boost/thread.hpp>

namespace icinga {

class rw_spin_lock
{
public:
	rw_spin_lock(void);

public:
	void acquire_reader(void);
	void release_reader(void);

	void acquire_writer(void);
	bool try_upgrade_writer(void);
	void release_writer(void);

private:
	static constexpr uint16_t HAS_WRITER = UINT16_MAX;
	static constexpr int RETRY_THRESHOLD = 100;
	std::atomic<uint16_t> _readers;
	uint16_t _recursion;
	boost::thread::id _owner;

	void acquire_writer_slow(void);
};

}

#endif /* RW_SPIN_LOCK_H */
