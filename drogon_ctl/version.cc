/**
 *
 *  version.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "version.h"
#include <drogon/config.h>
#include <drogon/version.h>
#include <iostream>

using namespace drogon_ctl;
static const char banner[] =
    "     _                             \n"
    "  __| |_ __ ___   __ _  ___  _ __  \n"
    " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
    "| (_| | | | (_) | (_| | (_) | | | |\n"
    " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
    "                 |___/             \n";

void version::handleCommand(std::vector<std::string> &parameters)
{
    std::cout << banner << std::endl;
    std::cout << "A utility for drogon" << std::endl;
    std::cout << "Version:" << VERSION << std::endl;
    std::cout << "Git commit:" << VERSION_MD5 << std::endl;
    std::cout << "Compile config:" << COMPILATION_FLAGS << " " << INCLUDING_DIRS
              << std::endl;
}
