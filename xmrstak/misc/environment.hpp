#pragma once

#include <mutex>

class printer;
class jconf;
class executor;
struct randomX_global_ctx;

namespace xmrstak
{

struct globalStates;
struct params;
struct motd;

struct environment
{
	static inline environment& inst(environment* init = nullptr)
	{
		static environment* env = nullptr;

		if(env == nullptr)
		{
			if(init == nullptr)
			{
				env = new environment;
			}
			else
				env = init;
		}

		return *env;
	}

	environment()
	{
	}

	printer* pPrinter = nullptr;
	jconf* pJconfConfig = nullptr;
	executor* pExecutor = nullptr;
	params* pParams = nullptr;

	std::mutex update;
};

} // namespace xmrstak
