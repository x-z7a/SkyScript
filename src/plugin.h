//
//
//    Copyright (C) 2026  Zodiac1214
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
//    USA
//

#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <numbers>
#include <memory>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <array>
#include <thread>

#include "XPLMDataAccess.h"
#include "XPLMScenery.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMMenus.h"

#include "log_msg.h"
#include "http_get.h"

#include "manager.h"

extern std::string xp_dir;
extern std::string plugin_dir;
extern std::string output_dir;
