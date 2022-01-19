#pragma once

//////////////////////////////////////////
////// raymath to glm conversions ////////
//////////////////////////////////////////

glm::vec2 fromVector2(const Vector2& v) {
	return (glm::vec2){v.x, v.y};
}

glm::vec3 fromVector3(const Vector3& v) {
	return (glm::vec3){v.x, v.y, v.z};
}

glm::vec4 fromVector4(const Vector4& v) {
	return (glm::vec4){v.x, v.y, v.z, v.w};
}



//////////////////////////////////////////
////// glm to raymath conversions ////////
//////////////////////////////////////////

Vector2 fromVector2(const glm::vec2& v) {
	return (Vector2){v.x, v.y};
}

Vector3 fromVector3(const glm::vec3& v) {
	return (Vector3){v.x, v.y, v.z};
}

Vector4 fromVector4(const glm::vec4& v) {
	return (Vector4){v.x, v.y, v.z, v.w};
}