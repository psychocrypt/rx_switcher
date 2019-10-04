#pragma once

#include <assert.h>
#include <string.h>
#include <string>
#include <array>

// Structures that we use to pass info between threads constructors are here just to make
// the stack allocation take up less space, heap is a shared resource that needs locks too of course

struct pool_job
{

	uint64_t iBlockHeight = uint64_t(-1);

	pool_job() = default;
};


struct sock_err
{
	std::string sSocketError;
	bool silent;

	sock_err() {}
	sock_err(std::string&& err, bool silent) :
		sSocketError(std::move(err)),
		silent(silent) {}
	sock_err(sock_err&& from) :
		sSocketError(std::move(from.sSocketError)),
		silent(from.silent) {}

	sock_err& operator=(sock_err&& from)
	{
		assert(this != &from);
		sSocketError = std::move(from.sSocketError);
		silent = from.silent;
		return *this;
	}

	~sock_err() {}

	sock_err(sock_err const&) = delete;
	sock_err& operator=(sock_err const&) = delete;
};

// Unlike socket errors, GPU errors are read-only strings
struct gpu_res_err
{
	size_t idx; // GPU index
	const char* error_str;
	gpu_res_err(const char* error_str, size_t idx) :
		error_str(error_str),
		idx(idx) {}
};

enum ex_event_name
{
	EV_INVALID_VAL,
	EV_SOCK_READY,
	EV_SOCK_ERROR,
	EV_POOL_HAVE_JOB,
	EV_PERF_TICK,
	EV_EVAL_POOL_CHOICE,
	EV_HASHRATE_LOOP
};

/*
   This is how I learned to stop worrying and love c++11 =).
   Ghosts of endless heap allocations have finally been exorcised. Thanks
   to the nifty magic of move semantics, string will only be allocated
   once on the heap. Considering that it makes a journey across stack,
   heap alloced queue, to another stack before being finally processed
   I think it is kind of nifty, don't you?
   Also note that for non-arg events we only copy two qwords
*/

struct ex_event
{
	ex_event_name iName;
	size_t iPoolId;

	union {
		pool_job oPoolJob;
		sock_err oSocketError;
		gpu_res_err oGpuError;
	};

	ex_event()
	{
		iName = EV_INVALID_VAL;
		iPoolId = 0;
	}
	ex_event(std::string&& err, bool silent, size_t id) :
		iName(EV_SOCK_ERROR),
		iPoolId(id),
		oSocketError(std::move(err), silent) {}
	ex_event(pool_job dat, size_t id) :
		iName(EV_POOL_HAVE_JOB),
		iPoolId(id),
		oPoolJob(dat) {}
	ex_event(ex_event_name ev, size_t id = 0) :
		iName(ev),
		iPoolId(id) {}

	// Delete the copy operators to make sure we are moving only what is needed
	ex_event(ex_event const&) = delete;
	ex_event& operator=(ex_event const&) = delete;

	ex_event(ex_event&& from)
	{
		iName = from.iName;
		iPoolId = from.iPoolId;

		switch(iName)
		{
		case EV_SOCK_ERROR:
			new(&oSocketError) sock_err(std::move(from.oSocketError));
			break;
		case EV_POOL_HAVE_JOB:
			oPoolJob = from.oPoolJob;
			break;
		default:
			break;
		}
	}

	ex_event& operator=(ex_event&& from)
	{
		assert(this != &from);

		if(iName == EV_SOCK_ERROR)
			oSocketError.~sock_err();

		iName = from.iName;
		iPoolId = from.iPoolId;

		switch(iName)
		{
		case EV_SOCK_ERROR:
			new(&oSocketError) sock_err();
			oSocketError = std::move(from.oSocketError);
			break;
		case EV_POOL_HAVE_JOB:
			oPoolJob = from.oPoolJob;
			break;
		default:
			break;
		}

		return *this;
	}

	~ex_event()
	{
		if(iName == EV_SOCK_ERROR)
			oSocketError.~sock_err();
	}
};

inline uint64_t t32_to_t64(uint32_t t) { return 0xFFFFFFFFFFFFFFFFULL / (0xFFFFFFFFULL / ((uint64_t)t)); }
inline uint64_t t64_to_diff(uint64_t t) { return 0xFFFFFFFFFFFFFFFFULL / t; }
inline uint64_t diff_to_t64(uint64_t d) { return 0xFFFFFFFFFFFFFFFFULL / d; }

#include <chrono>
//Get steady_clock timestamp - misc helper function
inline size_t get_timestamp()
{
	using namespace std::chrono;
	return time_point_cast<seconds>(steady_clock::now()).time_since_epoch().count();
};

//Get millisecond timestamp
inline size_t get_timestamp_ms()
{
	using namespace std::chrono;
	if(high_resolution_clock::is_steady)
		return time_point_cast<milliseconds>(high_resolution_clock::now()).time_since_epoch().count();
	else
		return time_point_cast<milliseconds>(steady_clock::now()).time_since_epoch().count();
}
