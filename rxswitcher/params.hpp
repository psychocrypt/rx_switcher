#pragma once

#include "rxswitcher/misc/environment.hpp"

#include <vector>
#include <cstdint>

namespace xmrstak
{

struct params
{

	static inline params& inst()
	{
		auto& env = environment::inst();
		if(env.pParams == nullptr)
		{
			std::unique_lock<std::mutex> lck(env.update);
			if(env.pParams == nullptr)
				env.pParams = new params;
		}
		return *env.pParams;
	}


	std::string configFile;
	std::string configFilePools;

	std::string outputFile;


	params() :
		configFile("config.txt"),
		configFilePools("pools.txt")
	{
	}
};

} // namespace xmrstak
