// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/fatal/fatal_p.h"

namespace Service {
namespace Fatal {

Fatal_P::Fatal_P(std::shared_ptr<Module> module)
    : Module::Interface(std::move(module), "fatal:p") {}

} // namespace Fatal
} // namespace Service
