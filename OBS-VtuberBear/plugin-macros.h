// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#ifndef PLUGINNAME_H
#define PLUGINNAME_H

#define PLUGIN_NAME "OBS-VtuberBear"
#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "1.0.0"
#endif

// ReSharper disable once CppInconsistentNaming
#define blog(level, msg, ...) blog(level, "[" PLUGIN_NAME "] " msg, ##__VA_ARGS__)

#endif // PLUGINNAME_H
