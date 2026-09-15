#ifndef FORMATS_OBJ_HPP
#define FORMATS_OBJ_HPP

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>

#include "rtweekend.hpp"


namespace OBJ {
    struct OBJVertex {
        point3 pos;
        vec3 normal;
        double u = 0.0, v = 0.0;
    };

    struct OBJLoadStatusReturn {
        bool hasUV;
        bool hasNormal;
    };

    OBJLoadStatusReturn loadOBJFromBuffer(
        const std::vector<char> &buffer, std::vector<OBJVertex> &outVertices
    );
};

struct obj_corner {
    point3 pos;
    vec3 normal;
    bool has_normal;
};

struct obj_mesh {
    std::vector<std::vector<obj_corner>> faces;  // one vector<obj_corner> per triangle
};

bool load_obj_corners(const std::string &path, obj_mesh &out);

#endif // FORMATS_OBJ_HPP
