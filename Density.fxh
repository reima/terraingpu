float DENSITY(float3 Position) {
  float density = -Position.y-0.1;
  density += cos((Position.x-0.5)*3)*cos((Position.z-0.5)*3);
  return density;
}