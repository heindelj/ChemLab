#pragma once

#include "raylib.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

//////////////////////////////////////////
////// raymath to glm conversions ////////
//////////////////////////////////////////

glm::vec2 fromVector2(Vector2 v) {
	return (glm::vec2){v.x, v.y};
}

glm::vec3 fromVector3(Vector3 v) {
	return (glm::vec3){v.x, v.y, v.z};
}

glm::vec4 fromVector4(Vector4 v) {
	return (glm::vec4){v.x, v.y, v.z, v.w};
}



//////////////////////////////////////////
////// glm to raymath conversions ////////
//////////////////////////////////////////

Vector2 fromVector2(glm::vec2 v) {
	return (Vector2){v.x, v.y};
}

Vector3 fromVector3(glm::vec3 v) {
	return (Vector3){v.x, v.y, v.z};
}

Vector4 fromVector4(glm::vec4 v) {
	return (Vector4){v.x, v.y, v.z, v.w};
}