// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once

// From cppreference on std::visit. Helper function that allows std::visit to call a different function depending on type in the variant
template<class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

template<typename>
constexpr bool ALWAYS_FALSE = false;
