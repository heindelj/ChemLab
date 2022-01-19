#pragma once
#include <iostream>

template<typename T>
void debug(T data) {
	std::cout << data << std::endl;
}

void debug(const Vector2& v) {
	std::cout << "[ " << v.x << ", " << v.y << " ]" << std::endl;
}

void debug(const Vector3& v) {
	std::cout << "[ " << v.x << ", " << v.y <<  ", " << v.z << " ]" << std::endl;
}

void debug(const Vector4& v) {
	std::cout << "[ " << v.x << ", " << v.y <<  ", " << v.z << ", " << v.w << " ]" << std::endl;
}

void debug(const glm::vec2& v) {
	std::cout << "[ " << v.x << ", " << v.y << " ]" << std::endl;
}

void debug(const glm::vec3& v) {
	std::cout << "[ " << v.x << ", " << v.y <<  ", " << v.z << " ]" << std::endl;
}

void debug(const glm::vec4& v) {
	std::cout << "[ " << v.x << ", " << v.y <<  ", " << v.z << ", " << v.w << " ]" << std::endl;
}

void debug(const glm::mat4& m) {
	std::cout << "[ " << m[0].x << ", " << m[0].y <<  ", " << m[0].z << ", " << m[0].w << " ]" << std::endl;
	std::cout << "[ " << m[1].x << ", " << m[1].y <<  ", " << m[1].z << ", " << m[1].w << " ]" << std::endl;
	std::cout << "[ " << m[2].x << ", " << m[2].y <<  ", " << m[2].z << ", " << m[2].w << " ]" << std::endl;
	std::cout << "[ " << m[3].x << ", " << m[3].y <<  ", " << m[3].z << ", " << m[3].w << " ]" << std::endl;
}

void debug(const Atoms& atoms) {
	for (int i = 0 ; i < atoms.natoms; ++i) {
		std::cout << atoms.labels[i] << " " << atoms.xyz[i].x << " " << atoms.xyz[i].y  << " " << atoms.xyz[i].z  << std::endl;
	}
}

void debug(const Frames& frames) {
	for (int i = 0 ; i < frames.nframes; ++i) {
		std::cout << frames.atoms[i].natoms << std::endl;
		std::cout << frames.headers[i] << std::endl;
		debug(frames.atoms[i]);
	}
}