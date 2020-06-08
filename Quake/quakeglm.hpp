#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_macros.hpp"

// TODO VR: (P1) fix glm include paths
#include <common.hpp>
#include <trigonometric.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "quakeglm_qvec3.hpp"

using qfloat = float;

[[nodiscard]] constexpr inline qfloat operator"" _qf(long double x) noexcept
{
    return qfloat(x);
}
