#pragma once


#include "thdq.hpp"
#include "xmrstak/misc/environment.hpp"
#include "xmrstak/net/msgstruct.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <list>
#include <vector>

class jpsock;

namespace xmrstak
{
namespace cpu
{
class minethd;

} // namespace cpu
} // namespace xmrstak

class executor
{
  public:
	static executor* inst()
	{
		auto& env = xmrstak::environment::inst();
		if(env.pExecutor == nullptr)
		{
			std::unique_lock<std::mutex> lck(env.update);
			if(env.pExecutor == nullptr)
				env.pExecutor = new executor;
		}
		return env.pExecutor;
	};

	void ex_start() { ex_main(); }

	void get_http_report(ex_event_name ev_id, std::string& data);

	inline void push_event(ex_event&& ev) { oEventQ.push(std::move(ev)); }
	void push_timed_event(ex_event&& ev, size_t sec);

  private:
	struct timed_event
	{
		ex_event event;
		size_t ticks_left;

		timed_event(ex_event&& ev, size_t ticks) :
			event(std::move(ev)),
			ticks_left(ticks) {}
	};

	// In milliseconds, has to divide a second (1000ms) into an integer number
	constexpr static size_t iTickTime = 500;

	std::list<timed_event> lTimedEvents;
	std::mutex timed_event_mutex;
	thdq<ex_event> oEventQ;

	static constexpr size_t invalid_pool_id = -1;
	size_t current_pool_id = invalid_pool_id;

	std::list<jpsock> pools;
	size_t epool_id = invalid_pool_id;

	jpsock* pick_pool_by_id(size_t pool_id);

	executor();

	void ex_main();

	void ex_clock_thd();
	void pool_connect(jpsock* pool);

	struct sck_error_log
	{
		std::chrono::system_clock::time_point time;
		std::string msg;

		sck_error_log(std::string&& err) :
			msg(std::move(err))
		{
			time = std::chrono::system_clock::now();
		}
	};
	std::vector<sck_error_log> vSocketLog;

	// Element zero is always the success element.
	// Keep in mind that this is a tally and not a log like above
	struct result_tally
	{
		std::chrono::system_clock::time_point time;
		std::string msg;
		size_t count;

		result_tally() :
			msg("[OK]"),
			count(0)
		{
			time = std::chrono::system_clock::now();
		}

		result_tally(std::string&& err) :
			msg(std::move(err)),
			count(1)
		{
			time = std::chrono::system_clock::now();
		}

		void increment()
		{
			count++;
			time = std::chrono::system_clock::now();
		}

		bool compare(std::string& err)
		{
			if(msg == err)
				return true;
			else
				return false;
		}
	};

	std::chrono::system_clock::time_point tPoolConnTime;

	void log_socket_error(jpsock* pool, std::string&& sError);

	void on_sock_ready(size_t pool_id);
	void on_sock_error(size_t pool_id, std::string&& sError, bool silent);
	void on_pool_have_job(size_t pool_id, pool_job& oPoolJob);
	void connect_to_pools(std::list<jpsock*>& eval_pools);
	bool get_live_pools(std::vector<jpsock*>& eval_pools);
	void eval_pool_choice();

	inline size_t sec_to_ticks(size_t sec) { return sec * (1000 / iTickTime); }
};
