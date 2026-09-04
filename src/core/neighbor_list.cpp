#include "core/neighbor_list.h"

#include <algorithm>
#include <cmath>

NeighborList BuildNeighborList(const double* R, uint32_t natoms, double cutoff) {
    NeighborList nl;
    if (natoms < 2 || cutoff <= 0.0) return nl;

    double lo[3], hi[3];
    for (int k = 0; k < 3; ++k) lo[k] = hi[k] = R[k];
    for (uint32_t a = 0; a < natoms; ++a)
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], R[3 * a + k]);
            hi[k] = std::max(hi[k], R[3 * a + k]);
        }

    // Cell edge >= cutoff so the 27-cell stencil is complete. Grow it when
    // the box is so sparse that a dense grid would dwarf the atom count.
    double cell = cutoff;
    int64_t nx, ny, nz;
    for (;;) {
        nx = (int64_t)((hi[0] - lo[0]) / cell) + 1;
        ny = (int64_t)((hi[1] - lo[1]) / cell) + 1;
        nz = (int64_t)((hi[2] - lo[2]) / cell) + 1;
        if (nx * ny * nz <= 8 * (int64_t)natoms + 4096) break;
        cell *= 1.5;
    }
    const double inv = 1.0 / cell;
    const size_t ncell = (size_t)(nx * ny * nz);

    // Counting sort of atoms by cell: start[c]..start[c+1] index `sorted`.
    std::vector<int32_t> cellOf(natoms);
    std::vector<uint32_t> start(ncell + 1, 0);
    for (uint32_t a = 0; a < natoms; ++a) {
        const int64_t cx = (int64_t)((R[3 * a] - lo[0]) * inv);
        const int64_t cy = (int64_t)((R[3 * a + 1] - lo[1]) * inv);
        const int64_t cz = (int64_t)((R[3 * a + 2] - lo[2]) * inv);
        const int32_t c = (int32_t)((cx * ny + cy) * nz + cz);
        cellOf[a] = c;
        ++start[(size_t)c + 1];
    }
    for (size_t c = 0; c < ncell; ++c) start[c + 1] += start[c];
    std::vector<uint32_t> sorted(natoms);
    {
        std::vector<uint32_t> fill(start.begin(), start.end() - 1);
        for (uint32_t a = 0; a < natoms; ++a) sorted[fill[(size_t)cellOf[a]]++] = a;
    }

    const double cut2 = cutoff * cutoff;
    nl.i.reserve(natoms * 2);
    nl.j.reserve(natoms * 2);
    nl.d.reserve(natoms * 2);
    std::vector<std::pair<uint32_t, double>> block;   // this atom's neighbours, sorted before emitting
    for (uint32_t a = 0; a < natoms; ++a) {
        const int32_t c = cellOf[a];
        const int64_t cz = c % nz, cy = (c / nz) % ny, cx = c / (nz * ny);
        const double ax = R[3 * a], ay = R[3 * a + 1], az = R[3 * a + 2];
        block.clear();
        for (int64_t dx = -1; dx <= 1; ++dx) {
            const int64_t x = cx + dx;
            if (x < 0 || x >= nx) continue;
            for (int64_t dy = -1; dy <= 1; ++dy) {
                const int64_t y = cy + dy;
                if (y < 0 || y >= ny) continue;
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    const int64_t z = cz + dz;
                    if (z < 0 || z >= nz) continue;
                    const size_t nc = (size_t)((x * ny + y) * nz + z);
                    for (uint32_t k = start[nc]; k < start[nc + 1]; ++k) {
                        const uint32_t b = sorted[k];
                        if (b <= a) continue;   // each pair once, i < j
                        const double ddx = ax - R[3 * b], ddy = ay - R[3 * b + 1], ddz = az - R[3 * b + 2];
                        const double d2 = ddx * ddx + ddy * ddy + ddz * ddz;
                        if (d2 < cut2) block.emplace_back(b, std::sqrt(d2));
                    }
                }
            }
        }
        std::sort(block.begin(), block.end(), [](const auto& p, const auto& q) { return p.first < q.first; });
        for (const auto& [b, dist] : block) {
            nl.i.push_back((int64_t)a);
            nl.j.push_back((int64_t)b);
            nl.d.push_back(dist);
        }
    }
    return nl;
}
