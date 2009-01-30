float DENSITY(float3 Position) {
  float density = -Position.y;
  Position.xz *= 5;
  density += cos(Position.x*1)*cos(Position.z*1)*0.2;
  density += cos(Position.x*2)*cos(Position.z*2)*0.1;
  density += cos(Position.x*4)*cos(Position.z*4)*0.05;
  return density;
}