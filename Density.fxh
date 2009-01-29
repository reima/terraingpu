float DENSITY(float3 Position) {
  float density = -Position.y-0.1;
  density += sin(Position.x*6)*cos(Position.z*6);
  return density;
}