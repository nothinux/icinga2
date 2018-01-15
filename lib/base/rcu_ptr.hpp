#pragma once

#include <memory>
#include <atomic>
#include <boost/smart_ptr/make_shared.hpp>

template <typename T>
class rcu_ptr {
	boost::shared_ptr<T> sp;

public:
	// TODO add
	// template <typename Y>
	// rcu_ptr(const std::boost::shared_ptr<Y>& r) {}

	rcu_ptr() = default;

	rcu_ptr(const boost::shared_ptr<T>& desired)
		: sp(desired)
	{ }

	rcu_ptr(boost::shared_ptr<T>&& desired)
		: sp(std::move(desired))
	{ }

	rcu_ptr(const rcu_ptr&) = delete;
	rcu_ptr& operator=(const rcu_ptr&) = delete;
	rcu_ptr(rcu_ptr&&) = delete;
	rcu_ptr& operator=(rcu_ptr&&) = delete;

	~rcu_ptr() = default;

	void operator=(const boost::shared_ptr<T>& desired) { reset(desired); }

	boost::shared_ptr<const T> read() const {
		return boost::atomic_load_explicit(&sp, std::memory_order_consume);
	}

	// Overwrites the content of the wrapped boost::shared_ptr.
	// We can use it to reset the wrapped data to a new value independent from
	// the old value. ( e.g. vector.clear() )
	void reset(const boost::shared_ptr<T>& r) {
		boost::atomic_store_explicit(&sp, r, std::memory_order_release);
	}

	void reset(boost::shared_ptr<T>&& r) {
		boost::atomic_store_explicit(&sp, std::move(r), std::memory_order_release);
	}

	// Updates the content of the wrapped boost::shared_ptr.
	// We can use it to update the wrapped data to a new value which is
	// dependent from the old value ( e.g. vector.push_back() ).
	//
	// @param fun is a lambda which is called whenever an update
	// needs to be done, i.e. it will be called continuously until the update is
	// successful.
	//
	// A call expression with this function is invalid,
	// if T is a non-copyable type.
	template <typename R>
	void copy_update(R&& fun) {
		boost::shared_ptr<T> sp_l = boost::atomic_load_explicit(&sp, std::memory_order_consume);
		boost::shared_ptr<T> r;

		do {
			if (sp_l) {
				// deep copy
				r = boost::make_shared<T>(*sp_l);
			}

			// update
			std::forward<R>(fun)(r.get());
		} while (!boost::atomic_compare_exchange_explicit(&sp, &sp_l, std::move(r),
											  std::memory_order_release,
											  std::memory_order_consume));
	}
};

