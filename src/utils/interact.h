// Anything to do with interacting with a molecule. Lots of ray casting and
// edit mode related functionality.

int BallAndStickModel::TestRayAgainst(Ray ray) {
	int collisonIndex = -1; // default which is returned on no collision
	float collisionDistance = FLT_MAX;

	// SPEED: This may be very slow for large numbers of atoms
	// so we should consider using a proper data structure
	// for this if it becomes a bottleneck
	for (int i = 0; i < this->numSpheres; i++) {
		EntityIndices indices = this->IDToEntityIndices[i];
		RayCollision collisionInfo = GetRayCollisionMesh(ray, this->sphereMesh, this->transforms[indices.materialIndex][indices.transformIndex]); // SPEED: Might be faster to check against sphere since we know these are spheres
		// Check if we got a collision and then make sure we don't
		// overwrite a collision with a closer object
		if (collisionInfo.hit) {
			if (collisonIndex == -1) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			} else if (collisionInfo.distance < collisionDistance) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			}			
		}
	}

	return collisonIndex;
}

int SpheresModel::TestRayAgainst(Ray ray) {
	int collisonIndex = -1; // default which is returned on no collision
	float collisionDistance = FLT_MAX;

	// SPEED: This may be very slow for large numbers of atoms
	// so we should consider using a proper data structure
	// for this if it becomes a bottleneck
	for (int i = 0; i < this->numSpheres; i++) {
		EntityIndices indices = this->IDToEntityIndices[i];
		RayCollision collisionInfo = GetRayCollisionMesh(ray, this->sphereMesh,
		this->transforms[indices.materialIndex][indices.transformIndex]); // SPEED: Might be faster to check against sphere since we know these are spheres
		// Check if we got a collision and then make sure we don't
		// overwrite a collision with a closer object
		if (collisionInfo.hit) {
			if (collisonIndex == -1) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			} else if (collisionInfo.distance < collisionDistance) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			}			
		}
	}

	return collisonIndex;
}

int SticksModel::TestRayAgainst(Ray ray) {
	int collisonIndex = -1; // default which is returned on no collision
	float collisionDistance = FLT_MAX;

	// SPEED: This may be very slow for large numbers of atoms
	// so we should consider using a proper data structure
	// for this if it becomes a bottleneck
	for (int i = 0; i < this->numSticks; i++) {
		EntityIndices indices = this->IDToEntityIndices[i];
		RayCollision collisionInfo = GetRayCollisionMesh(ray, this->stickMesh,
			this->transforms[indices.materialIndex][indices.transformIndex]);
		// Check if we got a collision and then make sure we don't
		// overwrite a collision with a closer object
		if (collisionInfo.hit) {
			if (collisonIndex == -1) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			} else if (collisionInfo.distance < collisionDistance) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			}			
		}
	}

	return collisonIndex;
}

int TestRayAgainstAtoms(Ray ray, const Atoms& atoms) {
	int collisonIndex = -1; // default which is returned on no collision
	float collisionDistance = FLT_MAX;

	// SPEED: This may be very slow for large numbers of atoms
	// so we should consider using a proper data structure
	// for this if it becomes a bottleneck
	for (int i = 0; i < atoms.natoms; i++) {
		RayCollision collisionInfo = GetRayCollisionSphere(ray, atoms.xyz[i], atoms.renderData[i].vdwRadius * g_ballScale);
		// Check if we got a collision and then make sure we don't
		// overwrite a collision with a closer object
		if (collisionInfo.hit) {
			if (collisonIndex == -1) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			} else if (collisionInfo.distance < collisionDistance) {
				collisonIndex = i;
				collisionDistance = collisionInfo.distance;
			}			
		}
	}

	return collisonIndex;
}