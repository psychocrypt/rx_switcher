/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  * Additional permission under GNU GPL version 3 section 7
  *
  * If you modify this Program, or any covered work, by linking or combining
  * it with OpenSSL (or a modified version of that library), containing parts
  * covered by the terms of OpenSSL License and SSLeay License, the licensors
  * of this Program grant you additional permission to convey the resulting work.
  *
  */

#include "executor.hpp"
#include "rxswitcher/jconf.hpp"
#include "rxswitcher/net/jpsock.hpp"
#include "rxswitcher/net/socket.hpp"

#include "rxswitcher/misc/console.hpp"
#include "rxswitcher/version.hpp"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <functional>
#include <string>
#include <thread>
#include <time.h>
#include <cstdlib>


#ifdef _WIN32
#define strncasecmp _strnicmp
#endif // _WIN32

executor::executor()
{
}

bool executor::get_live_pools(std::vector<jpsock*>& eval_pools)
{
	size_t limit = jconf::inst()->GetGiveUpLimit();
	size_t wait = jconf::inst()->GetNetRetry();

	if(limit == 0)
		limit = (-1); //No limit = limit of 2^64-1

	size_t pool_count = 0;
	size_t over_limit = 0;
	for(jpsock& pool : pools)
	{

		// Only eval live pools
		size_t num, dtime;
		pool.update_disconnects(num, dtime);

		if(dtime == 0 || (dtime >= wait && num <= limit))
			eval_pools.emplace_back(&pool);

		pool_count++;
		if(num > limit)
			over_limit++;
	}

	if(eval_pools.size() == 0)
	{
		if(epool_id != invalid_pool_id)
		{
			printer::inst()->print_msg(L0, "All pools are dead. Idling...");
		}

		if(over_limit == pool_count)
		{
			printer::inst()->print_msg(L0, "All pools are over give up limit. Exiting.");
			exit(0);
		}

		return false;
	}

	return true;
}

/*
 * This event is called by the timer and whenever something relevant happens.
 * The job here is to decide if we want to connect, disconnect, or switch jobs (or do nothing)
 */
void executor::eval_pool_choice()
{
	std::vector<jpsock*> eval_pools;
	eval_pools.reserve(pools.size());

	if(!get_live_pools(eval_pools))
		return;

	size_t running = 0;
	for(jpsock* pool : eval_pools)
	{
		if(pool->is_running())
			running++;
	}

	// Special case - if we are without a pool, connect to all find a live pool asap
	if(running == 0)
	{
		for(jpsock* pool : eval_pools)
		{
			if(pool->can_connect())
			{

				printer::inst()->print_msg(L1, "Fast-connecting to %s pool ...", pool->get_pool_addr());
				std::string error;
				if(!pool->connect(error))
					log_socket_error(pool, std::move(error));
			}
		}

		return;
	}

	std::sort(eval_pools.begin(), eval_pools.end(), [](jpsock* a, jpsock* b) { return b->get_pool_weight(true) < a->get_pool_weight(true); });
	jpsock* goal = eval_pools[0];

	if(goal->get_pool_id() != epool_id)
	{
		if(!goal->is_running() && goal->can_connect())
		{
			printer::inst()->print_msg(L1, "Connecting to %s pool ...", goal->get_pool_addr());

			std::string error;
			if(!goal->connect(error))
				log_socket_error(goal, std::move(error));
			return;
		}

		if(goal->is_logged_in())
		{
			pool_job oPoolJob;
			if(!goal->get_current_job(oPoolJob))
			{
				goal->disconnect();
				return;
			}

			size_t prev_pool_id = current_pool_id;
			current_pool_id = goal->get_pool_id();
			on_pool_have_job(current_pool_id, oPoolJob);

			jpsock* prev_pool = pick_pool_by_id(prev_pool_id);

			return;
		}
	}
	else
	{
		/* All is good - but check if we can do better */
		std::sort(eval_pools.begin(), eval_pools.end(), [](jpsock* a, jpsock* b) { return b->get_pool_weight(false) < a->get_pool_weight(false); });
		jpsock* goal2 = eval_pools[0];

		if(goal->get_pool_id() != goal2->get_pool_id())
		{
			if(!goal2->is_running() && goal2->can_connect())
			{
				printer::inst()->print_msg(L1, "Background-connect to %s pool ...", goal2->get_pool_addr());
				std::string error;
				if(!goal2->connect(error))
					log_socket_error(goal2, std::move(error));
				return;
			}
		}
	}


	for(jpsock& pool : pools)
	{
		if(goal->is_logged_in() && pool.is_logged_in() && pool.get_pool_id() != goal->get_pool_id())
			pool.disconnect(true);
	}
}

void executor::log_socket_error(jpsock* pool, std::string&& sError)
{
	std::string pool_name;
	pool_name.reserve(128);
	pool_name.append("[").append(pool->get_pool_addr()).append("] ");
	sError.insert(0, pool_name);

	vSocketLog.emplace_back(std::move(sError));
	printer::inst()->print_msg(L1, "SOCKET ERROR - %s", vSocketLog.back().msg.c_str());

	push_event(ex_event(EV_EVAL_POOL_CHOICE));
}


jpsock* executor::pick_pool_by_id(size_t pool_id)
{
	if(pool_id == invalid_pool_id)
		return nullptr;

	for(jpsock& pool : pools)
		if(pool.get_pool_id() == pool_id)
			return &pool;

	return nullptr;
}

void executor::on_sock_ready(size_t pool_id)
{
	jpsock* pool = pick_pool_by_id(pool_id);

	printer::inst()->print_msg(L1, "Pool %s connected. Logging in...", pool->get_pool_addr());

	if(!pool->cmd_login())
	{
		if(pool->have_call_error())
		{
			std::string str = "Login error: " + pool->get_call_error();
			log_socket_error(pool, std::move(str));
		}

		if(!pool->have_sock_error())
			pool->disconnect();
	}
}

void executor::on_sock_error(size_t pool_id, std::string&& sError, bool silent)
{
	jpsock* pool = pick_pool_by_id(pool_id);

	pool->disconnect();

	if(pool_id == current_pool_id)
		current_pool_id = invalid_pool_id;

	if(silent)
		return;

	log_socket_error(pool, std::move(sError));
}

void executor::on_pool_have_job(size_t pool_id, pool_job& oPoolJob)
{
	if(pool_id != current_pool_id)
		return;

	jpsock* pool = pick_pool_by_id(pool_id);

	printer::inst()->print_msg(L0, "Block height: %llu.", oPoolJob.iBlockHeight);

	if(epool_id != pool_id)
	{
		jpsock* prev_pool;
		if(epool_id != invalid_pool_id && (prev_pool = pick_pool_by_id(epool_id)) != nullptr)
		{
			printer::inst()->print_msg(L2, "Pool switched.");
		}
		else
			printer::inst()->print_msg(L2, "Pool logged in.");

		epool_id = pool_id;
	}
	else
		printer::inst()->print_msg(L3, "New block detected.");
}

#ifndef _WIN32

#include <signal.h>
void disable_sigpipe()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if(sigaction(SIGPIPE, &sa, 0) == -1)
		printer::inst()->print_msg(L1, "ERROR: Call to sigaction failed!");
}

#else
inline void disable_sigpipe()
{
}
#endif


#ifndef _WIN32
void stop_xmrstak()
{
	printer::inst()->print_msg(L0, "Stop all instances of xmr-stak");
	std::system("killall -9 xmr-stak");
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	std::system("xmr-stak should be stopped now.");
}

void start_xmrstakrx()
{
	printer::inst()->print_msg(L0, "start xmr-stak-rx");
	execl("xmr-stak-rx", "xmr-stak-rx", NULL);
}
#else
#include <windows.h>
#include <cstdio>
#include <tchar.h>
#include <psapi.h>
#include <process.h>

void checkPID( DWORD processID )
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ | PROCESS_TERMINATE,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod),
             &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName,
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }

	std::string pName = szProcessName;
	std::size_t found = pName.find("xmr-stak");
    if(found != std::string::npos)
	{
	    _tprintf( TEXT("kill %s  (PID: %u)\n"), szProcessName, processID );
		uint32_t ret;
		TerminateProcess(hProcess, ret);
	}

    // Release the handle to the process.

    CloseHandle( hProcess );
}

void stop_xmrstak()
{
	DWORD aProcesses[1024], cbNeeded, cProcesses;

	if (!EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		printer::inst()->print_msg(L0, "ERROR: can not kill xmr-stak. Plese stop xmr-stak and start xmr-stak-rx!!");
		return;
	}

	cProcesses = cbNeeded / sizeof(DWORD);

	for (uint32_t i = 0; i < cProcesses; i++)
	{
		if(aProcesses[i] != 0)
		{
			checkPID(aProcesses[i]);
		}
	}
}

void start_xmrstakrx()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	printer::inst()->print_msg(L0, "start xmr-stak-rx");
	SHELLEXECUTEINFO shExecInfo = {0};
	shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shExecInfo.hwnd = NULL;
	shExecInfo.lpVerb = NULL;
	shExecInfo.lpFile = "xmr-stak-rx";
	shExecInfo.lpParameters = NULL;
	shExecInfo.lpDirectory = NULL;
	shExecInfo.nShow = SW_SHOW;
	shExecInfo.hInstApp = NULL;

	ShellExecuteEx(&shExecInfo);
	printer::inst()->print_msg(L0, "You can close this window, xmr-stak-rx is now running.");
}
#endif

void executor::ex_main()
{
	disable_sigpipe();

	assert(1000 % iTickTime == 0);

	size_t pc = jconf::inst()->GetPoolCount();
	bool already_have_cli_pool = false;
	size_t i = 0;
	for(; i < pc; i++)
	{
		jconf::pool_cfg cfg;
		jconf::inst()->GetPoolConfig(i, cfg);
#ifdef CONF_NO_TLS
		if(cfg.tls)
		{
			printer::inst()->print_msg(L1, "ERROR: No miner was compiled without TLS support.");
			win_exit();
		}
#endif
		pools.emplace_back(i, cfg.sPoolAddr, cfg.sWalletAddr, cfg.sRigId, cfg.sPasswd, cfg.weight, cfg.tls, cfg.tls_fingerprint, cfg.nicehash);
	}


	ex_event ev;

	eval_pool_choice();

	constexpr uint64_t fork_height = 1978433;

	bool forked = false;

	size_t cnt = 0;
	while(!forked)
	{
		ev = oEventQ.pop();
		switch(ev.iName)
		{
		case EV_SOCK_READY:
			on_sock_ready(ev.iPoolId);
			break;

		case EV_SOCK_ERROR:
			on_sock_error(ev.iPoolId, std::move(ev.oSocketError.sSocketError), ev.oSocketError.silent);
			break;

		case EV_POOL_HAVE_JOB:
			if(ev.oPoolJob.iBlockHeight >= fork_height)
			{
				printer::inst()->print_msg(L0, "Monero forked to randomx.");
				forked = true;
				break;
			}
			on_pool_have_job(ev.iPoolId, ev.oPoolJob);
			break;

		case EV_EVAL_POOL_CHOICE:
			eval_pool_choice();
			break;

		case EV_INVALID_VAL:
		default:
			assert(false);
			break;
		}
	}
	if(forked)
	{
		stop_xmrstak();
		start_xmrstakrx();
	}
}

inline const char* hps_format(double h, char* buf, size_t l)
{
	if(std::isnormal(h) || h == 0.0)
	{
		snprintf(buf, l, " %6.1f", h);
		return buf;
	}
	else
		return "   (na)";
}
