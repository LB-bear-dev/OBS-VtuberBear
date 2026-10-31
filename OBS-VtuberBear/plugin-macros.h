/*
OBS-VtuberBear
Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#ifndef PLUGINNAME_H
#define PLUGINNAME_H

#define PLUGIN_NAME "OBS-VtuberBear"
#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "1.0.0"
#endif

// Wraps OBS's blog() under its own name so every call gets the plugin prefix.
// ReSharper disable once CppInconsistentNaming
#define blog(level, msg, ...) blog(level, "[" PLUGIN_NAME "] " msg, ##__VA_ARGS__)

#endif // PLUGINNAME_H
