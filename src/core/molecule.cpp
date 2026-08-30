#include "core/molecule.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>

#include "core/atomic_data.h"
#include "core/math_utils.h"

RenderData GetRenderData(const std::string& atomLabel) {
    auto c = atomColors.find(atomLabel);
    auto v = atomVdwRadii.find(atomLabel);
    auto r = covalentRadii.find(atomLabel);
    if (c == atomColors.end() || v == atomVdwRadii.end() || r == covalentRadii.end())
        throw std::invalid_argument("Unknown element label: " + atomLabel);
    return RenderData{c->second, v->second, r->second};
}

bool IsKnownElement(const std::string& atomLabel) {
    return atomColors.count(atomLabel) && atomVdwRadii.count(atomLabel) && covalentRadii.count(atomLabel);
}

BondList MakeCovalentBondList(const Atoms& atoms, float tolerance) {
    // Eq. (1) of "A rule-based algorithm for automatic bond type perception",
    // J. Cheminformatics 4, 26 (2012). Only the distance criterion for now.
    BondList bonds;
    for (uint32_t i = 0; i + 1 < atoms.natoms; i++) {
        for (uint32_t j = i + 1; j < atoms.natoms; j++) {
            const float cutoff = atoms.renderData[i].covalentRadius + atoms.renderData[j].covalentRadius + tolerance;
            if (Vector3Length(Vector3Subtract(atoms.xyz[i], atoms.xyz[j])) < cutoff)
                bonds.pairs.emplace_back(i, j);
        }
    }
    return bonds;
}

float Distance(const Vector3& a, const Vector3& b) { return norm(a - b); }

float AngleDeg(const Vector3& a, const Vector3& b, const Vector3& c) {
    const Vector3 u = a - b, v = c - b;
    const float d = dot(u, v) / (norm(u) * norm(v));
    return acosf(std::fmax(-1.0f, std::fmin(1.0f, d))) * RAD2DEG;
}

float DihedralDeg(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d) {
    const Vector3 r1 = b - a, r2 = c - b, r3 = d - c;
    const Vector3 n1 = normalize(cross(r1, r2));
    const Vector3 n2 = normalize(cross(r2, r3));
    const Vector3 m1 = cross(n1, normalize(r2));
    return atan2f(dot(m1, n2), dot(n1, n2)) * RAD2DEG;
}

Vector3 Centroid(const std::vector<Vector3>& points) {
    if (points.empty()) return Vector3Zero();
    return centroid(points);
}

namespace {

bool ParseDouble(const char* text, double& out) {
    char* end = nullptr;
    out = std::strtod(text, &end);
    return end != text;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

}  // namespace

std::optional<double> ParseEnergyFromHeader(const std::string& header) {
    // Pass 1: key=value / key:value tokens (extxyz and friends). Rank keys so
    // "energy" beats "energy_hartree" beats any other *energy* key, and
    // bias/confinement/reference terms never win over the real energy.
    int bestRank = 99;
    double bestValue = 0.0;
    bool sawKeyValue = false;
    size_t i = 0;
    const size_t n = header.size();
    while (i < n) {
        while (i < n && std::isspace((unsigned char)header[i])) i++;
        size_t start = i;
        bool inQuotes = false;
        while (i < n && (inQuotes || !std::isspace((unsigned char)header[i]))) {
            if (header[i] == '"') inQuotes = !inQuotes;
            i++;
        }
        if (i <= start) break;
        const std::string token = header.substr(start, i - start);
        const size_t eq = token.find_first_of("=:");
        if (eq == std::string::npos || eq == 0) continue;
        sawKeyValue = sawKeyValue || token[eq] == '=';
        const std::string key = ToLower(token.substr(0, eq));
        if (key.find("energy") == std::string::npos && key != "e") continue;
        if (key.find("bias") != std::string::npos || key.find("confine") != std::string::npos ||
            key.find("free_energy") != std::string::npos)
            continue;
        double value;
        if (!ParseDouble(token.c_str() + eq + 1, value)) continue;
        const int rank = key == "energy" || key == "e" ? 0
                       : key == "energy_hartree" || key == "total_energy" || key == "energy_ev" ? 1
                                                                                               : 2;
        if (rank < bestRank) {
            bestRank = rank;
            bestValue = value;
        }
    }
    if (bestRank < 99) return bestValue;

    // Pass 2: "E = -76.4" / "Energy: -76.4" with spaces around the separator.
    const std::string lower = ToLower(header);
    for (const char* key : {"energy", "e "}) {
        size_t pos = lower.find(key);
        while (pos != std::string::npos) {
            size_t j = pos + std::strlen(key);
            while (j < n && std::isspace((unsigned char)lower[j])) j++;
            if (j < n && (lower[j] == '=' || lower[j] == ':')) {
                double value;
                if (ParseDouble(header.c_str() + j + 1, value)) return value;
            }
            pos = lower.find(key, pos + 1);
        }
    }

    // Pass 3: a bare float (common "natoms\n-76.43\n..." files) -- but only when
    // the header is not key=value formatted, where a bare float would just be
    // some unrelated field's number.
    if (!sawKeyValue) {
        for (size_t k = 0; k < n; ++k) {
            const char c = header[k];
            if (std::isdigit((unsigned char)c) || (c == '-' && k + 1 < n && std::isdigit((unsigned char)header[k + 1]))) {
                double value;
                char* end = nullptr;
                value = std::strtod(header.c_str() + k, &end);
                if (end != header.c_str() + k && std::strchr(header.c_str() + k, '.') && std::strchr(header.c_str() + k, '.') < end)
                    return value;
                k = end - header.c_str();
            }
        }
    }
    return std::nullopt;
}

void CacheFrameEnergies(Frames& frames) {
    frames.energies.assign(frames.headers.size(), std::numeric_limits<double>::quiet_NaN());
    frames.anyEnergy = false;
    for (size_t i = 0; i < frames.headers.size(); ++i) {
        if (auto e = ParseEnergyFromHeader(frames.headers[i])) {
            frames.energies[i] = *e;
            frames.anyEnergy = true;
        }
    }
}

