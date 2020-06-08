Con_Printf("has handtouch\n");


Con_Printf("c %d>%d %d>%d %d>%d %d<%d %d<%d %d<%d\n", handposmin[0],
    target->v.absmax[0], handposmin[1], target->v.absmax[1], handposmin[2],
    target->v.absmax[2], handposmax[0], target->v.absmin[0], handposmax[1],
    target->v.absmin[1], handposmax[2], target->v.absmin[2]);



// TODO VR: this kinda works in E1M1 if put inside FlyMove
// TODO VR: test, what is this one for? How does it differ from world.cpp?

auto doHandTouch = [&](vec3_t handpos, vec3_t handrot, int type) {
    vec3_t fwd, right, up, end;
    AngleVectors(handrot, fwd, right, up);
    fwd[0] *= 1.f;
    fwd[1] *= 1.f;
    fwd[2] *= 1.f;

    VectorCopy(handpos, end);
    VectorAdd(end, fwd, end);

    vec3_t mins{-2, -2, -2};
    vec3_t maxs{2, 2, 2};
    // trace_t trace = SV_Move(handpos, mins, maxs, end, type, ent);
    trace_t trace =
        SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

    // vec3_t impact;
    // trace_t trace = TraceLine(cl.handpos[1], end, impact);
    // SV_ClipMoveToEntity(sv.edicts, start, mins, maxs, end);

    /* if(trace.allsolid)
     { // entity is trapped in another solid
         // Con_Printf("all solid\n");
         return;
     }


     if(trace.fraction == 1)
     { // moved the entire distance
         // Con_Printf("entire dist\n");
         return;
     }*/

    if(!trace.ent)
    {
        // Con_Printf("no ent\n");
        return;
    }

    vec3_t aMin, aMax, bMin, bMax;
    VectorAdd(trace.ent->v.origin, trace.ent->v.mins, aMin);
    VectorAdd(trace.ent->v.origin, trace.ent->v.maxs, aMax);
    VectorAdd(handpos, mins, bMin);
    VectorAdd(handpos, maxs, bMax);

    if(quake::util::boxIntersection(aMin, aMax, bMin, bMax))
    {
        SV_Impact(ent, trace.ent, &entvars_t::handtouch);
    }
};

doHandTouch(ent->v.handpos, ent->v.handrot, MOVE_NORMAL);
doHandTouch(ent->v.offhandpos, ent->v.offhandrot, MOVE_NORMAL);



void VR_Move(usercmd_t* cmd)
{
    if(!vr_enabled.value)
    {
        return;
    }

    // TODO VR: repetition of ofs calculation
    // TODO VR: adj unused? could be used to find position of muzzle
    //
    /*
    vec3_t adj;
    _VectorCopy(cl.handpos[1], adj);

    vec3_t ofs = {vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2].value +
            vr_gunmodely.value};

    vec3_t fwd2, right, up;
    AngleVectors(cl.handrot[1], fwd2, right, up);
    fwd2[0] *= vr_gunmodelscale.value * ofs[2];
    fwd2[1] *= vr_gunmodelscale.value * ofs[2];
    fwd2[2] *= vr_gunmodelscale.value * ofs[2];
    VectorAdd(adj, fwd2, adj);
    */

    // TODO VR: not needed anymore, changing QC - what to do?
    //
    // vec3_t adjhandpos;
    // VectorCopy(cl.handpos[1], adjhandpos);
    // adjhandpos[2] -= vr_projectilespawn_z_offset.value;



    class ParticleBuffer
    {
    private:
        // TODO VR: use raw buffer for more speed
        std::vector<particle_t> _particles;
        std::size_t _maxParticles;

    public:
        void initialize(std::size_t maxParticles)
        {
            _maxParticles = maxParticles;
            _particles.reserve(maxParticles);
        }

        void cleanup()
        {
            _particles.erase(
                std::remove_if(_particles.begin(), _particles.end(),
                    [](const particle_t& p) { return cl.time >= p.die; }),
                _particles.end());
        }

        [[nodiscard]] particle_t& create()
        {
            return _particles.emplace_back();
        }

        template <typename F>
        void forActive(F&& f)
        {
            for(auto& p : _particles)
            {
                f(p);
            }
        }

        [[nodiscard]] bool reachedMax() const noexcept
        {
            return _particles.size() == _maxParticles;
        }

        void clear()
        {
            _particles.clear();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return _particles.empty();
        }
    };



    class ParticleBuffer
    {
    private:
        particle_t* _particles;
        particle_t* _aliveEnd;
        particle_t* _end;

    public:
        void initialize(const std::size_t maxParticles) noexcept
        {
            _particles = (particle_t*)Hunk_AllocName(
                maxParticles * sizeof(particle_t), "particles");
            _aliveEnd = _particles;
            _end = _particles + maxParticles;
        }

        void cleanup() noexcept
        {
            const auto it = std::remove_if(_particles, _aliveEnd,
                [](const particle_t& p) { return cl.time >= p.die; });

#if 0
        for(auto p = it; p != _aliveEnd; ++p)
        {
            p->~particle_t();
        }
#endif

            _aliveEnd = it;
        }

        [[nodiscard]] particle_t& create() noexcept
        {
#if 0
        const auto p = _aliveEnd++;
        new(p) particle_t;
        return *p;
#else
            return *_aliveEnd++;
#endif
        }

        template <typename F>
        void forActive(F&& f) noexcept
        {
            for(auto p = _particles; p != _aliveEnd; ++p)
            {
                f(*p);
            }
        }

        [[nodiscard]] bool reachedMax() const noexcept
        {
            return _aliveEnd == _end;
        }

        void clear() noexcept
        {
#if 0
        for(auto p = _particles; p != _aliveEnd; ++p)
        {
            p->~particle_t();
        }
#endif

            _aliveEnd = _particles;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return _aliveEnd == _particles;
        }
    };


    // s = ftos(xdmg);
    // sprint (self, "OFFHANDDMG: ");
    // sprint (self, s);
    // sprint (self, "\n");


    static void vec3lerp(vec3_t out, vec3_t start, vec3_t end, double f)
    {
        out[0] = lerp(start[0], end[0], f);
        out[1] = lerp(start[1], end[1], f);
        out[2] = lerp(start[2], end[2], f);
    }



// non-working smoothed hand rotation
#if 0
                if (i == 0) continue;

                const auto ox = cl.handrot[i][PITCH];
                const auto oy = cl.handrot[i][YAW];
                const auto oz = cl.handrot[i][ROLL];

                const auto tx = handrottemp[PITCH];
                const auto ty = handrottemp[YAW];
                const auto tz = handrottemp[ROLL];

                const glm::vec3 orig{ ox, oy, oz };

                // glm::fquat q{glm::radians(orig)};
                // q = glm::mix(glm::normalize(q), glm::normalize(glm::fquat(glm::radians(glm::vec3(tx, ty, tz)))), 0.05f);

                glm::mat3 m(toVec3(mat[0]), toVec3(mat[1]), toVec3(mat[2]));
                glm::fquat q(m);

                const glm::vec3 res{ glm::degrees(glm::eulerAngles(glm::normalize(q))) };

                const auto nx = res[PITCH];
                const auto ny = res[YAW];
                const auto nz = res[ROLL];

                auto fx = nx;
                auto fy = ny;
                auto fz = nz;

                if (oy > 90.f)
                {
                    fx -= 180.f;
                    fy -= 180.f;
                    fy *= -1.f;
                    fz += 180.f;

                    if (ox > 0.f)
                    {
                        fx += 360.f;
                    }
                }

                if (false)
                {
                    Con_Printf("%d %d %d | %d %d %d | %d %d %d\n",
                        (int)ox, (int)oy, (int)oz,
                        (int)nx, (int)ny, (int)nz,
                        (int)fx, (int)fy, (int)fz
                    );

                    quake::util::debugPrintSeparated(" ", (int)ox, (int)oy, (int)oz);
                    quake::util::debugPrint(" | ");
                    quake::util::debugPrintSeparated(" ", (int)nx, (int)ny, (int)nz);
                    quake::util::debugPrint(" | ");
                    quake::util::debugPrintSeparated(" ", (int)fx, (int)fy, (int)fz);
                    quake::util::debugPrint("\n");
                }

                handrottemp[0] = fx;
                handrottemp[1] = fy;
                handrottemp[2] = fz;
#endif



    case VrAimMode::e_CONTROLLER:
    {
        cl.viewangles[PITCH] = orientation[PITCH];
        cl.viewangles[YAW] = orientation[YAW];

        vec3_t mat[3];
        vec3_t matTmp[3];

        vec3_t rotOfs = {vr_gunangle.value, vr_gunyaw.value, 0};

        vec3_t gunMatPitch[3];
        CreateRotMat(0, rotOfs[0], gunMatPitch); // pitch

        vec3_t gunMatYaw[3];
        CreateRotMat(1, rotOfs[1], gunMatYaw); // yaw

        vec3_t gunMatRoll[3];
        CreateRotMat(2, rotOfs[2], gunMatRoll); // roll

        for(int i = 0; i < 2; i++)
        {
            RotMatFromAngleVector(controllers[i].orientation, mat);

            R_ConcatRotations(gunMatRoll, mat, matTmp);
            for(int j = 0; j < 3; ++j)
            {
                VectorCopy(matTmp[j], mat[j]);
            }

            R_ConcatRotations(gunMatPitch, mat, matTmp);
            for(int j = 0; j < 3; ++j)
            {
                VectorCopy(matTmp[j], mat[j]);
            }

            R_ConcatRotations(gunMatYaw, mat, matTmp);
            for(int j = 0; j < 3; ++j)
            {
                VectorCopy(matTmp[j], mat[j]);
            }

            vec3_t handrottemp;
            AngleVectorFromRotMat(mat, handrottemp);
            VectorCopy(handrottemp, cl.handrot[i]);
        }

        if(cl.viewent.model)
        {
            auto* hdr = (aliashdr_t*)Mod_Extradata(cl.viewent.model);
            Mod_Weapon(cl.viewent.model->name, hdr);

            // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
            // testhdr->flags |= EF_GRENADE;
            // VectorScale(testhdr->scale_origin, 0.5f,
            // testhdr->scale_origin);

            // BModels cannot be scaled, doesnt work
            // qmodel_t* test = Mod_ForName("maps/b_shell1.bsp", true);
            // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
            // VectorScale(testhdr->scale_origin, 0.5f,
            // testhdr->scale_origin);
        }

        if(cl.offhand_viewent.model)
        {
            // aliashdr_t* hdr =
            // (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model);
            // Mod_Weapon(cl.offhand_viewent.model->name, hdr);

            ApplyMod_Weapon(VR_GetOffHandFistCvarEntry(),
                (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model));
        }

        SetHandPos(0, player);
        SetHandPos(1, player);

        Con_Printf("%d %d %d | ", (int)cl.handpos[1][0], (int)cl.handpos[1][1],
            (int)cl.handpos[1][2]);

        Con_Printf("%d %d %d\n", (int)cl.handrot[1][0], (int)cl.handrot[1][1],
            (int)cl.handrot[1][2]);

        auto up = glm::vec3{0, 0, 1};

        auto m =
            glm::lookAtRH(toVec3(cl.handpos[1]), toVec3(cl.handpos[0]), up);

        vr::HmdMatrix34_t mm;
        for(int x = 0; x < 3; ++x)
        {
            for(int y = 0; y < 4; ++y)
            {
                mm.m[x][y] = m[x][y];
            }
        }

        auto ypr = QuatToYawPitchRoll(Matrix34ToQuaternion(mm));

        const auto nx = ypr[PITCH];
        const auto ny = ypr[YAW];
        const auto nz = ypr[ROLL];

        auto fy = nx;
        auto fx = ny;
        auto fz = nz;

        if(fy > 90.f)
        {
            fx -= 180.f;
            fy -= 180.f;
            fy *= -1.f;
            fz += 180.f;

            if(fx > 0.f)
            {
                fx += 360.f;
            }
        }


        cl.handrot[1][PITCH] = fx;
        cl.handrot[1][YAW] = fy;
        cl.handrot[1][ROLL] = fz;

        // TODO VR: interpolate based on weapon weight?
        VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
        // TODO VR: what sets the shooting origin?

        // TODO VR: teleportation stuff
        VR_DoTeleportation();
        break;
    }
}
cl.viewangles[ROLL] = orientation[ROLL];



// -----------------------------------------------------------------------
// VR: Detect & resolve hand collisions against the world or entities.

// Positions.
auto currHandPos = toVec3(cl.handpos[index]);
const auto desiredHandPos = toVec3(finalPre);

// Size of hand hitboxes.
vec3_t mins{-1.f, -1.f, -1.f};
vec3_t maxs{1.f, 1.f, 1.f};

// Start around the upper torso, not actual center of the player.
vec3_t adjPlayerOrigin;
VR_GetAdjustedPlayerOrigin(adjPlayerOrigin, player);

// TODO VR:
const float gunLength = 13;

// 1. If hands get too far, bring them closer to the player.
constexpr auto maxHandPlayerDiff = 100.f;
const auto handPlayerDiff = currHandPos - toVec3(adjPlayerOrigin);
if(glm::length(handPlayerDiff) > maxHandPlayerDiff)
{
    const auto dir = glm::normalize(handPlayerDiff);
    currHandPos[0] = dir[0] * maxHandPlayerDiff;
    currHandPos[1] = dir[1] * maxHandPlayerDiff;
    currHandPos[2] = dir[2] * maxHandPlayerDiff;
}

// 2. Trace the current hand position against the desired one.
// `SV_Move` detects entities as well, not just geometry.
const trace_t handTrace =
    SV_Move(cl.handpos[index], mins, maxs, finalPre, MOVE_NORMAL, sv_player);



vec3_t forward, right, up;
AngleVectors(cl.handrot[1], forward, right, up);



// Trace from upper torso to desired final location. `SV_Move` detects
// entities as well, not just geometry.
const trace_t trace =
    SV_Move(adjPlayerOrigin, mins, maxs, finalPre, MOVE_NORMAL, sv_player);

vec3_t adjFinalPre;
VectorCopy(finalPre, adjFinalPre);

adjFinalPre[0] += forward[0] * gunLength;
adjFinalPre[1] += forward[1] * gunLength;
adjFinalPre[2] += forward[2] * gunLength;

// TODO VR:
const trace_t gunTrace =
    SV_Move(cl.handpos[1], mins, maxs, adjFinalPre, MOVE_NORMAL, sv_player);

// Origin of the trace.
const auto orig = quake::util::toVec3(adjPlayerOrigin);

// Final position before collision resolution.
const auto pre = quake::util::toVec3(finalPre);

// Final position after full collision resolution.
const auto crop = quake::util::toVec3(trace.endpos);

// TODO VR:
auto gunCrop = quake::util::toVec3(gunTrace.endpos) -=
    toVec3(forward) * gunLength;

// Compute final collision resolution position, starting from the desired
// position and resolving only against the collision plane's normal vector.
VectorCopy(finalPre, finalVec);
if(trace.fraction < 1.f)
{
    VectorCopy(finalPre, finalVec);
    for(int i = 0; i < 3; ++i)
    {
        if(trace.plane.normal[i] != 0)
        {
            finalVec[i] = crop[i];
        }
    }
}



// Con_Printf("newup: %.2f, %.2f, %.2f\n", newUp[0], newUp[1], newUp[2]);
// Con_Printf("mixup: %.2f, %.2f, %.2f\n\n", mixUp[0], mixUp[1], mixUp[2]);

// const auto oldDir = getDirectionVectorFromPitchYawRoll(cl.aimangles);
// const auto newDir = getDirectionVectorFromPitchYawRoll(cl.handrot[1]);
// Con_Printf("dir: %.2f, %.2f, %.2f\n", newDir[0], newDir[1], newDir[2]);
// const auto mixDir = glm::slerp(oldDir, newDir, 0.05f);

// handpos
// const auto pitch = glm::radians(cl.handrot[1][PITCH]);
// const auto yaw = glm::radians(cl.handrot[1][YAW]);
// const auto roll = glm::radians(cl.handrot[1][ROLL]);

//  const glm::vec3 oldAngles{oldx, oldy, oldz};

// auto dx = sin(yaw);
// auto dy = -(sin(pitch)*cos(yaw));
// auto dz = -(cos(pitch)*cos(yaw));

//    auto dx = std::cos(yaw) * std::cos(pitch);
//    auto dy = std::sin(yaw) * std::cos(pitch);
//    auto dz = std::sin(pitch);

// const auto [fwd, right, up] = getGlmAngledVectors(newDir);
// Con_Printf("fwd: %.2f, %.2f, %.2f\n", fwd[0], fwd[1], fwd[2]);



const auto mySlerp = [](auto start, auto end, float percent) {
    // Dot product - the cosine of the angle between 2 vectors.
    float dot = glm::dot(start, end);
    // Clamp it to be in the range of Acos()
    // This may be unnecessary, but floating point
    // precision can be a fickle mistress.
    dot = std::clamp(dot, -1.0f, 1.0f);
    // Acos(dot) returns the angle between start and end,
    // And multiplying that by percent returns the angle between
    // start and the final result.
    float theta = acos(dot) * percent;
    auto RelativeVec = glm::normalize(end - start * dot);
    // Orthonormal basis
    // The final result.
    return ((start * cos(theta)) + (RelativeVec * sin(theta)));
};

const auto [oldFwd, oldRight, oldUp] = getGlmAngledVectors(cl.prevhandrot[1]);
const auto [newFwd, newRight, newUp] = getGlmAngledVectors(cl.handrot[1]);

const auto nOldFwd = glm::normalize(oldFwd);
const auto nOldUp = glm::normalize(oldUp);
const auto nNewFwd = glm::normalize(newFwd);
const auto nNewUp = glm::normalize(newUp);

const float frametime = cl.time - cl.oldtime;
const auto factor = 0.1f + ((vr_2h_aim_transition / 10.f) * 2.f);
const auto ftw = (factor * frametime) * 100.f;

const auto slerpFwd = glm::slerp(nOldFwd, nNewFwd, ftw);
const auto slerpUp = glm::slerp(nOldUp, nNewUp, ftw);

const auto anyNan = [](const glm::vec3& v) {
    return std::isnan(v[0]) || std::isnan(v[1]) || std::isnan(v[2]);
};

const auto mixFwd = anyNan(slerpFwd) ? nNewFwd : slerpFwd;
const auto mixUp = anyNan(slerpUp) ? nNewUp : slerpUp;

// TODO VR: should be mixUp, this causes issues  when meleeing
const auto [p, y, r] = pitchYawRollFromDirectionVector(mixUp, mixFwd);
// Con_Printf("pyr: %.2f, %.2f, %.2f\n", p, y, r);

const auto fixRoll = cl.handrot[1][ROLL] + 360.f;

Con_Printf("%.2f, %.2f, %.2f\n", cl.prevhandrot[1][ROLL] + 360.f,
    LerpDegrees(cl.prevhandrot[1][ROLL] + 360.f, fixRoll, ftw), fixRoll);

cl.handrot[1][PITCH] = p;
cl.handrot[1][YAW] = y;
cl.handrot[1][ROLL] =
    r; // LerpDegrees(cl.prevhandrot[1][ROLL] + 360.f, fixRoll, ftw) - 360.f;

VectorCopy(cl.handrot[1], cl.prevhandrot[1]);

// TODO VR: interpolate based on weapon weight?
VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
// TODO VR: what sets the shooting origin?

// TODO VR: teleportation stuff
VR_DoTeleportation();



float LerpDegrees(float a, float b,
    float lerpFactor) // Lerps from angle a to b (both between 0.f and 360.f),
                      // taking the shortest path
{
    float result;
    float diff = b - a;
    if(diff < -180.f)
    {
        // lerp upwards past 360
        b += 360.f;
        result = lerp(a, b, lerpFactor);
        if(result >= 360.f)
        {
            result -= 360.f;
        }
    }
    else if(diff > 180.f)
    {
        // lerp downwards past 0
        b -= 360.f;
        result = lerp(a, b, lerpFactor);
        if(result < 0.f)
        {
            result += 360.f;
        }
    }
    else
    {
        // straight lerp
        result = lerp(a, b, lerpFactor);
    }

    return result;
}

template <typename... Ts>
[[nodiscard]] constexpr auto makeAdjustedMenuLabels(const Ts&... labels)
{
    constexpr auto maxLen = 26;

    assert(((strlen(labels) <= maxLen) && ...));
    return std::array{(std::string(maxLen - strlen(labels), ' ') + labels)...};
}



// TODO VR:
if(false && !(clip & 1))
{
    // floor not detected
    // Con_Printf("floor not detected\n");

    constexpr glm::vec3 zOff{0.f, 0.f, 1.f};

    const auto traceZLine = [&](const glm::vec3& point) {
        // TODO VR: find all these traces with two zeros and create function
        return SV_Move(
            point + zOff, vec3_zero, vec3_zero, point - zOff, MOVE_NORMAL, ent);
    };

    const auto doTrace = [&](const glm::vec3& point) {
        const trace_t t = traceZLine(point);

        if(!quake::util::hitSomething(t) || t.plane.normal[2] <= 0.7)
        {
            return false;
        }

        if(t.ent->v.solid == SOLID_BSP)
        {
            ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
            ent->v.groundentity = EDICT_TO_PROG(t.ent);
        }

        return true;
    };

    const auto diff = ent->v.absmax - ent->v.absmin;
    const glm::vec3 offX{diff[0], 0.f, 0.f};
    const glm::vec3 offY{0.f, diff[1], 0.f};
    const glm::vec3 offXY{diff[0], diff[1], 0.f};
    const glm::vec3 offXYHalf = offXY / 2.f;

    const bool anyGroundHit =             //
        doTrace(ent->v.absmin) ||         //
        doTrace(ent->v.absmin + offX) ||  //
        doTrace(ent->v.absmin + offY) ||  //
        doTrace(ent->v.absmin + offXY) || //
        doTrace(ent->v.absmin + offXYHalf);

    (void)anyGroundHit;
}



if(false)
{
    cl.handvel[index] =
        (cl.handpos[index] - lastPlayerTranslation) - oldHandpos;
    const auto [vFwd, vRight, vUp] = getAngledVectors(cl.handrot[index]);

    cl.handvel[index] += glm::cross(controllers[index].a_velocity, vUp * 0.1f);
}

if(false)
{
    cl.handvel[index] = Vec3RotateZ(cl.handvel[index], vrYaw * M_PI_DIV_180);
}



const auto maxIdxBetween = [](const auto& a, const auto& b) {
    return a > b ? 0 : 1;
};

const auto mapRange = [](const auto input, const auto inputMin,
                          const auto inputMax, const auto outputMin,
                          const auto outputMax) {
    const double slope = 1.0 * (outputMax - outputMin) / (inputMax - inputMin);

    return outputMin + slope * (input - inputMin);
};



// const auto lenH0 = glm::length(headHandDiffs[0]);
// const auto lenH1 = glm::length(headHandDiffs[1]);
//
// const auto longestIdx = maxIdxBetween(lenH0, lenH1);
// const auto shortestIdx = longestIdx == 0 ? 1 : 0;
//
// const auto longestHeadHandDiff = headHandDirs[longestIdx];
// const auto shortestHeadHandDiff = headHandDirs[shortestIdx];

// const auto maxDotIdx = maxIdxBetween(dotHeadHand[0], dotHeadHand[1]);
// const auto minDotIdx = maxDotIdx == 0 ? 1 : 0;
//
// const auto& maxDot = headHandDirs[maxDotIdx];
// const auto& minDot = headHandDirs[minDotIdx];

// const auto lenDiff = lenH1 - lenH0;
// const auto dotDiff = dotHeadHand[1] - dotHeadHand[0];

// const auto lengthCoefficient = std::clamp(
//     (float)mapRange(lenDiff, -1.5f, 1.5f, 0.45f, 0.65f), 0.0f, 1.f);

// const float scaling = mapRange(lenDiff, -1.0f, 20.f, 0.2f, 0.8f);

// Con_Printf("%.2f; %.2f\n", lenDiff, glm::length(mixHandDir));

// const float coefficient = mapRange(blendedDot, -1.f, 1.f, 0.f, 1.f);



// const auto turnDir = [&] {
//     glm::vec3 playerYawOnly = {0, VR_GetTurnYawAngle(), 0};
//     const auto [vfwd, vright, vup] = getAngledVectors(playerYawOnly);
//     return vfwd;
// }();



// const auto maxDotIdx = maxIdxBetween(dotHeadHand[0], dotHeadHand[1]);
// const auto minDotIdx = maxDotIdx == 0 ? 1 : 0;
//
// const auto& maxDot = dotHeadHand[maxDotIdx];
// const auto& minDot = dotHeadHand[minDotIdx];
// const auto blendedDot = glm::mix(minDot, maxDot, 0.5f);



// const auto& longestHLen = longestIdx == 0 ? lenH0 : lenH1;
// const auto& shortestHLen = longestIdx == 0 ? lenH1 : lenH0;

// const auto lenDiff = lenH1 - lenH0;
// const auto dotDiff = dotHeadHand[1] - dotHeadHand[0];

//  const auto lengthCoefficient = std::clamp(
//     (float)mapRange(lenDiff, -1.5f, 1.5f, 0.25f, 0.75f), 0.0f, 1.f);

// const auto h0lcoef = lengthCoefficient < 0.5f ? lengthCoefficient * 2.f
//                                              : lengthCoefficient * 0.1f;
// const auto h1lcoef =
//    lengthCoefficient > 0.5f ? lengthCoefficient : lengthCoefficient *
//    0.1f;
//
// const float h0coef =
//    glm::mix(std::clamp(dotHeadHand[0] * 1.15f, 0.1f, 1.f), h0lcoef,
//    0.5f);
// const float h1coef =
//    glm::mix(std::clamp(dotHeadHand[1] * 1.15f, 0.1f, 1.f), h1lcoef,
//    0.5f);
//
// const auto mixh0 =
//    glm::normalize(glm::mix(headDir, headHandDirs[0], h0coef));
// const auto mixh1 =
//    glm::normalize(glm::mix(headDir, headHandDirs[1], h1coef));


//    const auto mixHandDir = glm::normalize(glm::mix(mixh0, mixh1, 0.5f));
//
//    const float coefficient = mapRange(blendedDot, -1.f, 1.f, 0.f, 1.f);
//
//    const auto mixFinalDir = glm::normalize(
//        glm::mix(headDir, mixHandDir, std::clamp(coefficient, 0.f, 1.f)));
//
//    Con_Printf("%.2f -> %.2f || %.2f || %.2f | %.2f -> %.2f -> %.2f\n",
//    lenDiff,
//        lengthCoefficient, dotDiff, dotHeadHand[0], dotHeadHand[1],
//        blendedDot, coefficient);



[[nodiscard]] glm::vec3 QuatToYawPitchRoll3(const vr::HmdQuaternion_t& q)
{
    glm::vec3 res;

    // roll (x-axis rotation)
    double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    res[PITCH] = -std::atan2(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    double sinp = 2 * (q.w * q.y - q.z * q.x);
    if(std::abs(sinp) >= 1)
        res[YAW] =
            std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
    else
        res[YAW] = std::asin(sinp);

    // yaw (z-axis rotation)
    double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    res[ROLL] = -std::atan2(siny_cosp, cosy_cosp);

    return glm::degrees(res) + glm::vec3{0, VR_GetTurnYawAngle(), 0};
}

[[nodiscard]] float NormalizeAngle(float angle)
{
    while(angle > 360) angle -= 360;
    while(angle < 0) angle += 360;
    return angle;
}

[[nodiscard]] glm::vec3 NormalizeAngles(glm::vec3 angles)
{
    angles.x = NormalizeAngle(angles.x);
    angles.y = NormalizeAngle(angles.y);
    angles.z = NormalizeAngle(angles.z);
    return angles;
}

[[nodiscard]] glm::vec3 QuatToYawPitchRoll4(const vr::HmdQuaternion_t& q1)
{
    double sqw = q1.w * q1.w;
    double sqx = q1.x * q1.x;
    double sqy = q1.y * q1.y;
    double sqz = q1.z * q1.z;

    // if normalised is one, otherwise is correction factor
    double unit = sqx + sqy + sqz + sqw;

    double test = q1.x * q1.w - q1.y * q1.z;

    glm::vec3 v;

    if(test > 0.4995f * unit)
    {
        // singularity at north pole
        v.z = 2.0 * atan2(q1.y, q1.x);
        v.x = glm::pi<double>() / 2.0;
        v.y = 0;
        return NormalizeAngles(glm::degrees(v));
    }

    if(test < -0.4995f * unit)
    {
        // singularity at south pole
        v.z = -2.0 * atan2(q1.y, q1.x);
        v.x = -glm::pi<double>() / 2.0;
        v.y = 0;
        return NormalizeAngles(glm::degrees(v));
    }

    vr::HmdQuaternion_t q{q1.w, q1.z, q1.x, q1.y};
    v.z = (double)atan2(2.0 * q.x * q.w + 2.0 * q.y * q.z,
        1 - 2.0 * (q.z * q.z + q.w * q.w));            // Yaw
    v.x = (double)asin(2.0 * (q.x * q.z - q.w * q.y)); // Pitch
    v.y = (double)atan2(2.0 * q.x * q.y + 2.0 * q.z * q.w,
        1 - 2.0 * (q.y * q.y + q.z * q.z)); // Roll

    return NormalizeAngles(
        glm::degrees(v) + glm::vec3{0, VR_GetTurnYawAngle(), 180});
}

[[nodiscard]] glm::vec3 QuatToYawPitchRoll2(const vr::HmdQuaternion_t& q)
{
    // return QuatToYawPitchRoll(q);

    const double test = q.x * q.y + q.z * q.w;

    if(test > 0.499)
    {
        Con_Printf("north singularity\n");

        // singularity at north pole
        return {
            //
            0,                     // bank
            2 * atan2(q.x, q.w),   // heading
            -glm::pi<double>() / 2 // attitude
        };                         //
    }

    if(test < -0.499)
    {
        Con_Printf("south singularity\n");

        // singularity at south pole
        return {
            //
            0,                    // bank
            -2 * atan2(q.x, q.w), // heading
            glm::pi<double>() / 2 // attitude
        };                        //
    }

    const double sqx = q.x * q.x;
    const double sqy = q.y * q.y;
    const double sqz = q.z * q.z;
    const double sqw = q.w * q.w;

    glm::vec3 res{
        -atan2(2 * q.x * q.w - 2 * q.y * q.z, 1 - 2 * sqx - 2 * sqz), // bank
        atan2(2 * q.y * q.w - 2 * q.x * q.z, 1 - 2 * sqy - 2 * sqz),  // heading
        -asin(2 * test) // attitude
    };

    return glm::degrees(res) + glm::vec3{0, VR_GetTurnYawAngle(), 0};
}


// TODO VR: (P0) needed at all? Consider cvar for old style weapon cycling
/*
// TODO VR: (P2) code repetition, don't change to best weapon
if(time > self.attack_finished &&
   getCurrentAmmo(MAIN_HAND) <= 0 &&
   self.weapon != WID_FIST &&
   self.weapon != WID_AXE &&
   self.weapon != WID_MJOLNIR &&
   self.weapon != WID_GRAPPLE)
{
    // self.weapon = W_BestWeapon ();
    W_SetCurrentAmmoFor (MAIN_HAND);
}

if(time > self.offhand_attack_finished &&
   getCurrentAmmo(OFF_HAND) <= 0 &&
   self.weapon2 != WID_FIST &&
   self.weapon2 != WID_AXE &&
   self.weapon2 != WID_MJOLNIR &&
   self.weapon2 != WID_GRAPPLE)
{
    // self.weapon2 = W_BestWeapon ();
    W_SetCurrentAmmoFor (OFF_HAND);
}
*/

if(hand == cVR_OffHand)
{
    Con_Printf(
        "otherWpnCvar: %d | 2hdmf: %d | active2hh: %d | if2ha: "
        "%d\n",
        otherWpnCvar, twoHDisplayModeFixed, VR_IsActive2HHelpingHand(hand),
        inFixed2HAiming);
}


{
    vr::VROverlayHandle_t oh;
    vr::VROverlay()->CreateOverlay("testoverlay", "testoverlay", &oh);
    vr::VROverlay()->SetOverlayFlag(
        oh, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true);
    vr::VROverlay()->ShowOverlay(oh);
}


thing = findradius(spot.origin, 32);
while(thing)
{
    if (thing.classname == "player")
        pcount = pcount + 1;
    thing = thing.chain;
}


/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss(edict_t* ent)
{
    // regular thinking
    if(!SV_RunThink(ent))
    {
        return;
    }

    glm::vec3 offsetBuffer;

    const auto checkGroundCollision = [&](trace_t& traceBuffer,
                                          const glm::vec3& move) {
        const auto checkCorner = [&](const glm::vec3& pos) {
            const glm::vec3 end = pos + move;
            traceBuffer = SV_MoveTrace(pos, end, MOVE_NOMONSTERS, ent);

            const auto hit = quake::util::hitSomething(traceBuffer);
            const auto goodNormal = traceBuffer.plane.normal[2] > 0.7;

            if(!strcmp(PR_GetString(ent->v.classname), "thrown_weapon"))
            {
                // Con_Printf("h: %d | n: %d\n", hit, goodNormal);
            }

            return hit && goodNormal;
        };

        const glm::vec3 bottomOrigin = ent->v.origin + ent->v.mins[2];

        // return checkCorner(bottomOrigin);

        const auto left = ent->v.mins[0];
        const auto right = ent->v.maxs[0];
        const auto fwd = ent->v.mins[1];
        const auto back = ent->v.maxs[1];

        offsetBuffer = glm::vec3{left, fwd, 0.f};
        if(checkCorner(bottomOrigin + offsetBuffer))
        {
            return true;
        }

        offsetBuffer = glm::vec3{left, back, 0.f};
        if(checkCorner(bottomOrigin + offsetBuffer))
        {
            return true;
        }

        offsetBuffer = glm::vec3{right, fwd, 0.f};
        if(checkCorner(bottomOrigin + offsetBuffer))
        {
            return true;
        }

        offsetBuffer = glm::vec3{right, back, 0.f};
        if(checkCorner(bottomOrigin + offsetBuffer))
        {
            return true;
        }

        /*
        const auto v0 = bottomOrigin +glm::vec3{left, fwd, 0.f} ;
        const auto v1 = bottomOrigin + glm::vec3{left, back, 0.f};
        const auto v2 = bottomOrigin + glm::vec3{right, fwd, 0.f};
        const auto v3 = bottomOrigin + glm::vec3{right, back, 0.f};

        return checkCorner(v0) || checkCorner(v1) || checkCorner(v2) ||
               checkCorner(v3);
        */

        return false;
    };

    // if onground, return without moving
    // if((int)ent->v.flags & FL_ONGROUND)
    {
        auto vel = ent->v.velocity;
        vel[2] -= SV_AddGravityImpl(ent);

        const glm::vec3 move = vel * static_cast<float>(host_frametime);

        if(trace_t traceBuffer; !checkGroundCollision(traceBuffer, move))
        {
            // if(!strcmp(PR_GetString(ent->v.classname), "thrown_weapon"))
            //     Con_Printf("F\n");

            if((int)ent->v.flags & FL_ONGROUND)
            {
                Con_Printf("Removing on ground\n");
                ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
            }
        }
        else
        {
            // if(!strcmp(PR_GetString(ent->v.classname), "thrown_weapon"))
            //     Con_Printf("T\n");

            const float backoff =
                ent->v.movetype == MOVETYPE_BOUNCE ? 1.5f : 1.f;

            ClipVelocity(ent->v.velocity, traceBuffer.plane.normal,
                ent->v.velocity, backoff);

            if(ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
            {
                // Con_Printf("RRR\n");
                if(!((int)ent->v.flags & FL_ONGROUND))
                {
                    Con_Printf("Setting on ground\n");
                    ent->v.flags = (int)ent->v.flags | FL_ONGROUND;

                    ent->v.groundentity = EDICT_TO_PROG(traceBuffer.ent);
                    ent->v.velocity = vec3_zero;
                    ent->v.avelocity = vec3_zero;
                    ent->v.origin =
                        traceBuffer.endpos - ent->v.mins[2] - offsetBuffer;

                    SV_LinkEdict(ent, true);
                    SV_PushEntityImpact(ent, traceBuffer);
                }

                return;
            }
        }
    }

    SV_CheckVelocity(ent);

    // add gravity
    if(ent->v.movetype != MOVETYPE_FLY &&
        ent->v.movetype != MOVETYPE_FLYMISSILE)
    {
        SV_AddGravity(ent);
    }

    // move angles
    ent->v.angles += static_cast<float>(host_frametime) * ent->v.avelocity;

    // move origin
    const glm::vec3 move = ent->v.velocity * static_cast<float>(host_frametime);
    // SV_PushEntity(ent, move);

    const trace_t trace = SV_PushEntity(ent, move);

    if(!quake::util::hitSomething(trace) || ent->free)
    {
        return;
    }

    const float backoff = ent->v.movetype == MOVETYPE_BOUNCE ? 1.5f : 1.f;
    ClipVelocity(ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);

    // stop if on ground
    /*
    if(checkGroundCollision({0.f, 0.f, 0.f}))
    {
        if(ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
        {
            if(!((int)ent->v.flags & FL_ONGROUND))
            {
                Con_Printf("Setting on ground DOWN\n");
                ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
            }

            ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
            ent->v.groundentity = EDICT_TO_PROG(trace.ent);
            ent->v.velocity = vec3_zero;
            ent->v.avelocity = vec3_zero;
        }
    }
    */

    // check for in water
    SV_CheckWaterTransition(ent);
}


    Mod_ForAllKnownNames([](const char* name) {
        const std::string_view s{name};

        const auto endsWith = [&s](const std::string_view sv) {
            return s.size() >= sv.size() &&
                   s.compare(s.size() - sv.size(), std::string::npos, sv) == 0;
        };

        if(!endsWith(".bsp"))
        {
            return;
        }

        const auto isAnyOf = [&s](const auto& range) {
            return std::any_of(std::begin(range), std::end(range),
                [&s](const auto& x) { return x == s; });
        };

        const auto alreadySeen =
            isAnyOf(mapsVanilla) || isAnyOf(mapsSoa) || isAnyOf(mapsDopa);

        if(!alreadySeen)
        {
            mapsExtra.emplace_back(s);
        }
    });


// by Qmaster
Wrist flick::
=========
Should be doable if you know the hand location with both time and distance check.  Pseudocode:
float lasthandpos; //store hand position every flicktime tick
float flicktime = 0.200; // or reasonable time in secs for flick check..,should probably get from cvar
float flickthreshholddistance = 4; // again, should be using a cvar for testing ease and player customizability
float flickfinished;

Then wherever you normally check hand position call this:
bool() FlickCheck = {
if (!grab button is pressed) return false;

// typical check every time interval
if (flickfinished<time)  {
float dist = handpos - lasthandpos;
lasthandpos = handpos;
flickfinished = time + flicktime; // typical time interval code
if (dist > flickthreshholddistance) return true;
}
}
};



    const auto handAnimationToFrame = [](const VrHandAnimation x) {
        if(x == VrHandAnimation::Open)
        {
            return 9;
        }

        if(x == VrHandAnimation::Pointing)
        {
            return 3;
        }

        if(x == VrHandAnimation::Fist)
        {
            return 6;
        }

        if(x == VrHandAnimation::AlmostPointing)
        {
            return 2;
        }

        assert(x == VrHandAnimation::OkSign);
        return 15;
    };
