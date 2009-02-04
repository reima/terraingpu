Texture3D g_tNoise3D;

SamplerState ssTrilinearRepeat {
  AddressU = WRAP;
  AddressV = WRAP;
  AddressW = WRAP;
  Filter = MIN_MAG_MIP_LINEAR;
};

float Noise(float3 tex) {
  return g_tNoise3D.Sample(ssTrilinearRepeat, tex*0.1).x*2-1;
}

float DENSITY(float3 Position) {
  //float density = 1 - length(Position);
  //float density = -Position.y+0.5;
  float density = 0;
  float3 warp = float3(Noise(Position*0.004), Noise((Position+0.1)*0.004), Noise((Position+0.2)*0.004));
  Position += warp;
  density += Noise(Position*1.01)*1.00;
  density += Noise(Position*1.96)*0.50;
  density += Noise(Position*4.03)*0.25;
  density += Noise(Position*7.97)*0.125;
  density += Noise(Position*16.07)*0.0625;
  return density;
}