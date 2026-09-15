#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

#include "include/obj.hpp"


OBJ::OBJLoadStatusReturn OBJ::loadOBJFromBuffer(
    const std::vector<char> &buffer,
    std::vector<OBJ::OBJVertex> &outVertices
) {
    std::vector<point3> positions;
    std::vector<vec3> normals;
    std::vector<std::pair<double, double>> uvs;

    bool hasUV = false;
    bool hasNormal = false;

    std::istringstream ss(std::string(buffer.begin(), buffer.end()));
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;  // At least one char
        if (line[line.length() - 1] == '\r') line.pop_back();
        if (line.empty()) continue;  // Welp, the one char WAS a \r wasnt it..

        std::istringstream ls(line);
        std::string type;
        ls >> type;

        if (type == "v") {
            double x, y, z;
            ls >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (type == "vt") {
            double u, v;
            ls >> u >> v;
            uvs.emplace_back(u, v);
            hasUV = true;
        }
        else if (type == "vn") {
            double x, y, z;
            ls >> x >> y >> z;
            normals.emplace_back(x, y, z);
            hasNormal = true;
        }
        else if (type == "f") {
            std::vector<OBJ::OBJVertex> face;
            std::string vertStr;
            while (ls >> vertStr) {
                int v = 0, vt = 0, vn = 0;

                size_t firstSlash = vertStr.find('/');
                if (firstSlash == std::string::npos) {
                    v = std::stoi(vertStr);

                } else {
                    v = std::stoi(vertStr.substr(0, firstSlash));
                    size_t secondSlash = vertStr.find('/', firstSlash + 1);

                    if (secondSlash == std::string::npos) {
                        vt = std::stoi(vertStr.substr(firstSlash + 1));
                    } else {
                        std::string vtStr = vertStr.substr(
                            firstSlash + 1, secondSlash - firstSlash - 1
                        );
                        if (!vtStr.empty()) vt = std::stoi(vtStr);

                        std::string vnStr = vertStr.substr(secondSlash + 1);
                        if (!vnStr.empty()) vn = std::stoi(vnStr);
                    }
                }

                // Someone decided that OBJ indices can be negative to signal
                // N-th from the last vertex parsed so far... THANKS A LOT MAN
                // FOR MAKING MY LIFE MORE DIFFICULT THAN IT NEEDS TO BE!
                if (v < 0) v = static_cast<int>(positions.size()) + v + 1;
                if (vt < 0) vt = static_cast<int>(uvs.size()) + vt + 1;
                if (vn < 0) vn = static_cast<int>(normals.size()) + vn + 1;

                if (v <= 0 || static_cast<size_t>(v) > positions.size())
                    throw std::runtime_error("OBJ: face references an invalid vertex index");

                OBJ::OBJVertex vert{};
                vert.pos = positions[v - 1];
                if (hasUV && vt > 0 && static_cast<size_t>(vt) <= uvs.size()) {
                    vert.u = uvs[vt - 1].first;
                    vert.v = uvs[vt - 1].second;
                }

                if (hasNormal && vn > 0 && static_cast<size_t>(vn) <= normals.size())
                    vert.normal = unit_vector(normals[vn - 1]);

                face.push_back(vert);
            }

            for (size_t i = 1; i + 1 < face.size(); ++i) {
                outVertices.push_back(face[0]);
                outVertices.push_back(face[i]);
                outVertices.push_back(face[i + 1]);
            }
        }
    }

    OBJLoadStatusReturn status;
    status.hasUV = hasUV;
    status.hasNormal = hasNormal;
    return status;
}

bool load_obj_corners(const std::string &path, obj_mesh &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    std::vector<char> buffer(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()
    );

    std::vector<OBJ::OBJVertex> vertices;
    OBJ::OBJLoadStatusReturn status = OBJ::loadOBJFromBuffer(buffer, vertices);

    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        std::vector<obj_corner> tri(3);
        for (int c = 0; c < 3; c++) {
            const OBJ::OBJVertex &v = vertices[i + c];
            tri[c].pos = v.pos;
            tri[c].normal = v.normal;
            tri[c].has_normal = status.hasNormal;
        }
        out.faces.push_back(tri);
    }

    // Fill in flat face normals for any triangle the OBJ didn't give one to.
    if (!status.hasNormal) {
        for (auto &tri : out.faces) {
            vec3 flatNormal = unit_vector(
                cross(tri[1].pos - tri[0].pos, tri[2].pos - tri[0].pos)
            );
            for (auto &corner : tri) {
                corner.normal = flatNormal;
                corner.has_normal = true;
            }
        }
    }

    return true;
}
