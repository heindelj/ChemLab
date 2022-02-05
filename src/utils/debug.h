#pragma once

template<typename T>
void debug(T data) {
	std::cout << data << std::endl;
}

template<typename T>
void debug(const std::set<T> data) {
	for (auto it = data.begin(); it != data.end(); ++it) 
		std::cout << *it << ", ";
	std::cout << std::endl;
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

void debug(const Color& color){
	debug(ColorNormalize(color));
}

void debug(const Matrix& m) {
	std::cout << "[ " << m.m0 << ", " << m.m4 <<  ", " << m.m8  << ", " << m.m12 << " ]" << std::endl;
	std::cout << "[ " << m.m1 << ", " << m.m5 <<  ", " << m.m9  << ", " << m.m13 << " ]" << std::endl;
	std::cout << "[ " << m.m2 << ", " << m.m6 <<  ", " << m.m10 << ", " << m.m14 << " ]" << std::endl;
	std::cout << "[ " << m.m3 << ", " << m.m7 <<  ", " << m.m11 << ", " << m.m15 << " ]" << std::endl;
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