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

#include "xmrstak/jconf.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/misc/executor.hpp"
#include "xmrstak/params.hpp"
#include "xmrstak/version.hpp"


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <fstream>

#ifndef CONF_NO_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#include <windows.h>
#endif // _WIN32

int do_benchmark(int block_version, int wait_sec, int work_sec);

void help()
{
	using namespace std;
	using namespace xmrstak;

	cout << "Version: " << get_version_str_short() << endl;
	cout << "Brought to by fireice_uk and psychocrypt under GPLv3." << endl;
}

bool file_exist(const std::string filename)
{
	std::ifstream fstream(filename);
	return fstream.good();
}

int main(int argc, char* argv[])
{
#ifndef CONF_NO_TLS
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_digests();
#endif

	srand(time(0));

	using namespace xmrstak;

	bool hasConfigFile = file_exist(params::inst().configFile);
	bool hasPoolConfig = file_exist(params::inst().configFilePools);

	// check if we need a guided start
	if(!hasConfigFile)
	{
		printer::inst()->print_msg(L0, "File 'config.txt' is missing.");
		return 1;
	}

	if(!hasPoolConfig)
	{
		printer::inst()->print_msg(L0, "File 'pools.txt' is missing.");
		return 1;
	}

	if(!jconf::inst()->parse_config(params::inst().configFile.c_str(), params::inst().configFilePools.c_str()))
	{
		win_exit();
		return 1;
	}

	if(strlen(jconf::inst()->GetOutputFile()) != 0)
		printer::inst()->open_logfile(jconf::inst()->GetOutputFile());

	executor::inst()->ex_start();

	return 0;
}
