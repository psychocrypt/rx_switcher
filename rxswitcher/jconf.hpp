#pragma once

#include "params.hpp"
#include "rxswitcher/misc/environment.hpp"

#include <stdlib.h>
#include <string>

class jconf
{
  public:
	static jconf* inst()
	{
		auto& env = xmrstak::environment::inst();
		if(env.pJconfConfig == nullptr)
		{
			std::unique_lock<std::mutex> lck(env.update);
			if(env.pJconfConfig == nullptr)
				env.pJconfConfig = new jconf;
		}
		return env.pJconfConfig;
	};

	bool parse_config(const char* sFilename, const char* sFilenamePools);

	struct pool_cfg
	{
		const char* sPoolAddr;
		const char* sWalletAddr;
		const char* sRigId;
		const char* sPasswd;
		bool nicehash;
		bool tls;
		const char* tls_fingerprint;
		size_t raw_weight;
		double weight;
	};

	size_t wt_max;
	size_t wt_min;

	uint64_t GetPoolCount();
	bool GetPoolConfig(size_t id, pool_cfg& cfg);

	enum slow_mem_cfg
	{
		always_use,
		no_mlck,
		print_warning,
		never_use,
		unknown_value
	};

	bool TlsSecureAlgos();

	uint64_t GetVerboseLevel();

	const char* GetOutputFile();

	uint64_t GetCallTimeout();
	uint64_t GetNetRetry();
	uint64_t GetGiveUpLimit();

	uint16_t GetHttpdPort();
	const char* GetHttpUsername();
	const char* GetHttpPassword();

	bool DaemonMode();

	bool PreferIpv4();

	inline bool HaveHardwareAes() { return bHaveAes; }

	static void cpuid(uint32_t eax, int32_t ecx, int32_t val[4]);

	slow_mem_cfg GetSlowMemSetting();

  private:
	jconf();

	bool parse_file(const char* sFilename, bool main_conf);

	bool check_cpu_features();
	struct opaque_private;
	opaque_private* prv;

	bool bHaveAes;
};
